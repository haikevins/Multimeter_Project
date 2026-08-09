#ifndef LCD1602_DRIVER_H
#define LCD1602_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define LCD1602_TIMEOUT_MS 10U

typedef enum
{
  LCD1602_OK = 0,
  LCD1602_ERROR_NOT_READY,
  LCD1602_ERROR_BUS_BUSY,
  LCD1602_ERROR_START_TIMEOUT,
  LCD1602_ERROR_ADDR_TIMEOUT,
  LCD1602_ERROR_NACK,
  LCD1602_ERROR_DATA_TIMEOUT,
  LCD1602_ERROR_BUS,
  LCD1602_ERROR_NOT_FOUND
} Lcd1602Driver_Status_t;

Lcd1602Driver_Status_t Lcd1602Driver_Init(void);
Lcd1602Driver_Status_t Lcd1602Driver_Clear(void);
Lcd1602Driver_Status_t Lcd1602Driver_WriteChar(char c);
Lcd1602Driver_Status_t Lcd1602Driver_WriteString(const char* str);
Lcd1602Driver_Status_t Lcd1602Driver_SetCursor(char col, char row);
uint8_t Lcd1602Driver_IsReady(void);
Lcd1602Driver_Status_t Lcd1602Driver_GetLastError(void);
const char* Lcd1602Driver_StatusString(Lcd1602Driver_Status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LCD1602_DRIVER_H */
