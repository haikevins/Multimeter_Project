#include "measurement_service.h"
#include "adc_driver.h"
#include "capacitor_charge_driver.h"
#include "pwm_capture_driver.h"
#include "pwm_output_driver.h"
#include "system_time.h"
#include "debug_logger.h"
#include "error_manager.h"

#define THRESH_LOW_VOLT 1241.0F
#define THRESH_HIGH_VOLT 2482.0F

#define MIN_VOLTAGE 0.01F
#define RESISTOR_1 10000.0F
#define RESISTOR_2 20000.0F
#define PWM_TIMEOUT_MS 1100U
#define ADC_RESULT_TIMEOUT_MS 100U
#define RC_TIME_FACTOR 1.74563473F

/* Open-circuit detection for the resistor divider.
 * 1 Mohm is about 4055 ADC counts with a 10 kohm reference resistor, while
 * an open input tends to full scale. 4075 therefore preserves the documented
 * 1 Mohm range but treats roughly >2 Mohm/open as no component. Hysteresis
 * prevents status chatter near the boundary. */
#define RESISTOR_OPEN_ADC_THRESHOLD 4075.0F
#define RESISTOR_PRESENT_ADC_THRESHOLD 4060.0F
#define RESISTOR_OPEN_CONFIRM_COUNT 3U
#define RESISTOR_PRESENT_CONFIRM_COUNT 5U
#define RESISTOR_PRESENT_CONFIRM_MS 200U

/* Resistor-only stability filter. The ADC driver already averages 16 samples
 * per batch and applies a light low-pass filter. The service adds a robust
 * median + adaptive EMA + sticky output stage so a fixed resistor produces a
 * visually steady value without making a real resistor change feel sluggish. */
#define RESISTOR_MEDIAN_WINDOW              5U
#define RESISTOR_EMA_ALPHA_STABLE           0.05F
#define RESISTOR_EMA_ALPHA_FAST             0.30F
#define RESISTOR_FAST_CHANGE_REL            0.05F
#define RESISTOR_OUTPUT_DEADBAND_REL         0.005F
#define RESISTOR_OUTPUT_DEADBAND_ABS         1.0F
#define RESISTOR_OUTPUT_CONFIRM_MS           300U

/* Capacitor detection: the documented range starts at 100 nF. During the
 * timed charge from ~1 V to ~2 V, a real >=100 nF capacitor cannot jump over
 * the high threshold in the very first ADC batch. An open socket can. */
#define CAPACITOR_DETECT_FIRST_BATCH_COUNT 1U

typedef enum
{
  CAP_IDLE = 0,
  CAP_WAIT,
  CAP_DISCHARGING,
  CAP_CHARGING,
  CAP_DONE
} Capacitor_State_t;

static float measured_duty = 0.0F;
static float measured_resistor = 0.0F;
static float measured_frequency = 0.0F;
static float measured_capacitance = 0.0F;
static uint8_t capacitor_done = 0U;
static Capacitor_State_t capacitor_state = CAP_IDLE;
static Measurement_Mode_t active_mode = MEASUREMENT_MODE_NONE;
static Measurement_Status_t measurement_status = MEASUREMENT_STATUS_IDLE;
static uint8_t adc_request_pending = 0U;
static uint32_t adc_request_started_ms = 0U;
typedef enum
{
  RESISTOR_PRESENCE_UNKNOWN = 0,
  RESISTOR_PRESENCE_PRESENT,
  RESISTOR_PRESENCE_ABSENT
} Resistor_Presence_t;

static uint8_t resistor_open_count = 0U;
static uint8_t resistor_present_count = 0U;
static uint32_t resistor_present_candidate_ms = 0U;
static Resistor_Presence_t resistor_presence = RESISTOR_PRESENCE_UNKNOWN;

/* Resistor filter state. These values belong to MeasurementService because the
 * filtering policy is measurement-specific, not a generic ADC-driver concern. */
