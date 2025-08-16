/**
  ******************************************************************************
  * @file    transmit.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header file for PWM transmission module.
  *          Provides API for configuring and transmitting PWM frequency
  *          and duty cycle using TIM2.
  * @date    25-April-2025
  ******************************************************************************
  */


#ifndef __TRANSMIT_H__
#define __TRANSMIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define FREQ_MIN 1U
#define DUTY_MIN 1U
#define DUTY_MAX 100U
#define FREQ_MAX 100000U

#define SYSTEM_CLOCK 72000000U

  extern uint16_t transmited_duty;
  extern uint32_t transmited_frequency;

  void Transmit_Freq_Duty(uint32_t frequency, uint8_t duty);

#ifdef __cplusplus
}
#endif

#endif /* __TRANSMIT_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
