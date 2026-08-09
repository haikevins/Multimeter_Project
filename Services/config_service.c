#include "config_service.h"
#include "flash_storage_driver.h"

#define CONFIG_SERVICE_DEFAULT_FREQUENCY_HZ 50000U
#define CONFIG_SERVICE_DEFAULT_DUTY_PERCENT 50U
#define CONFIG_SERVICE_DEFAULT_FREQ_STEP_HZ 1000U
#define CONFIG_SERVICE_DEFAULT_DUTY_STEP    1U

typedef struct
{
  uint32_t frequency_hz;
  uint16_t duty_percent;
  uint16_t frequency_step_hz;
  uint8_t duty_step_percent;
} App_Config_State_t;

static App_Config_State_t app_config;

static uint32_t ConfigService_Clamp_U32(uint32_t value, uint32_t min_value, uint32_t max_value)
{
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

static void ConfigService_Load_Defaults(void)
{
  app_config.frequency_hz = CONFIG_SERVICE_DEFAULT_FREQUENCY_HZ;
  app_config.duty_percent = CONFIG_SERVICE_DEFAULT_DUTY_PERCENT;
  app_config.frequency_step_hz = CONFIG_SERVICE_DEFAULT_FREQ_STEP_HZ;
  app_config.duty_step_percent = CONFIG_SERVICE_DEFAULT_DUTY_STEP;
}

void ConfigService_Init(void)
{
  FlashStorage_Settings_t settings;
  FlashStorage_Status_t status;

  ConfigService_Load_Defaults();
  status = FlashStorage_Load(&settings);

  if ((status == FLASH_STORAGE_OK) || (status == FLASH_STORAGE_MIGRATED_V0)) {
    ConfigService_SetFrequency(settings.frequency_value);
    ConfigService_SetDuty(settings.duty_value);
    ConfigService_SetFrequencyStep(settings.freq_step);
    ConfigService_SetDutyStep(settings.duty_step);
  }
}

uint8_t ConfigService_Save(void)
{
  FlashStorage_Settings_t settings;

  settings.frequency_value = app_config.frequency_hz;
  settings.duty_value = app_config.duty_percent;
  settings.freq_step = app_config.frequency_step_hz;
  settings.duty_step = app_config.duty_step_percent;

  return (FlashStorage_Save(&settings) == FLASH_STORAGE_OK) ? 1U : 0U;
}

uint32_t ConfigService_GetFrequency(void)
{
  return app_config.frequency_hz;
}

uint16_t ConfigService_GetDuty(void)
{
  return app_config.duty_percent;
}

uint16_t ConfigService_GetFrequencyStep(void)
{
  return app_config.frequency_step_hz;
}

uint8_t ConfigService_GetDutyStep(void)
{
  return app_config.duty_step_percent;
}

void ConfigService_SetFrequency(uint32_t frequency_hz)
{
  app_config.frequency_hz = ConfigService_Clamp_U32(frequency_hz,
                                                CONFIG_SERVICE_FREQ_MIN_HZ,
                                                CONFIG_SERVICE_FREQ_MAX_HZ);
}

void ConfigService_SetDuty(uint16_t duty_percent)
{
  app_config.duty_percent = (uint16_t)ConfigService_Clamp_U32(duty_percent,
                                                          CONFIG_SERVICE_DUTY_MIN_PERCENT,
                                                          CONFIG_SERVICE_DUTY_MAX_PERCENT);
}

void ConfigService_SetFrequencyStep(uint16_t step_hz)
{
  app_config.frequency_step_hz = (uint16_t)ConfigService_Clamp_U32(step_hz,
                                                               CONFIG_SERVICE_FREQ_STEP_MIN_HZ,
                                                               CONFIG_SERVICE_FREQ_STEP_MAX_HZ);
}

void ConfigService_SetDutyStep(uint8_t step_percent)
{
  app_config.duty_step_percent = (uint8_t)ConfigService_Clamp_U32(step_percent,
                                                              CONFIG_SERVICE_DUTY_STEP_MIN,
                                                              CONFIG_SERVICE_DUTY_STEP_MAX);
}