static float resistor_adc_window[RESISTOR_MEDIAN_WINDOW] = { 0.0F };
static uint8_t resistor_adc_window_count = 0U;
static uint8_t resistor_adc_window_index = 0U;
static float resistor_ema_value = 0.0F;
static uint8_t resistor_ema_initialized = 0U;
static float resistor_stable_value = 0.0F;
static uint8_t resistor_stable_initialized = 0U;
static uint8_t resistor_output_candidate_active = 0U;
static int8_t resistor_output_candidate_direction = 0;
static uint32_t resistor_output_candidate_since_ms = 0U;

static uint8_t capacitor_charge_batch_count = 0U;

static float MeasurementService_AbsFloat(float value)
{
  return (value < 0.0F) ? -value : value;
}

static void MeasurementService_ResetResistorFilter(void)
{
  uint8_t i;

  for (i = 0U; i < RESISTOR_MEDIAN_WINDOW; ++i) {
    resistor_adc_window[i] = 0.0F;
  }

  resistor_adc_window_count = 0U;
  resistor_adc_window_index = 0U;
  resistor_ema_value = 0.0F;
  resistor_ema_initialized = 0U;
  resistor_stable_value = 0.0F;
  resistor_stable_initialized = 0U;
  resistor_output_candidate_active = 0U;
  resistor_output_candidate_direction = 0;
  resistor_output_candidate_since_ms = 0U;
}

static float MeasurementService_ResistorMedianAdc(float adc_value)
{
  float sorted[RESISTOR_MEDIAN_WINDOW];
  uint8_t count;
  uint8_t i;
  uint8_t j;

  resistor_adc_window[resistor_adc_window_index] = adc_value;
  resistor_adc_window_index = (uint8_t)((resistor_adc_window_index + 1U) % RESISTOR_MEDIAN_WINDOW);
  if (resistor_adc_window_count < RESISTOR_MEDIAN_WINDOW) {
    resistor_adc_window_count++;
  }

  count = resistor_adc_window_count;
  for (i = 0U; i < count; ++i) {
    sorted[i] = resistor_adc_window[i];
  }

  /* Tiny fixed-size insertion sort. Runs in main context, never in the ADC ISR. */
  for (i = 1U; i < count; ++i) {
    float key = sorted[i];
    j = i;
    while ((j > 0U) && (sorted[j - 1U] > key)) {
      sorted[j] = sorted[j - 1U];
      j--;
    }
    sorted[j] = key;
  }

  return sorted[count / 2U];
}

static float MeasurementService_FilterResistor(float raw_resistance, uint32_t now_ms)
{
  float relative_change;
  float alpha;
  float delta;
  float deadband;
  int8_t direction;

  if (!resistor_ema_initialized) {
    resistor_ema_value = raw_resistance;
    resistor_ema_initialized = 1U;
  } else {
    float denominator = MeasurementService_AbsFloat(resistor_ema_value);
    if (denominator < 1.0F) {
      denominator = 1.0F;
    }

    relative_change = MeasurementService_AbsFloat(raw_resistance - resistor_ema_value) / denominator;
    alpha = (relative_change >= RESISTOR_FAST_CHANGE_REL)
              ? RESISTOR_EMA_ALPHA_FAST
              : RESISTOR_EMA_ALPHA_STABLE;
    resistor_ema_value += alpha * (raw_resistance - resistor_ema_value);
  }

  if (!resistor_stable_initialized) {
    resistor_stable_value = resistor_ema_value;
    resistor_stable_initialized = 1U;
    resistor_output_candidate_active = 0U;
    resistor_output_candidate_direction = 0;
    return resistor_stable_value;
  }

  deadband = MeasurementService_AbsFloat(resistor_stable_value) * RESISTOR_OUTPUT_DEADBAND_REL;
  if (deadband < RESISTOR_OUTPUT_DEADBAND_ABS) {
    deadband = RESISTOR_OUTPUT_DEADBAND_ABS;
  }

  delta = MeasurementService_AbsFloat(resistor_ema_value - resistor_stable_value);
  if (delta <= deadband) {
    /* Stay exactly on the previous published value. Small ADC/thermal noise is
     * absorbed here instead of slowly walking the number on the LCD. */
    resistor_output_candidate_active = 0U;
    resistor_output_candidate_direction = 0;
    resistor_output_candidate_since_ms = 0U;
    return resistor_stable_value;
  }

  direction = (resistor_ema_value > resistor_stable_value) ? 1 : -1;
  if (!resistor_output_candidate_active ||
      (direction != resistor_output_candidate_direction)) {
    resistor_output_candidate_active = 1U;
    resistor_output_candidate_direction = direction;
    resistor_output_candidate_since_ms = now_ms;
    return resistor_stable_value;
  }

  if ((uint32_t)(now_ms - resistor_output_candidate_since_ms) < RESISTOR_OUTPUT_CONFIRM_MS) {
    return resistor_stable_value;
  }

  /* A change that survives outside the deadband for 300 ms is considered real. */
  resistor_stable_value = resistor_ema_value;
  resistor_output_candidate_active = 0U;
  resistor_output_candidate_direction = 0;
  resistor_output_candidate_since_ms = 0U;
  return resistor_stable_value;
}

