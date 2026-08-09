#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CONFIG_SERVICE_FREQ_MIN_HZ        1U
#define CONFIG_SERVICE_FREQ_MAX_HZ        100000U
#define CONFIG_SERVICE_DUTY_MIN_PERCENT   1U
#define CONFIG_SERVICE_DUTY_MAX_PERCENT   100U
#define CONFIG_SERVICE_FREQ_STEP_MIN_HZ   1U
#define CONFIG_SERVICE_FREQ_STEP_MAX_HZ   10000U
#define CONFIG_SERVICE_DUTY_STEP_MIN      1U
#define CONFIG_SERVICE_DUTY_STEP_MAX      10U

void ConfigService_Init(void);
uint8_t ConfigService_Save(void);

uint32_t ConfigService_GetFrequency(void);
uint16_t ConfigService_GetDuty(void);
uint16_t ConfigService_GetFrequencyStep(void);
uint8_t ConfigService_GetDutyStep(void);

void ConfigService_SetFrequency(uint32_t frequency_hz);
void ConfigService_SetDuty(uint16_t duty_percent);
void ConfigService_SetFrequencyStep(uint16_t step_hz);
void ConfigService_SetDutyStep(uint8_t step_percent);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_SERVICE_H */
