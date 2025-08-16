/**
  ******************************************************************************
  * @file    pwm_output.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header file for PWM output configuration using TIM2 Channel 2.
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __PWM_OUTPUT_H__
#define __PWM_OUTPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define PWM_OUTPUT_DEBUG

  void Pwm_Output_Init(void);
  void Pwm_Output_Enable(void);
  void Pwm_Output_Disable(void);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_OUTPUT_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