static void MeasurementService_ResetAdcWatchdog(void)
{
  adc_request_pending = 0U;
  adc_request_started_ms = 0U;
}

static void MeasurementService_RequestAdc(AdcDriver_Input_t input)
{
  if (adc_request_pending || (measurement_status == MEASUREMENT_STATUS_ERROR)) {
    return;
  }

  if (AdcDriver_Request(input)) {
    adc_request_pending = 1U;
    adc_request_started_ms = SystemTime_GetTick();
  }
}

static uint8_t MeasurementService_GetAdcResult(AdcDriver_Input_t input, float* value)
{
  uint32_t now;

  if (AdcDriver_GetResult(input, value)) {
    MeasurementService_ResetAdcWatchdog();
    return 1U;
  }

  if (!adc_request_pending) {
    return 0U;
  }

  now = SystemTime_GetTick();
  if ((uint32_t)(now - adc_request_started_ms) > ADC_RESULT_TIMEOUT_MS) {
    AdcDriver_Cancel();
    MeasurementService_ResetAdcWatchdog();
    measurement_status = MEASUREMENT_STATUS_ERROR;
    ErrorManager_Report(ERROR_SOURCE_ADC,
                        ERROR_CODE_ADC_CONVERSION_TIMEOUT,
                        ERROR_SEVERITY_ERROR);
  }

  return 0U;
}

