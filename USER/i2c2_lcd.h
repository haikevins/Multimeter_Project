/**
  ******************************************************************************
  * @file    lcd_i2c2.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header for I2C2 LCD driver (16x2 LCD with PCF8574).
  * @date    25-April-2025
  ******************************************************************************
  * @attention
  * Provides functions for initializing and writing to 16x2 character LCD
  * over I2C using STM32F1 I2C2 peripheral.
  *
  * Wiring example:
  *   - PB10: I2C2_SCL
  *   - PB11: I2C2_SDA
  *
  * LCD address is auto-scanned from 0x20–0x3F (except reserved 0x28).
  ******************************************************************************
  */
#ifndef __I2C_LCD_H__
#define __I2C_LCD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define I2C2_LCD_TIMEOUT 10000

#define I2C2_LCD_DEBUG

  extern uint8_t i2ic2_lcd_address;

  uint8_t I2C2_Scan_Address(void);

  void I2C2_LCD_Init(void);
  void I2C2_LCD_Clear(void);
  void I2C2_Send_Stop(void);
  void I2C2_Send_Start(void);
  void I2C2_LCD_Load_Icons(void);
  void I2C2_LCD_Send_Char(char c);
  void I2C2_Send_Data(uint8_t data);
  void I2C2_LCD_Write_byte(char data);
  void I2C2_LCD_Data_Write(char data);
  void I2C2_Send_Address(uint8_t addr);
  void I2C2_LCD_Send_Float(float number);
  void I2C2_LCD_Control_Write(char data);
  void I2C2_LCD_Send_Number(uint16_t num);
  void I2C2_LCD_Send_String(const char *str);
  void I2C2_LCD_Set_Cursor(char col, char row);
  void I2C2_LCD_Create_Char(uint8_t location, uint8_t *charmap);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_I2C2_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
