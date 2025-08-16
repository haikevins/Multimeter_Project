/**
  ******************************************************************************
  * @file    adc.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header file for ADC configuration, initialization, and reading.
  *          Provides APIs for enabling/disabling ADC1, reading raw values,
  *          and applying per-channel low-pass filtering.
  * @date    25-April-2025
  ******************************************************************************
  */

#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define ADC_VCC 3.3F
#define ADC_RESOLUTION 4096.0F

#define ADC_SAMPLE_COUNT 16

#define ADC_FILTER_ALPHA 0.2F

#define ADC_DEBUG

  typedef enum {
    ADC_INPUT_RES_CHANNEL = 0,
    ADC_INPUT_CAP_CHANNEL,  //1
    ADC_NUM_CHANNELS        //2
  } Adc_Input_t;

  void Adc_Init(void);
  void Adc_Enable(void);
  void Adc_Disable(void);
  float Adc_Read_Channel(Adc_Input_t adc_input);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/
	