static void MeasurementService_ProcessResistor(void)
{
  float adc_val;

  if (measurement_status == MEASUREMENT_STATUS_ERROR) {
    return;
  }

  if (MeasurementService_GetAdcResult(ADC_DRIVER_INPUT_RESISTOR, &adc_val)) {
    float vadc;
    uint32_t now = SystemTime_GetTick();

    /*
     * Presence detection is intentionally stateful.
     *
     * UNKNOWN:
     *   Do not display a resistance until the input has been confirmed.
     * PRESENT:
     *   A few high/open samples are ignored so removing a resistor cannot
     *   cause READY/NO_COMPONENT chatter.
     * ABSENT:
     *   NO_COMPONENT is latched. It may only leave this state after the ADC
     *   stays continuously inside the valid-present region for 200 ms.
     *
     * This asymmetric debounce is important because an open divider input is
     * very close to full scale and occasional ADC noise can otherwise look
     * like a very large resistor for one or two batches.
     */
    if (resistor_presence == RESISTOR_PRESENCE_UNKNOWN) {
      if (adc_val >= RESISTOR_OPEN_ADC_THRESHOLD) {
        resistor_present_count = 0U;
        resistor_present_candidate_ms = 0U;

        if (resistor_open_count < RESISTOR_OPEN_CONFIRM_COUNT) {
          resistor_open_count++;
        }

        if (resistor_open_count >= RESISTOR_OPEN_CONFIRM_COUNT) {
          resistor_presence = RESISTOR_PRESENCE_ABSENT;
          measured_resistor = 0.0F;
          MeasurementService_ResetResistorFilter();
          measurement_status = MEASUREMENT_STATUS_NO_COMPONENT;
        } else {
          measurement_status = MEASUREMENT_STATUS_MEASURING;
        }
      } else if (adc_val <= RESISTOR_PRESENT_ADC_THRESHOLD) {
        resistor_open_count = 0U;

        if (resistor_present_count < RESISTOR_OPEN_CONFIRM_COUNT) {
          resistor_present_count++;
        }

        if (resistor_present_count >= RESISTOR_OPEN_CONFIRM_COUNT) {
          resistor_presence = RESISTOR_PRESENCE_PRESENT;
          resistor_present_count = 0U;
        } else {
          measurement_status = MEASUREMENT_STATUS_MEASURING;
          MeasurementService_RequestAdc(ADC_DRIVER_INPUT_RESISTOR);
          return;
        }
      } else {
        /* Hysteresis band: neither prove present nor absent yet. */
        resistor_open_count = 0U;
        resistor_present_count = 0U;
        measurement_status = MEASUREMENT_STATUS_MEASURING;
        MeasurementService_RequestAdc(ADC_DRIVER_INPUT_RESISTOR);
        return;
      }
    } else if (resistor_presence == RESISTOR_PRESENCE_PRESENT) {
      if (adc_val >= RESISTOR_OPEN_ADC_THRESHOLD) {
        if (resistor_open_count < RESISTOR_OPEN_CONFIRM_COUNT) {
          resistor_open_count++;
        }

        if (resistor_open_count >= RESISTOR_OPEN_CONFIRM_COUNT) {
          resistor_presence = RESISTOR_PRESENCE_ABSENT;
          resistor_present_count = 0U;
          resistor_present_candidate_ms = 0U;
          measured_resistor = 0.0F;
          MeasurementService_ResetResistorFilter();
          measurement_status = MEASUREMENT_STATUS_NO_COMPONENT;
        }

        /* Until absence is confirmed, keep the last valid READY value. */
        MeasurementService_RequestAdc(ADC_DRIVER_INPUT_RESISTOR);
        return;
      }

      resistor_open_count = 0U;
    } else { /* RESISTOR_PRESENCE_ABSENT */
      measurement_status = MEASUREMENT_STATUS_NO_COMPONENT;
      measured_resistor = 0.0F;

      if (adc_val <= RESISTOR_PRESENT_ADC_THRESHOLD) {
        if (resistor_present_count == 0U) {
          resistor_present_candidate_ms = now;
        }

        if (resistor_present_count < 0xFFU) {
          resistor_present_count++;
        }

        if ((resistor_present_count >= RESISTOR_PRESENT_CONFIRM_COUNT) &&
            ((uint32_t)(now - resistor_present_candidate_ms) >= RESISTOR_PRESENT_CONFIRM_MS)) {
          resistor_presence = RESISTOR_PRESENCE_PRESENT;
          resistor_present_count = 0U;
          resistor_open_count = 0U;
          resistor_present_candidate_ms = 0U;
          MeasurementService_ResetResistorFilter();
        } else {
          /* Stay latched as NO_COMPONENT during insertion qualification. */
          MeasurementService_RequestAdc(ADC_DRIVER_INPUT_RESISTOR);
          return;
        }
      } else {
        /* Any sample back near open/full-scale restarts the full 200 ms test. */
        resistor_present_count = 0U;
        resistor_present_candidate_ms = 0U;
        MeasurementService_RequestAdc(ADC_DRIVER_INPUT_RESISTOR);
        return;
      }
    }

    if (resistor_presence == RESISTOR_PRESENCE_PRESENT) {
      float median_adc = MeasurementService_ResistorMedianAdc(adc_val);
      float raw_resistance;

      vadc = (median_adc / ADC_RESOLUTION) * ADC_VCC;

      if (vadc < MIN_VOLTAGE) {
        raw_resistance = 0.0F;
        measured_resistor = MeasurementService_FilterResistor(raw_resistance, now);
        measurement_status = MEASUREMENT_STATUS_READY;
      } else if ((ADC_VCC - vadc) < MIN_VOLTAGE) {
        /* Defensive fallback: a present state must never publish a near-VCC
         * garbage value. Re-latch it as no component instead. */
        resistor_presence = RESISTOR_PRESENCE_ABSENT;
        resistor_present_count = 0U;
        resistor_present_candidate_ms = 0U;
        measured_resistor = 0.0F;
        MeasurementService_ResetResistorFilter();
        measurement_status = MEASUREMENT_STATUS_NO_COMPONENT;
      } else {
        raw_resistance = RESISTOR_1 * (vadc / (ADC_VCC - vadc));
        measured_resistor = MeasurementService_FilterResistor(raw_resistance, now);
        measurement_status = MEASUREMENT_STATUS_READY;
      }
    }
  }

  if (measurement_status != MEASUREMENT_STATUS_ERROR) {
    MeasurementService_RequestAdc(ADC_DRIVER_INPUT_RESISTOR);
  }
}

