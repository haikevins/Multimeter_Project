#include "adc.h"
#include <stdio.h>

/* Mapping enum -> ADC_Channel_x */
static const uint8_t adc_channel_map[ADC_NUM_CHANNELS] = {
  [ADC_INPUT_RES_CHANNEL] = ADC_Channel_3,
  [ADC_INPUT_CAP_CHANNEL] = ADC_Channel_4
};

/* Stores filtered values for each channel */
static float adc_filtered[ADC_NUM_CHANNELS] = { 0 };

/* Tracks whether GPIO for each ADC channel is initialized */
static uint8_t adc_gpio_initialized[ADC_NUM_CHANNELS] = { 0 };

/**
  * @brief  Configure GPIO and ADC for a given ADC input channel.
  * @param  adc_input: ADC input index (see @ref Adc_Input_t).
  * @note   - Initializes GPIO pin as analog input if not already configured.
  *         - Initializes ADC1 once (calibration included).
  * @retval None
  */
static void Adc_Config(Adc_Input_t adc_input) {
  if (adc_input >= ADC_NUM_CHANNELS) {
    return;
  }

  uint8_t adc_channel = adc_channel_map[adc_input];
  uint8_t adc_gpio_pin = 0;
  static uint8_t adc_initialized = 0;

  switch (adc_channel) {
    case ADC_Channel_0:
      {
        adc_gpio_pin = GPIO_Pin_0;
        break;
      }
    case ADC_Channel_1:
      {
        adc_gpio_pin = GPIO_Pin_1;
        break;
      }
    case ADC_Channel_2:
      {
        adc_gpio_pin = GPIO_Pin_2;
        break;
      }
    case ADC_Channel_3:
      {
        adc_gpio_pin = GPIO_Pin_3;
        break;
      }
    default:
      return;
  }

  if (!adc_gpio_initialized[adc_input]) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = adc_gpio_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    adc_gpio_initialized[adc_input] = 1;
  }

  if (!adc_initialized) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; /* Software trigger */
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
		
		/* Reset calibration */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
      ;
		/* Start calibration */
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
      ;

    adc_initialized = 1;
  }
}

/**
  * @brief  Initialize all project-specific ADC channels.
  * @note   Typically called once during system startup.
  * @retval None
  */
void Adc_Init(void) {
  for (Adc_Input_t i = (Adc_Input_t)0; i < ADC_NUM_CHANNELS; ++i) {
    Adc_Config(i);
  }

  Adc_Disable();
}

/**
  * @brief  Enable ADC1 peripheral.
  * @retval None
  */
void Adc_Enable(void) {
  ADC_Cmd(ADC1, ENABLE);

#ifdef ADC_DEBUG
  printf("ADC Enable!\r\n");
#endif
}

/**
  * @brief  Disable ADC1 peripheral.
  * @retval None
  */
void Adc_Disable(void) {
  ADC_Cmd(ADC1, DISABLE);

#ifdef ADC_DEBUG
  printf("ADC Disable!\r\n");
#endif
}

/**
  * @brief  First-order low-pass filter for ADC readings.
  * @param  value_input: Current raw ADC value.
  * @param  prev_output: Previous filtered value.
  * @param  alpha: Filter coefficient (0.0 – 1.0).
  * @retval New filtered value.
  */
static inline float Adc_Low_Pass_Filter(float value_input, float prev_output, float alpha) {
  return alpha * value_input + (1.0f - alpha) * prev_output;
}

/**
  * @brief  Read and filter ADC value from a given input channel.
  * @param  adc_input: ADC input index (see @ref Adc_Input_t).
  * @retval Filtered ADC value (0 – 4095 as float).
  * @note   - Takes multiple samples (@ref ADC_SAMPLE_COUNT) and averages them.
  *         - Applies per-channel low-pass filtering.
  */
float Adc_Read_Channel(Adc_Input_t adc_input) {
  if (adc_input >= ADC_NUM_CHANNELS) {
    return 0;
  }

  uint8_t adc_channel = adc_channel_map[adc_input];
  uint32_t adc_sum_value = 0;

  ADC_RegularChannelConfig(ADC1, adc_channel, 1, ADC_SampleTime_239Cycles5);

  for (uint8_t i = 0; i < ADC_SAMPLE_COUNT; ++i) {
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC)) /*End Of Conversion*/
      ;
    adc_sum_value += ADC_GetConversionValue(ADC1);
  }

  float adc_raw_value = (float)(adc_sum_value / ADC_SAMPLE_COUNT);

  /* Apply per-channel filtering */
  if (adc_channel < ADC_NUM_CHANNELS) {
    adc_filtered[adc_channel] = Adc_Low_Pass_Filter(adc_raw_value, adc_filtered[adc_channel], ADC_FILTER_ALPHA);
  } else {
    return adc_raw_value;   /* fallback */
  }

  return adc_filtered[adc_channel];
}

/**
  * @note ADC conversion time:
  *       T_total = T_sample + 12.5 cycles
  *       - T_sample: sampling time
  *       - 12.5: fixed cycles for conversion
  */
