/**
  ******************************************************************************
  * @file    pwm_input.c
  * @author  Nguyen Ngoc Hai
  * @brief   PWM input capture implementation using TIM1, TIM3, and TIM4.
  *
  * This module initializes timer peripherals to capture PWM signals and
  * calculate duty cycle and frequency. It supports multiple frequency ranges
  * by switching between different timers with different prescalers.
  *
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __PWM_INPUT_H__
#define __PWM_INPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define THRESH_LOW_FREQ 200
#define THRESH_MID_FREQ 4000

#define PWM_INPUT_DEGUG

  typedef enum {
    PWM_CHANNEL_1 = 0,
    PWM_CHANNEL_3,   //1
    PWM_CHANNEL_4,   //2
    PWM_CHANNEL_NUM  //3
  } PWM_Channel;

  typedef struct
  {
    volatile uint8_t ready_flag;
    volatile uint32_t period;
    volatile uint32_t high_time;
    volatile uint32_t last_capture_time_ms;
    uint32_t timer_clock_hz;
  } Pwm_Input_Capture;

  extern Pwm_Input_Capture pwm_inputs[PWM_CHANNEL_NUM];

  void Pwm_Input_Init(void);
  void Pwm_Input_Enable(void);
  void Pwm_Input_Disable(void);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_INPUT_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
	
	