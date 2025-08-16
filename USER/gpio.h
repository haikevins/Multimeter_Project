/**
  ******************************************************************************
  * @file    gpio.h
  * @author  Nguyen Ngoc Hai
  * @brief   GPIO button interface for menu navigation (UP, DOWN, SELECT, CHARGE).
  *          Provides initialization and handling functions for button input with LCD menu system.
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

/*Button*/
#define GPIO_BUTTON_PORT GPIOB
#define GPIO_BUTTON_UP_PIN GPIO_Pin_12
#define GPIO_BUTTON_DOWN_PIN GPIO_Pin_13
#define GPIO_BUTTON_SELECT_PIN GPIO_Pin_14
#define GPIO_BUTTON_CHARGE_PIN GPIO_Pin_15

/*Button Algorithm*/
#define GPIO_BUTTON_HIGH_PERCENT 70
#define GPIO_BUTTON_SAMPLING_TIMES 64

#define GPIO_BUTTON_INDEX_IS_VALID(i) ((i) < GPIO_BUTTON_NUMBERS)

#define GPIO_BUTTON_DEBUG

  typedef enum {
    BUTTON_UP = 0,
    BUTTON_DOWN,         // 1
    BUTTON_SELECT,       // 2
    BUTTON_CHARGE,       // 3
    GPIO_BUTTON_NUMBERS  // 4
  } ButtonID_t;


  /*functions*/
  void Gpio_Button_Init_All(void);
  void Gpio_Button_Reset_All(void);
  void Gpio_Button_Update_All(void);
  void Gpio_Button_Enable(uint8_t index);
  void Gpio_Button_Disable(uint8_t index);
  void Gpio_Button_Reset_Once(uint8_t index);
  uint8_t Gpio_Button_Is_Enabled(uint8_t index);
  uint8_t Gpio_Button_Is_Pressed_Once(uint8_t index);
  void Gpio_Init_Pin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIOMode_TypeDef GPIO_Mode, GPIOSpeed_TypeDef GPIO_Speed);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/









