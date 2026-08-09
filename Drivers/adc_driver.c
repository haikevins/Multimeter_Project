#include "adc_driver.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "debug_logger.h"

/* Mapping logical input -> STM32 ADC channel. */
static const uint8_t adc_channel_map[ADC_DRIVER_INPUT_COUNT] = {
  [ADC_DRIVER_INPUT_RESISTOR] = ADC_Channel_3,
  [ADC_DRIVER_INPUT_CAPACITOR] = ADC_Channel_4
};

/* Per-input filter state. */
static float adc_filtered[ADC_DRIVER_INPUT_COUNT] = { 0 };
static uint8_t adc_filter_initialized[ADC_DRIVER_INPUT_COUNT] = { 0 };
static uint8_t adc_gpio_initialized[ADC_DRIVER_INPUT_COUNT] = { 0 };

/* Completed raw batches are written by the ADC ISR and consumed in main. */
static volatile uint32_t adc_completed_sum[ADC_DRIVER_INPUT_COUNT] = { 0 };
static volatile uint8_t adc_result_ready[ADC_DRIVER_INPUT_COUNT] = { 0 };

/* Exactly one ADC1 batch may be active at a time. */
static volatile uint8_t adc_busy = 0U;
static volatile AdcDriver_Input_t adc_active_input = ADC_DRIVER_INPUT_RESISTOR;
static volatile uint32_t adc_sample_sum = 0U;
static volatile uint8_t adc_sample_count = 0U;

static inline float AdcDriver_Low_Pass_Filter(float value_input, float prev_output, float alpha) {
  return alpha * value_input + (1.0f - alpha) * prev_output;
}

static void AdcDriver_Config(AdcDriver_Input_t adc_input) {
  uint8_t adc_channel;
  uint16_t adc_gpio_pin = 0U;
  static uint8_t adc_initialized = 0U;

  if (adc_input >= ADC_DRIVER_INPUT_COUNT) {
    return;
  }

  adc_channel = adc_channel_map[adc_input];

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

  switch (adc_channel) {
    case ADC_Channel_0: adc_gpio_pin = GPIO_Pin_0; break;
    case ADC_Channel_1: adc_gpio_pin = GPIO_Pin_1; break;
    case ADC_Channel_2: adc_gpio_pin = GPIO_Pin_2; break;
    case ADC_Channel_3: adc_gpio_pin = GPIO_Pin_3; break;
    case ADC_Channel_4: adc_gpio_pin = GPIO_Pin_4; break;
    default: return;
  }

  if (!adc_gpio_initialized[adc_input]) {
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = adc_gpio_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    adc_gpio_initialized[adc_input] = 1U;
  }

  if (!adc_initialized) {
    ADC_InitTypeDef ADC_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 72 MHz / 6 = 12 MHz ADC clock (STM32F1 ADC max = 14 MHz). */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    /* Startup-only calibration. Runtime measurement never waits on ADC EOC. */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
      ;

    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
      ;

    /* PWM capture IRQs are priority 0 in this project. ADC is intentionally
     * lower priority so frequency measurement remains the most time-critical. */
    NVIC_InitStructure.NVIC_IRQChannel = ADC1_2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    ADC_ITConfig(ADC1, ADC_IT_EOC, DISABLE);
    adc_initialized = 1U;
  }
}

void AdcDriver_Init(void) {
  AdcDriver_Input_t i;

  for (i = (AdcDriver_Input_t)0; i < ADC_DRIVER_INPUT_COUNT; ++i) {
    AdcDriver_Config(i);
  }

  AdcDriver_Disable();
}

void AdcDriver_Enable(void) {
  ADC_Cmd(ADC1, ENABLE);

  DebugLogger_Log(DEBUG_LEVEL_TRACE, "ADC", "Enable");
}

