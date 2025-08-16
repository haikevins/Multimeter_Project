/**
  ******************************************************************************
  * @file    flash.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header for Flash memory read/write configuration functions.
  * @date    25-April-2025
  ******************************************************************************
  */
#ifndef __FLASH_H__
#define __FLASH_H__

#include "stm32f10x.h"

#define FLASH_MAGIC 0xA5A5A5A5
#define FLASH_PAGE_ADDRESS ((uint32_t)0x0800FC00)

#define FLASH_DEBUG

typedef struct
{
  uint32_t magic;           /*!< Magic number to validate Flash data */
  uint32_t frequency_value; /*!< Frequency output value in Hz */
  uint16_t duty_value;      /*!< Duty cycle output in % */
  uint16_t freq_step;       /*!< Step size for frequency adjustment */
  uint8_t duty_step;        /*!< Step size for duty cycle adjustment */
  uint8_t reserved;         /*!< Reserved for alignment/future use */
} Config_t;

extern Config_t currentConfig;

/*functions*/
void Flash_Lock(void);
void Flash_Init(void);
void Flash_Save(void);
void Flash_Unlock(void);
void Flash_Read_Config(Config_t* cfg);
void Flash_Write_Config(const Config_t* cfg);
void Flash_Erase_Page(uint32_t address);

#endif /* __FLASH_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
