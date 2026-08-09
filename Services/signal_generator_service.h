/**
  ******************************************************************************
  * @file    signal_generator_service.h
  * @brief   PWM signal-generator service: validates settings and calculates timer values.
  ******************************************************************************
  */
#ifndef SIGNAL_GENERATOR_SERVICE_H
#define SIGNAL_GENERATOR_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SIGNAL_GENERATOR_FREQ_MIN_HZ      1U
#define SIGNAL_GENERATOR_FREQ_MAX_HZ      100000U
#define SIGNAL_GENERATOR_DUTY_MIN_PERCENT 1U
#define SIGNAL_GENERATOR_DUTY_MAX_PERCENT 100U
#define SIGNAL_GENERATOR_TIMER_CLOCK_HZ   72000000U

void SignalGeneratorService_Init(uint32_t frequency_hz, uint8_t duty_percent);
void SignalGeneratorService_Apply(uint32_t frequency_hz, uint8_t duty_percent);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_GENERATOR_SERVICE_H */