void AdcDriver_Cancel(void) {
  AdcDriver_Input_t i;

  ADC_ITConfig(ADC1, ADC_IT_EOC, DISABLE);
  ADC_SoftwareStartConvCmd(ADC1, DISABLE);
  ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
  ADC_ClearFlag(ADC1, ADC_FLAG_EOC);

  adc_busy = 0U;
  adc_sample_sum = 0U;
  adc_sample_count = 0U;

  /* Do not let a completed result from the previous menu state leak into
   * the next measurement. Filter history is intentionally preserved. */
  for (i = (AdcDriver_Input_t)0; i < ADC_DRIVER_INPUT_COUNT; ++i) {
    adc_result_ready[i] = 0U;
  }
}

void AdcDriver_Disable(void) {
  AdcDriver_Cancel();
  ADC_Cmd(ADC1, DISABLE);

  DebugLogger_Log(DEBUG_LEVEL_TRACE, "ADC", "Disable");
}

uint8_t AdcDriver_IsBusy(void) {
  return adc_busy;
}

/**
  * @brief Start one 16-sample conversion batch and return immediately.
  * @retval 1 when started, 0 when busy/result pending/invalid.
  */
uint8_t AdcDriver_Request(AdcDriver_Input_t adc_input) {
  uint8_t adc_channel;

  if (adc_input >= ADC_DRIVER_INPUT_COUNT || adc_busy || adc_result_ready[adc_input]) {
    return 0U;
  }

  adc_channel = adc_channel_map[adc_input];
  adc_active_input = adc_input;
  adc_sample_sum = 0U;
  adc_sample_count = 0U;
  adc_busy = 1U;

  ADC_RegularChannelConfig(ADC1, adc_channel, 1, ADC_SampleTime_239Cycles5);
  ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);
  ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
  ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
  ADC_SoftwareStartConvCmd(ADC1, ENABLE);

  return 1U;
}

/**
  * @brief Consume one completed ADC batch in main context.
  * @note  Floating-point averaging/filtering stays out of the ISR.
  */
uint8_t AdcDriver_GetResult(AdcDriver_Input_t adc_input, float* value) {
  uint32_t sum;
  float adc_raw_value;

  if (adc_input >= ADC_DRIVER_INPUT_COUNT || value == 0 || !adc_result_ready[adc_input]) {
    return 0U;
  }

  sum = adc_completed_sum[adc_input];
  adc_result_ready[adc_input] = 0U;

  adc_raw_value = (float)sum / (float)ADC_SAMPLE_COUNT;

  if (!adc_filter_initialized[adc_input]) {
    adc_filtered[adc_input] = adc_raw_value;
    adc_filter_initialized[adc_input] = 1U;
  } else {
    adc_filtered[adc_input] = AdcDriver_Low_Pass_Filter(
      adc_raw_value,
      adc_filtered[adc_input],
      ADC_FILTER_ALPHA
    );
  }

  *value = adc_filtered[adc_input];
  return 1U;
}

void AdcDriver_ResetFilter(AdcDriver_Input_t adc_input) {
  if (adc_input >= ADC_DRIVER_INPUT_COUNT) {
    return;
  }

  adc_filtered[adc_input] = 0.0F;
  adc_filter_initialized[adc_input] = 0U;
}

/**
  * @brief ADC1/ADC2 shared interrupt handler.
  * @note  Handles one EOC at a time, accumulates 16 samples, then publishes
  *        the raw sum. No delays, loops, printf or floating-point work here.
  */
void ADC1_2_IRQHandler(void) {
  uint16_t sample;
  AdcDriver_Input_t input;

  if (ADC_GetITStatus(ADC1, ADC_IT_EOC) == RESET) {
    return;
  }

  sample = ADC_GetConversionValue(ADC1);
  ADC_ClearITPendingBit(ADC1, ADC_IT_EOC);

  if (!adc_busy) {
    ADC_ITConfig(ADC1, ADC_IT_EOC, DISABLE);
    return;
  }

  adc_sample_sum += sample;
  adc_sample_count++;

  if (adc_sample_count < ADC_SAMPLE_COUNT) {
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    return;
  }

  input = adc_active_input;
  adc_completed_sum[input] = adc_sample_sum;
  adc_result_ready[input] = 1U;

  adc_busy = 0U;
  adc_sample_sum = 0U;
  adc_sample_count = 0U;
  ADC_ITConfig(ADC1, ADC_IT_EOC, DISABLE);
}
