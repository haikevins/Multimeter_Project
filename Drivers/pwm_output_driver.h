/**
  ******************************************************************************
  * @file    pwm_output_driver.h
  * @brief   TIM2 CH2 PWM output hardware driver.
  ******************************************************************************
  */
#ifndef PWM_OUTPUT_DRIVER_H
#define PWM_OUTPUT_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void PwmOutputDriver_Init(void);
void PwmOutputDriver_Enable(void);
void PwmOutputDriver_Disable(void);
void PwmOutputDriver_ApplyTimer(uint16_t prescaler, uint16_t arr, uint16_t compare);

#ifdef __cplusplus
}
#endif

#endif /* PWM_OUTPUT_DRIVER_H */
