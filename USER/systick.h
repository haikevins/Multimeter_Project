/**
  ******************************************************************************
  * @file    systick.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header file for SysTick timer configuration and timing utilities.
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __SYSTICK_H__
#define __SYSTICK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

  void SysTick_Init(void);
  void Delay_Ms(uint16_t time_ms);
  uint32_t SysTick_Get_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTICK_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