static void MeasurementService_EnterCapacitor(void)
{
  CapacitorChargeDriver_Enter();
  capacitor_state = CAP_IDLE;
  capacitor_done = 0U;
  capacitor_charge_batch_count = 0U;
  AdcDriver_Cancel();
  MeasurementService_ResetAdcWatchdog();
  measurement_status = MEASUREMENT_STATUS_WAIT_CHARGE;
  DebugLogger_Log(DEBUG_LEVEL_TRACE, "MEASURE", "Capacitor enter");
}

static void MeasurementService_ExitCapacitor(void)
{
  capacitor_state = CAP_IDLE;
  capacitor_done = 0U;
  capacitor_charge_batch_count = 0U;
  CapacitorChargeDriver_SetCharging(0U);
  AdcDriver_Cancel();
  MeasurementService_ResetAdcWatchdog();
  CapacitorChargeDriver_Exit();
  DebugLogger_Log(DEBUG_LEVEL_TRACE, "MEASURE", "Capacitor exit");
}

static void MeasurementService_ProcessCapacitor(void)
{
  float adc_val;
  float measure_cap_time_s;
  static uint32_t measure_cap_start_time = 0U;
  static uint32_t measure_cap_stop_time = 0U;

  if ((capacitor_state == CAP_IDLE) ||
      (measurement_status == MEASUREMENT_STATUS_ERROR)) {
    return;
  }

  if (capacitor_state == CAP_DONE) {
    CapacitorChargeDriver_SetCharging(0U);
    capacitor_done = 1U;
    capacitor_state = CAP_IDLE;
    AdcDriver_Cancel();
    MeasurementService_ResetAdcWatchdog();
    measurement_status = MEASUREMENT_STATUS_READY;
    return;
  }

  if (!MeasurementService_GetAdcResult(ADC_DRIVER_INPUT_CAPACITOR, &adc_val)) {
    if (measurement_status != MEASUREMENT_STATUS_ERROR) {
      MeasurementService_RequestAdc(ADC_DRIVER_INPUT_CAPACITOR);
    }
    return;
  }

  switch (capacitor_state) {
    case CAP_WAIT:
      CapacitorChargeDriver_SetCharging(1U);
      measurement_status = MEASUREMENT_STATUS_CHARGING;
      if (adc_val > THRESH_HIGH_VOLT) {
        CapacitorChargeDriver_SetCharging(0U);
        capacitor_state = CAP_DISCHARGING;
        measurement_status = MEASUREMENT_STATUS_DISCHARGING;
      }
      break;

    case CAP_DISCHARGING:
      CapacitorChargeDriver_SetCharging(0U);
      measurement_status = MEASUREMENT_STATUS_DISCHARGING;
      if (adc_val < THRESH_LOW_VOLT) {
        CapacitorChargeDriver_SetCharging(1U);
        measure_cap_start_time = SystemTime_GetTick();
        capacitor_charge_batch_count = 0U;
        AdcDriver_ResetFilter(ADC_DRIVER_INPUT_CAPACITOR);
        capacitor_state = CAP_CHARGING;
        measurement_status = MEASUREMENT_STATUS_CHARGING;
      }
      break;

    case CAP_CHARGING:
      measurement_status = MEASUREMENT_STATUS_CHARGING;
      capacitor_charge_batch_count++;

      if (adc_val > THRESH_HIGH_VOLT) {
        /* If the node jumped from the low threshold to the high threshold in
         * the first ADC batch, no useful RC time constant exists. For the
         * documented >=100 nF range this indicates an empty/open socket (or a
         * capacitor below the supported range), not a valid capacitance. */
        if (capacitor_charge_batch_count <= CAPACITOR_DETECT_FIRST_BATCH_COUNT) {
          measured_capacitance = 0.0F;
          CapacitorChargeDriver_SetCharging(0U);
          capacitor_state = CAP_IDLE;
          capacitor_done = 0U;
          measurement_status = MEASUREMENT_STATUS_NO_COMPONENT;
          AdcDriver_Cancel();
          MeasurementService_ResetAdcWatchdog();
          break;
        }

        measure_cap_stop_time = SystemTime_GetTick();
        measure_cap_time_s = (measure_cap_stop_time - measure_cap_start_time) / 1000.0F;
        measured_capacitance = RC_TIME_FACTOR * measure_cap_time_s / RESISTOR_2;
        CapacitorChargeDriver_SetCharging(0U);
        capacitor_state = CAP_DONE;

        DebugLogger_Log(DEBUG_LEVEL_TRACE, "MEASURE",
                        "Cap t=%lums C=%.4fF",
                        (unsigned long)(measure_cap_stop_time - measure_cap_start_time),
                        measured_capacitance);
      }
      break;

    case CAP_IDLE:
    case CAP_DONE:
    default:
      break;
  }

  if ((capacitor_state != CAP_DONE) && (capacitor_state != CAP_IDLE)) {
    MeasurementService_RequestAdc(ADC_DRIVER_INPUT_CAPACITOR);
  }
}

