/**
  ******************************************************************************
  * @file    capacitor_charge_driver.h
  * @brief   Hardware driver for the capacitor charge/discharge control pin.
  ******************************************************************************
  */
#ifndef CAPACITOR_CHARGE_DRIVER_H
#define CAPACITOR_CHARGE_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void CapacitorChargeDriver_Enter(void);
void CapacitorChargeDriver_Exit(void);
void CapacitorChargeDriver_SetCharging(uint8_t charging);

#ifdef __cplusplus
}
#endif

#endif /* CAPACITOR_CHARGE_DRIVER_H */
