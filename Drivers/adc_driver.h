#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ADC_VCC          3.3F
#define ADC_RESOLUTION   4096.0F
#define ADC_SAMPLE_COUNT 16U
#define ADC_FILTER_ALPHA 0.2F

typedef enum
{
  ADC_DRIVER_INPUT_RESISTOR = 0,
  ADC_DRIVER_INPUT_CAPACITOR,
  ADC_DRIVER_INPUT_COUNT
} AdcDriver_Input_t;

void AdcDriver_Init(void);
void AdcDriver_Enable(void);
void AdcDriver_Disable(void);
void AdcDriver_Cancel(void);
uint8_t AdcDriver_IsBusy(void);
uint8_t AdcDriver_Request(AdcDriver_Input_t input);
uint8_t AdcDriver_GetResult(AdcDriver_Input_t input, float* value);
void AdcDriver_ResetFilter(AdcDriver_Input_t input);

#ifdef __cplusplus
}
#endif

#endif /* ADC_DRIVER_H */