static void MeasurementService_ProcessFrequencyDuty(void)
{
  PwmCaptureSample_t sample;
  uint32_t now = SystemTime_GetTick();

  if (measurement_status == MEASUREMENT_STATUS_ERROR) {
    return;
  }

  if (!PwmCaptureDriver_Read(&sample)) {
    measurement_status = MEASUREMENT_STATUS_ERROR;
    ErrorManager_Report(ERROR_SOURCE_PWM_INPUT,
                        ERROR_CODE_PWM_CAPTURE_UNAVAILABLE,
                        ERROR_SEVERITY_ERROR);
    return;
  }

  if (sample.fresh) {
    if ((sample.period_ticks > 0U) && (sample.high_ticks <= sample.period_ticks)) {
      measured_frequency = (float)sample.timer_clock_hz / (float)sample.period_ticks;
      measured_duty = ((float)sample.high_ticks / (float)sample.period_ticks) * 100.0F;
      measurement_status = MEASUREMENT_STATUS_READY;
    } else {
      measured_frequency = 0.0F;
      measured_duty = 0.0F;
      measurement_status = MEASUREMENT_STATUS_ERROR;
      ErrorManager_Report(ERROR_SOURCE_PWM_INPUT,
                          ERROR_CODE_PWM_INVALID_CAPTURE,
                          ERROR_SEVERITY_ERROR);
    }
  } else if ((uint32_t)(now - sample.last_capture_time_ms) > PWM_TIMEOUT_MS) {
    measured_frequency = 0.0F;
    measured_duty = 0.0F;
    measurement_status = MEASUREMENT_STATUS_NO_SIGNAL;
  }
}

void MeasurementService_Init(void)
{
  active_mode = MEASUREMENT_MODE_NONE;
  capacitor_state = CAP_IDLE;
  capacitor_done = 0U;
  resistor_open_count = 0U;
  resistor_present_count = 0U;
  resistor_present_candidate_ms = 0U;
  resistor_presence = RESISTOR_PRESENCE_UNKNOWN;
  MeasurementService_ResetResistorFilter();
  capacitor_charge_batch_count = 0U;
  measurement_status = MEASUREMENT_STATUS_IDLE;
  MeasurementService_ResetAdcWatchdog();
  AdcDriver_Init();
  PwmCaptureDriver_Init();
}

