/**
  ******************************************************************************
  * @file    measurement_service.h
  * @brief   Non-blocking measurement use-cases and measurement-mode lifecycle.
  ******************************************************************************
  */
#ifndef MEASUREMENT_SERVICE_H
#define MEASUREMENT_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  MEASUREMENT_MODE_NONE = 0,
  MEASUREMENT_MODE_RESISTOR,
  MEASUREMENT_MODE_CAPACITOR,
  MEASUREMENT_MODE_FREQUENCY_DUTY
} Measurement_Mode_t;

typedef enum
{
  MEASUREMENT_RESULT_RESISTOR = 0,
  MEASUREMENT_RESULT_CAPACITANCE,
  MEASUREMENT_RESULT_FREQUENCY,
  MEASUREMENT_RESULT_DUTY
} Measurement_Result_t;

typedef enum
{
  MEASUREMENT_STATUS_IDLE = 0,
  MEASUREMENT_STATUS_MEASURING,
  MEASUREMENT_STATUS_READY,
  MEASUREMENT_STATUS_WAIT_CHARGE,
  MEASUREMENT_STATUS_DISCHARGING,
  MEASUREMENT_STATUS_CHARGING,
  MEASUREMENT_STATUS_NO_SIGNAL,
  MEASUREMENT_STATUS_NO_COMPONENT,
  MEASUREMENT_STATUS_ERROR
} Measurement_Status_t;

void MeasurementService_Init(void);
void MeasurementService_SetMode(Measurement_Mode_t mode, uint32_t expected_frequency_hz);
void MeasurementService_Process(void);
void MeasurementService_StartCapacitor(void);
uint8_t MeasurementService_TakeCapacitorDone(void);
float MeasurementService_GetResult(Measurement_Result_t result);
Measurement_Status_t MeasurementService_GetStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_SERVICE_H */
