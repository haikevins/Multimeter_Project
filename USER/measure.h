/**
  ******************************************************************************
  * @file    capacitor_measure.h
  * @author  Nguyen Ngoc Hai
  * @brief   This file contains the definitions and function prototypes for 
  *          analog (ADC-based) and timing-based capacitor measurement routines.
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __MEASURE_H__
#define __MEASURE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define GPIO_CHARGE_PORT GPIOA
#define GPIO_CHARGE_PIN GPIO_Pin_5

#define THRESH_LOW_VOLT 1241  /*!< ~1.0V */
#define THRESH_HIGH_VOTL 2482 /*!< ~2.0V */

#define MIN_VOLTAGE 0.01F
#define RESISTOR_1 10000.0F
#define RESISTOR_2 20000.0F

#define PWM_TIMEOUT 1100

#define MEASURE_DEBUG

  typedef enum {
    CAP_IDLE,        /*!< Idle state, waiting for trigger */
    CAP_WAIT,        /*!< Waiting for discharge to complete */
    CAP_DISCHARGING, /*!< Actively discharging capacitor */
    CAP_CHARGING,    /*!< Charging and timing */
    CAP_DONE         /*!< Measurement complete */
  } CapState;

  extern uint8_t measure_cap_done;

  extern float measured_duty;
  extern float measured_resistor;
  extern float measured_frequency;
  extern float measured_capacitance;

  void Measure_Resistor(void);
  void Measure_Capacitor(void);
  void Measure_Capacitor_Start(void);
  void Measure_Charge_Pin_Init(void);
  void Measure_Charge_Pin_DeInit(void);
  void Measure_Freq_Duty_Apdative(void);

#ifdef __cplusplus
}
#endif

#endif /* __MEASURE_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
