/**
  ******************************************************************************
  * @file    error_manager.h
  * @brief   Central non-blocking error registry for application modules.
  ******************************************************************************
  */
#ifndef ERROR_MANAGER_H
#define ERROR_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  ERROR_SOURCE_SYSTEM = 0,
  ERROR_SOURCE_FLASH,
  ERROR_SOURCE_I2C_LCD,
  ERROR_SOURCE_UART,
  ERROR_SOURCE_ADC,
  ERROR_SOURCE_BUTTON,
  ERROR_SOURCE_MEASURE,
  ERROR_SOURCE_PWM_INPUT,
  ERROR_SOURCE_PWM_OUTPUT
} Error_Source_t;

typedef enum
{
  ERROR_SEVERITY_WARNING = 0,
  ERROR_SEVERITY_ERROR,
  ERROR_SEVERITY_FATAL
} Error_Severity_t;

/* Codes are grouped by subsystem to remain readable in a debugger/log. */
typedef enum
{
  ERROR_CODE_NONE = 0x0000,

  ERROR_CODE_FLASH_MIGRATED_V0 = 0x0101,
  ERROR_CODE_FLASH_BAD_MAGIC   = 0x0102,
  ERROR_CODE_FLASH_BAD_VERSION = 0x0103,
  ERROR_CODE_FLASH_BAD_SIZE    = 0x0104,
  ERROR_CODE_FLASH_BAD_CRC     = 0x0105,
  ERROR_CODE_FLASH_BAD_RANGE   = 0x0106,
  ERROR_CODE_FLASH_ERASE       = 0x0107,
  ERROR_CODE_FLASH_PROGRAM     = 0x0108,
  ERROR_CODE_FLASH_VERIFY      = 0x0109,

  ERROR_CODE_LCD_NOT_READY     = 0x0201,
  ERROR_CODE_LCD_BUS_BUSY      = 0x0202,
  ERROR_CODE_LCD_START_TIMEOUT = 0x0203,
  ERROR_CODE_LCD_ADDR_TIMEOUT  = 0x0204,
  ERROR_CODE_LCD_NACK          = 0x0205,
  ERROR_CODE_LCD_DATA_TIMEOUT  = 0x0206,
  ERROR_CODE_LCD_BUS           = 0x0207,
  ERROR_CODE_LCD_NOT_FOUND     = 0x0208,

  ERROR_CODE_UART_TX_TIMEOUT   = 0x0301,

  ERROR_CODE_BUTTON_QUEUE_FULL = 0x0401,

  ERROR_CODE_ADC_CONVERSION_TIMEOUT = 0x0501,

  ERROR_CODE_PWM_CAPTURE_UNAVAILABLE = 0x0601,
  ERROR_CODE_PWM_INVALID_CAPTURE     = 0x0602
} Error_Code_t;

typedef struct
{
  Error_Source_t source;
  Error_Code_t code;
  Error_Severity_t severity;
  uint32_t timestamp_ms;
  uint16_t repeat_count;
} Error_Record_t;

#define ERROR_MANAGER_HISTORY_SIZE 8U
#define ERROR_MANAGER_COALESCE_MS 1000U

void ErrorManager_Init(void);
void ErrorManager_Report(Error_Source_t source,
                         Error_Code_t code,
                         Error_Severity_t severity);
void ErrorManager_Flush(void);
void ErrorManager_Clear(void);
uint8_t ErrorManager_Get_Last(Error_Record_t* record);
uint32_t ErrorManager_Get_Total_Count(void);

const char* ErrorManager_Source_String(Error_Source_t source);
const char* ErrorManager_Code_String(Error_Code_t code);
const char* ErrorManager_Severity_String(Error_Severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* ERROR_MANAGER_H */
