#include "transmit.h"

uint16_t transmited_duty = 0;
uint32_t transmited_frequency = 0;

/**
  * @brief  Configuration result for timer-based PWM generation.
  */
typedef struct {
  uint16_t prescaler;     /*!< Timer prescaler value */
  uint16_t arr;           /*!< Auto-reload register (period) */
  uint32_t actual_freq;   /*!< Calculated PWM frequency */
  uint8_t valid;          /*!< 1 if within error tolerance, otherwise 0 */
} TimerConfig;

/**
  * @brief  Find optimal prescaler/ARR combination for target PWM frequency.
  * @param  target_freq Desired PWM frequency in Hz.
  * @param  timer_clk   Timer input clock frequency in Hz.
  * @retval TimerConfig Best configuration found (valid=1 if within <1% error).
  */
TimerConfig FindOptimalPWM(uint32_t target_freq, uint32_t timer_clk) {
  TimerConfig config = {
    0xFFFF,
    0xFFFF,
    0,
    0
  };
  uint32_t min_error = 0xFFFFFFFF;
  uint32_t max_presc = timer_clk / FREQ_MIN;
  if (max_presc > 0xFFFF) {
    max_presc = 0xFFFF;
  }

  for (uint16_t presc = 0; presc <= max_presc; ++presc) {
    float temp_arr_f = (float)timer_clk / ((presc + 1) * target_freq);
    uint32_t temp_arr = (uint32_t)(temp_arr_f + 0.5f);

    if (temp_arr == 0 || temp_arr > 0xFFFF) {
      continue;
    }

    uint32_t actual_freq = timer_clk / ((presc + 1) * temp_arr);

    uint32_t error;
    if (actual_freq > target_freq) {
      error = actual_freq - target_freq;
    } else {
      error = target_freq - actual_freq;
    }

    if (error < min_error || (error == min_error && temp_arr > config.arr)) {
      config.prescaler = presc;
      config.arr = temp_arr - 1;
      config.actual_freq = actual_freq;

      if (error * 100 < target_freq) {
        config.valid = 1;
      } else {
        config.valid = 0;
      }
      min_error = error;

      if (config.valid) {
        break;
      }
    }
  }

  return config;
}

/**
  * @brief  Transmit PWM with target frequency and duty cycle (TIM2 CH2).
  * @param  frequency     Desired PWM frequency in Hz. Clamped to [@ref FREQ_MIN, @ref FREQ_MAX].
  * @param  duty_percent  Desired duty cycle in percent. Clamped to [@ref DUTY_MIN, @ref DUTY_MAX].
  * @note   Uses FindOptimalPWM() to select prescaler/ARR with <1% frequency error when possible.
  * @note   Assumes TIMER input clock equals @ref SYSTEM_CLOCK. If your TIM2 clock is prescaled,
  *         pass the correct timer clock to FindOptimalPWM() or adjust @ref SYSTEM_CLOCK accordingly.
  * @pre    TIM2 CH2 must be configured in PWM mode, GPIO pin is mapped (AF), and TIM2 clock enabled.
  * @post   Updates TIM2 PSC, ARR and CCR2 registers.
  * @retval None
  */
void Transmit_Freq_Duty(uint32_t frequency, uint8_t duty_percent) {
	/* Clamp frequency and duty cycle */
  if (frequency < FREQ_MIN) {
    frequency = FREQ_MIN;
  }

  if (frequency > FREQ_MAX) {
    frequency = FREQ_MAX;
  }

  if (duty_percent < DUTY_MIN) {
    duty_percent = DUTY_MIN;
  }

  if (duty_percent > DUTY_MAX) {
    duty_percent = DUTY_MAX;
  }
	
	/* Compute optimal prescaler/ARR for the requested frequency */
  uint32_t timer_clk = SYSTEM_CLOCK;
  TimerConfig config = FindOptimalPWM(frequency, timer_clk);
	
  if (!config.valid) {
    return;
  }
	
	/* Apply timer settings */
  TIM_PrescalerConfig(TIM2, config.prescaler, TIM_PSCReloadMode_Immediate);
  TIM_SetAutoreload(TIM2, config.arr);

	/* Compute CCR from duty%: CCR = (ARR + 1) * duty% / 100
	 Use 64-bit intermediate to avoid overflow on large ARR */
  uint32_t ticks = config.arr + 1;
  uint32_t ccr = ((uint64_t)ticks * duty_percent) / 100;
	
	/* Guard: ensure CCR <= ARR */
  if (ccr > config.arr) {
    ccr = config.arr;
  }
	
  TIM_SetCompare2(TIM2, ccr);
}

