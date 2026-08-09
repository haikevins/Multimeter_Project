/**
  ******************************************************************************
  * @file    button_driver.h
  * @brief   Active-low button driver with time-based debounce and event FIFO.
  ******************************************************************************
  */
#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  BUTTON_UP = 0,
  BUTTON_DOWN,
  BUTTON_SELECT,
  BUTTON_CHARGE,
  BUTTON_COUNT
} ButtonId_t;

typedef enum
{
  BUTTON_EVENT_NONE = 0,
  BUTTON_EVENT_UP_PRESSED,
  BUTTON_EVENT_DOWN_PRESSED,
  BUTTON_EVENT_SELECT_PRESSED,
  BUTTON_EVENT_CHARGE_PRESSED
} ButtonEvent_t;

void ButtonDriver_Init(void);
void ButtonDriver_Process(void);
ButtonEvent_t ButtonDriver_GetEvent(void);
void ButtonDriver_Enable(ButtonId_t button);
void ButtonDriver_Disable(ButtonId_t button);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_DRIVER_H */
