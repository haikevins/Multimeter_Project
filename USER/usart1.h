/**
  ******************************************************************************
  * @file    usart1.h
  * @author  Nguyen Ngoc Hai
  * @date    25-April-2025
  * @brief   USART1 interface header.
  *
  * Provides initialization, transmit, and interrupt-based receive handling
  * for USART1 communication, including buffer management and command parsing.
  ******************************************************************************
  */

#ifndef __UART1_H__
#define __UART1_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define USART1_RX_BUFFER_SIZE 64

#define USART1_Send_Digit(num) Usart1_Send_Format("%d", num)
#define USART1_Send_Hex(num) Usart1_Send_Format("%04X", num)
#define USART1_Send_Float(n, p) Usart1_Send_Format("%.*f", p, n)

  void Usart1_Send_Char(char chr);
  void Usart1_Send_String(char *str);
  void Usart1_Init(uint16_t baudrate);
  void Usart1_Send_Format(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __UART1_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
