#include "measure.h"
#include "adc.h"
#include "gpio.h"
#include "menu.h"
#include <stdio.h>
#include "systick.h"
#include "transmit.h"
#include "pwm_input.h"

/* Measurement results */
float measured_duty = 0.0f;
float measured_resistor = 0.0f;
float measured_frequency = 0.0f;
float measured_capacitance = 0.0f;

/** @brief Measurement state flag: 1 when capacitor measurement completed */
uint8_t measure_cap_done = 0;
/** @brief Capacitor measurement state machine */
CapState cap_state = CAP_IDLE;

/** 
 * @brief RC time constant factor for capacitance calculation. 
 * Computed as: ln(Vth / (Vcc - Vth)) * Vcc
 */
const float RC_TIME_FACTOR = 1.74563473f;

/**
  * @brief  Measure resistance using voltage divider method.
  * @note   Reads ADC value from @ref ADC_INPUT_RES_CHANNEL.
  * @retval None. Updates @ref measured_resistor.
  */
void Measure_Resistor(void) {
  float adc_val = Adc_Read_Channel(ADC_INPUT_RES_CHANNEL);

  if (adc_val <= 1.0f) {
    measured_resistor = 0.00f;
    return;
  }

  float Vadc = (adc_val / ADC_RESOLUTION) * ADC_VCC;

  if (Vadc < MIN_VOLTAGE || (ADC_VCC - Vadc) < MIN_VOLTAGE) {
    measured_resistor = 0.00f;
    return;
  }

  measured_resistor = RESISTOR_1 * (Vadc / (ADC_VCC - Vadc));
}

/**
  * @brief  Initialize charge pin (push-pull output, low state).
  * @note   Used before starting capacitor measurement.
  * @retval None
  */
void Measure_Charge_Pin_Init(void) {
  Gpio_Init_Pin(GPIO_CHARGE_PORT, GPIO_CHARGE_PIN, GPIO_Mode_Out_PP, GPIO_Speed_50MHz);
  GPIO_ResetBits(GPIO_CHARGE_PORT, GPIO_CHARGE_PIN);
#ifdef MEASURE_DEBUG
  printf("Charge Pin Enable!\r\n");
#endif
}

/**
  * @brief  Deinitialize charge pin (floating input).
  * @note   Puts capacitor node back to high impedance.
  * @retval None
  */
void Measure_Charge_Pin_DeInit(void) {
  Gpio_Init_Pin(GPIO_CHARGE_PORT, GPIO_CHARGE_PIN, GPIO_Mode_IN_FLOATING, GPIO_Speed_50MHz);
#ifdef MEASURE_DEBUG
  printf("Charge Pin Disable!\r\n");
#endif
}

/**
  * @brief  Start capacitor measurement sequence.
  * @retval None
  */
void Measure_Capacitor_Start(void) {
  cap_state = CAP_WAIT;
}

/**
  * @brief  Execute capacitor measurement state machine.
  * @note   Uses charge/discharge cycles and SysTick timestamps
  *         to estimate capacitance based on RC time.
  * @retval None. Updates @ref measured_capacitance and @ref measure_cap_done.
  */
void Measure_Capacitor(void) {
  float adc_val = Adc_Read_Channel(ADC_INPUT_CAP_CHANNEL);
  float measure_cap_time_s = 0;
  static uint32_t measure_cap_start_time = 0, measure_cap_stop_time = 0;

  switch (cap_state) {
    case CAP_WAIT:
      {
        Menu_Print_String(0, 1, "Waiting...");

        GPIO_SetBits(GPIO_CHARGE_PORT, GPIO_CHARGE_PIN);

        if (adc_val > THRESH_HIGH_VOTL) /*Luc dau tu co the chua dat muc 2V*/
        {
          cap_state = CAP_DISCHARGING;
        }
        break;
      }
    case CAP_DISCHARGING:
      {
        GPIO_ResetBits(GPIO_CHARGE_PORT, GPIO_CHARGE_PIN);

        if (adc_val < THRESH_LOW_VOLT) /*Xa den khi tu dat muc 1V*/
        {
          cap_state = CAP_CHARGING;
          GPIO_SetBits(GPIO_CHARGE_PORT, GPIO_CHARGE_PIN); /*Pull high*/
          measure_cap_start_time = SysTick_Get_Tick();
        }
        break;
      }
    case CAP_CHARGING:
      {
        if (adc_val > THRESH_HIGH_VOTL) /*Sac den khi tu dat muc 2V*/
        {
          measure_cap_stop_time = SysTick_Get_Tick();
          cap_state = CAP_DONE;

          measure_cap_time_s = (measure_cap_stop_time - measure_cap_start_time) / 1000.0f;
#ifdef MEASURE_DEBUG
          printf("%u - %u - %.4f\r\n", measure_cap_start_time, measure_cap_stop_time, measure_cap_time_s);
#endif
          measured_capacitance = RC_TIME_FACTOR * measure_cap_time_s / RESISTOR_2;
#ifdef MEASURE_DEBUG
          printf("%.4f\r\n", measured_capacitance);
#endif
        }
        break;
      }
    case CAP_DONE:
      {
        measure_cap_done = 1;
        cap_state = CAP_IDLE;
      }
    default:
      {
        cap_state = CAP_IDLE;
        break;
      }
  }
}

/**
  * @brief  Compute frequency and duty cycle from PWM capture data.
  * @param  pwm_x Pointer to @ref Pwm_Input_Capture struct
  * @param  freq  Pointer to store measured frequency (Hz)
  * @param  duty  Pointer to store measured duty cycle (%)
  * @note   If no signal is detected within @ref PWM_TIMEOUT, freq and duty are reset to 0.
  * @retval None
  */
static void Measure_Freq_Duty(Pwm_Input_Capture* pwm_x, float* freq, float* duty) {
  uint32_t now = SysTick_Get_Tick();

  if (pwm_x->ready_flag) {
    pwm_x->ready_flag = 0;

    if (pwm_x->period > 0 && pwm_x->high_time <= pwm_x->period) {
      *freq = (float)pwm_x->timer_clock_hz / (float)(pwm_x->period);
      *duty = ((float)(pwm_x->high_time) / (float)(pwm_x->period)) * 100.0f;
    } else {
      *freq = 0.0f;
      *duty = 0.0f;
    }
  } else if ((now - pwm_x->last_capture_time_ms) > PWM_TIMEOUT) {
    *freq = 0.0f;
    *duty = 0.0f;
  }
}

/**
  * @brief  Adaptive measurement of frequency and duty cycle.
  * @note   Selects PWM input channel depending on @ref transmited_frequency:
  *         - Low frequency  ? PWM_CHANNEL_4
  *         - Mid frequency  ? PWM_CHANNEL_3
  *         - High frequency ? PWM_CHANNEL_1
  * @retval None. Updates @ref measured_frequency and @ref measured_duty.
  */
void Measure_Freq_Duty_Apdative(void) {
  if (transmited_frequency < THRESH_LOW_FREQ) {
    Measure_Freq_Duty(&pwm_inputs[PWM_CHANNEL_4], &measured_frequency, &measured_duty);

  } else if (transmited_frequency < THRESH_MID_FREQ) {
    Measure_Freq_Duty(&pwm_inputs[PWM_CHANNEL_3], &measured_frequency, &measured_duty);
  } else {
    Measure_Freq_Duty(&pwm_inputs[PWM_CHANNEL_1], &measured_frequency, &measured_duty);
  }
}