void MeasurementService_SetMode(Measurement_Mode_t mode, uint32_t expected_frequency_hz)
{
  Measurement_Mode_t previous_mode = active_mode;

  if (previous_mode == mode) {
    return;
  }

  /* Exit current hardware ownership first. Shared resources are preserved for
     the same transitions used by the original All Measurement flow. */
  switch (previous_mode) {
    case MEASUREMENT_MODE_RESISTOR:
      if (mode == MEASUREMENT_MODE_CAPACITOR) {
        AdcDriver_Cancel();
      } else {
        AdcDriver_Disable();
      }
      break;

    case MEASUREMENT_MODE_CAPACITOR:
      MeasurementService_ExitCapacitor();
      AdcDriver_Disable();
      break;

    case MEASUREMENT_MODE_FREQUENCY_DUTY:
      PwmOutputDriver_Disable();
      PwmCaptureDriver_Disable();
      break;

    case MEASUREMENT_MODE_NONE:
    default:
      break;
  }

  active_mode = mode;

  switch (active_mode) {
    case MEASUREMENT_MODE_RESISTOR:
      measured_resistor = 0.0F;
      resistor_open_count = 0U;
      resistor_present_count = 0U;
      resistor_present_candidate_ms = 0U;
      resistor_presence = RESISTOR_PRESENCE_UNKNOWN;
      MeasurementService_ResetResistorFilter();
      MeasurementService_ResetAdcWatchdog();
      measurement_status = MEASUREMENT_STATUS_MEASURING;
      AdcDriver_Enable();
      AdcDriver_ResetFilter(ADC_DRIVER_INPUT_RESISTOR);
      break;

    case MEASUREMENT_MODE_CAPACITOR:
      if (previous_mode != MEASUREMENT_MODE_RESISTOR) {
        AdcDriver_Enable();
      }
      MeasurementService_EnterCapacitor();
      break;

    case MEASUREMENT_MODE_FREQUENCY_DUTY:
      measured_frequency = 0.0F;
      measured_duty = 0.0F;
      measurement_status = MEASUREMENT_STATUS_MEASURING;
      PwmOutputDriver_Enable();
      PwmCaptureDriver_Enable(expected_frequency_hz);
      break;

    case MEASUREMENT_MODE_NONE:
      MeasurementService_ResetAdcWatchdog();
      measurement_status = MEASUREMENT_STATUS_IDLE;
      break;

    default:
      measurement_status = MEASUREMENT_STATUS_ERROR;
      break;
  }
}

void MeasurementService_Process(void)
{
  switch (active_mode) {
    case MEASUREMENT_MODE_RESISTOR:
      MeasurementService_ProcessResistor();
      break;

    case MEASUREMENT_MODE_CAPACITOR:
      MeasurementService_ProcessCapacitor();
      break;

    case MEASUREMENT_MODE_FREQUENCY_DUTY:
      MeasurementService_ProcessFrequencyDuty();
      break;

    case MEASUREMENT_MODE_NONE:
    default:
      break;
  }
}

void MeasurementService_StartCapacitor(void)
{
  if (active_mode != MEASUREMENT_MODE_CAPACITOR) {
    return;
  }

  capacitor_done = 0U;
  capacitor_charge_batch_count = 0U;
  AdcDriver_Cancel();
  AdcDriver_ResetFilter(ADC_DRIVER_INPUT_CAPACITOR);
  MeasurementService_ResetAdcWatchdog();
  CapacitorChargeDriver_SetCharging(1U);
  capacitor_state = CAP_WAIT;
  measurement_status = MEASUREMENT_STATUS_CHARGING;
}

uint8_t MeasurementService_TakeCapacitorDone(void)
{
  uint8_t done = capacitor_done;
  capacitor_done = 0U;
  return done;
}

float MeasurementService_GetResult(Measurement_Result_t result)
{
  switch (result) {
    case MEASUREMENT_RESULT_RESISTOR:    return measured_resistor;
    case MEASUREMENT_RESULT_CAPACITANCE: return measured_capacitance;
    case MEASUREMENT_RESULT_FREQUENCY:   return measured_frequency;
    case MEASUREMENT_RESULT_DUTY:        return measured_duty;
    default:                             return 0.0F;
  }
}

Measurement_Status_t MeasurementService_GetStatus(void)
{
  return measurement_status;
}
