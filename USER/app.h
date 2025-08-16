/**
  ******************************************************************************
  * @file    app.h
  * @author  Nguyen Ngoc Hai
  * @brief   Application layer interface for STM32 dual-core system.
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define APP_DEBUG

  void App_Init(void);
  void App_Logic(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
