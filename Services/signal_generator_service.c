#include "signal_generator_service.h"
#include "pwm_output_driver.h"

typedef struct
{
  uint16_t prescaler;
  uint16_t arr;
  uint32_t actual_freq;
  uint8_t valid;
} SignalGenerator_TimerConfig_t;

static SignalGenerator_TimerConfig_t SignalGeneratorService_FindOptimal(uint32_t target_freq,
                                                                         uint32_t timer_clk)
{
  SignalGenerator_TimerConfig_t config = {0xFFFFU, 0xFFFFU, 0U, 0U};
  uint32_t min_error = 0xFFFFFFFFUL;
  uint32_t max_presc = timer_clk / SIGNAL_GENERATOR_FREQ_MIN_HZ;
  uint16_t presc;

  if (max_presc > 0xFFFFU) {
    max_presc = 0xFFFFU;
  }

  for (presc = 0U; presc <= max_presc; ++presc) {
    float temp_arr_f = (float)timer_clk / ((presc + 1U) * target_freq);
    uint32_t temp_arr = (uint32_t)(temp_arr_f + 0.5F);
    uint32_t actual_freq;
    uint32_t error;

    if ((temp_arr == 0U) || (temp_arr > 0xFFFFU)) {
      continue;
    }

    actual_freq = timer_clk / ((presc + 1U) * temp_arr);
    error = (actual_freq > target_freq)
              ? (actual_freq - target_freq)
              : (target_freq - actual_freq);

    if ((error < min_error) || ((error == min_error) && (temp_arr > config.arr))) {
      config.prescaler = presc;
      config.arr = (uint16_t)(temp_arr - 1U);
      config.actual_freq = actual_freq;
      config.valid = (error * 100U < target_freq) ? 1U : 0U;
      min_error = error;

      if (config.valid) {
        break;
      }
    }
  }

  return config;
}

void SignalGeneratorService_Init(uint32_t frequency_hz, uint8_t duty_percent)
{
  PwmOutputDriver_Init();
  SignalGeneratorService_Apply(frequency_hz, duty_percent);
}

void SignalGeneratorService_Apply(uint32_t frequency_hz, uint8_t duty_percent)
{
  SignalGenerator_TimerConfig_t config;
  uint32_t ticks;
  uint32_t compare;

  if (frequency_hz < SIGNAL_GENERATOR_FREQ_MIN_HZ) {
    frequency_hz = SIGNAL_GENERATOR_FREQ_MIN_HZ;
  } else if (frequency_hz > SIGNAL_GENERATOR_FREQ_MAX_HZ) {
    frequency_hz = SIGNAL_GENERATOR_FREQ_MAX_HZ;
  }

  if (duty_percent < SIGNAL_GENERATOR_DUTY_MIN_PERCENT) {
    duty_percent = SIGNAL_GENERATOR_DUTY_MIN_PERCENT;
  } else if (duty_percent > SIGNAL_GENERATOR_DUTY_MAX_PERCENT) {
    duty_percent = SIGNAL_GENERATOR_DUTY_MAX_PERCENT;
  }

  config = SignalGeneratorService_FindOptimal(frequency_hz, SIGNAL_GENERATOR_TIMER_CLOCK_HZ);
  if (!config.valid) {
    return;
  }

  ticks = (uint32_t)config.arr + 1U;
  compare = (uint32_t)(((uint64_t)ticks * duty_percent) / 100U);
  if (compare > config.arr) {
    compare = config.arr;
  }

  PwmOutputDriver_ApplyTimer(config.prescaler, config.arr, (uint16_t)compare);
}
