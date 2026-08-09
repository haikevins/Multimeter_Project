#include "flash_storage_driver.h"
#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "error_manager.h"
#include "debug_logger.h"

/* Private on-Flash record. Layout details never leave flash.c. */
typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t record_size;
  uint32_t frequency_value;
  uint16_t duty_value;
  uint16_t freq_step;
  uint8_t duty_step;
  uint8_t reserved[3];
  uint32_t crc32;
} Config_t;

/* Legacy layout used before FLASH_STORAGE_VERSION was introduced. */
typedef struct
{
  uint32_t magic;
  uint32_t frequency_value;
  uint16_t duty_value;
  uint16_t freq_step;
  uint8_t duty_step;
  uint8_t reserved;
} Config_V0_t;

static Config_t currentConfig;

static FlashStorage_Status_t flash_last_status = FLASH_STORAGE_OK;
static uint8_t flash_needs_rewrite = 0U;

static const Config_t defaultConfigTemplate = {
  .magic = FLASH_STORAGE_MAGIC,
  .version = FLASH_STORAGE_VERSION,
  .record_size = sizeof(Config_t),
  .frequency_value = 50000U,
  .duty_value = 50U,
  .freq_step = 1000U,
  .duty_step = 1U,
  .reserved = {0U, 0U, 0U},
  .crc32 = 0U
};

static uint32_t Flash_Calculate_CRC32(const Config_t* cfg);
static void Flash_Finalize_Config(Config_t* cfg);
static FlashStorage_Status_t Flash_Validate_Config(const Config_t* cfg);
static uint8_t Flash_Config_Values_Valid(uint32_t frequency,
                                         uint16_t duty,
                                         uint16_t frequency_step,
                                         uint8_t duty_step_value);
static uint8_t Flash_Try_Migrate_V0(Config_t* cfg);

static Error_Code_t Flash_Status_To_Error_Code(FlashStorage_Status_t status)
{
  switch (status) {
    case FLASH_STORAGE_MIGRATED_V0:  return ERROR_CODE_FLASH_MIGRATED_V0;
    case FLASH_STORAGE_ERROR_MAGIC:  return ERROR_CODE_FLASH_BAD_MAGIC;
    case FLASH_STORAGE_ERROR_VERSION:return ERROR_CODE_FLASH_BAD_VERSION;
    case FLASH_STORAGE_ERROR_SIZE:   return ERROR_CODE_FLASH_BAD_SIZE;
    case FLASH_STORAGE_ERROR_CRC:    return ERROR_CODE_FLASH_BAD_CRC;
    case FLASH_STORAGE_ERROR_RANGE:  return ERROR_CODE_FLASH_BAD_RANGE;
    case FLASH_STORAGE_ERROR_ERASE:  return ERROR_CODE_FLASH_ERASE;
    case FLASH_STORAGE_ERROR_PROGRAM:return ERROR_CODE_FLASH_PROGRAM;
    case FLASH_STORAGE_ERROR_VERIFY: return ERROR_CODE_FLASH_VERIFY;
    case FLASH_STORAGE_OK:
    default:                         return ERROR_CODE_NONE;
  }
}

static void Flash_Set_Status(FlashStorage_Status_t status)
{
  Error_Code_t code;
  Error_Severity_t severity;

  flash_last_status = status;
  code = Flash_Status_To_Error_Code(status);
  if (code == ERROR_CODE_NONE) {
    return;
  }

  severity = (status == FLASH_STORAGE_MIGRATED_V0 ||
              status == FLASH_STORAGE_ERROR_MAGIC ||
              status == FLASH_STORAGE_ERROR_VERSION ||
              status == FLASH_STORAGE_ERROR_SIZE ||
              status == FLASH_STORAGE_ERROR_CRC)
               ? ERROR_SEVERITY_WARNING
               : ERROR_SEVERITY_ERROR;

  ErrorManager_Report(ERROR_SOURCE_FLASH, code, severity);
}

/**
  * @brief  IEEE CRC-32 over persistent record metadata + payload.
  * @note   The crc32 field itself is intentionally excluded.
  */
static uint32_t Flash_Calculate_CRC32(const Config_t* cfg)
{
  const uint8_t* data = (const uint8_t*)cfg;
  const uint32_t length = (uint32_t)offsetof(Config_t, crc32);
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t i;
  uint8_t bit;

  for (i = 0U; i < length; ++i) {
    crc ^= data[i];

    for (bit = 0U; bit < 8U; ++bit) {
      if ((crc & 1U) != 0U) {
        crc = (crc >> 1U) ^ 0xEDB88320UL;
      } else {
        crc >>= 1U;
      }
    }
  }

  return ~crc;
}

/**
  * @brief Fill record metadata and compute its final CRC.
  */
static void Flash_Finalize_Config(Config_t* cfg)
{
  cfg->magic = FLASH_STORAGE_MAGIC;
  cfg->version = FLASH_STORAGE_VERSION;
  cfg->record_size = (uint16_t)sizeof(Config_t);
  cfg->reserved[0] = 0U;
  cfg->reserved[1] = 0U;
  cfg->reserved[2] = 0U;
  cfg->crc32 = Flash_Calculate_CRC32(cfg);
}

/**
  * @brief Check semantic ranges independently of CRC/format metadata.
  */
static uint8_t Flash_Config_Values_Valid(uint32_t frequency,
                                         uint16_t duty,
                                         uint16_t frequency_step,
                                         uint8_t duty_step_value)
{
  if ((frequency < 1U) || (frequency > 100000U)) {
    return 0U;
  }

  if ((duty < 1U) || (duty > 100U)) {
    return 0U;
  }

  if ((frequency_step < 1U) || (frequency_step > 10000U)) {
    return 0U;
  }

  if ((duty_step_value < 1U) || (duty_step_value > 10U)) {
    return 0U;
  }

  return 1U;
}

/**
  * @brief Validate persistent record format, integrity and value ranges.
  */
static FlashStorage_Status_t Flash_Validate_Config(const Config_t* cfg)
{
  if (cfg->magic != FLASH_STORAGE_MAGIC) {
    return FLASH_STORAGE_ERROR_MAGIC;
  }

  if (cfg->version != FLASH_STORAGE_VERSION) {
    return FLASH_STORAGE_ERROR_VERSION;
  }

  if (cfg->record_size != (uint16_t)sizeof(Config_t)) {
    return FLASH_STORAGE_ERROR_SIZE;
  }

  if (cfg->crc32 != Flash_Calculate_CRC32(cfg)) {
    return FLASH_STORAGE_ERROR_CRC;
  }

  if (!Flash_Config_Values_Valid(cfg->frequency_value,
                                 cfg->duty_value,
                                 cfg->freq_step,
                                 cfg->duty_step)) {
    return FLASH_STORAGE_ERROR_RANGE;
  }

  return FLASH_STORAGE_OK;
}

/**
  * @brief Convert the pre-versioned V0 Flash record to the current V1 layout.
  * @retval 1 if a valid V0 record was found and converted, otherwise 0.
  */
static uint8_t Flash_Try_Migrate_V0(Config_t* cfg)
{
  Config_V0_t legacy;

  memcpy(&legacy, (const void*)FLASH_STORAGE_PAGE_ADDRESS, sizeof(legacy));

  if (legacy.magic != FLASH_STORAGE_MAGIC) {
    return 0U;
  }

  if (!Flash_Config_Values_Valid(legacy.frequency_value,
                                 legacy.duty_value,
                                 legacy.freq_step,
                                 legacy.duty_step)) {
    return 0U;
  }

  *cfg = defaultConfigTemplate;
  cfg->frequency_value = legacy.frequency_value;
  cfg->duty_value = legacy.duty_value;
  cfg->freq_step = legacy.freq_step;
  cfg->duty_step = legacy.duty_step;
  Flash_Finalize_Config(cfg);

  return 1U;
}

/**
  * @brief  Writes and verifies one complete versioned configuration record.
  * @note   Each erase/program operation is checked. currentConfig is updated
  *         only by FlashStorage_Save() after this function reports FLASH_STORAGE_OK.
  */
static void Flash_Write_Config(const Config_t* cfg)
{
  const uint16_t* data;
  uint32_t halfword_count;
  uint32_t address;
  uint32_t i;
  uint32_t primask;
  FLASH_Status hw_status;
  FlashStorage_Status_t result;
  Config_t verify;

  result = Flash_Validate_Config(cfg);
  if (result != FLASH_STORAGE_OK) {
    Flash_Set_Status(result);
    return;
  }

  data = (const uint16_t*)cfg;
  halfword_count = (uint32_t)(sizeof(Config_t) / sizeof(uint16_t));
  address = FLASH_STORAGE_PAGE_ADDRESS;
  result = FLASH_STORAGE_OK;

  primask = __get_PRIMASK();
  __disable_irq();

  FLASH_Unlock();
  FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

  hw_status = FLASH_ErasePage(FLASH_STORAGE_PAGE_ADDRESS);
  if (hw_status != FLASH_COMPLETE) {
    result = FLASH_STORAGE_ERROR_ERASE;
    goto flash_write_done;
  }

  for (i = 0U; i < halfword_count; ++i) {
    hw_status = FLASH_ProgramHalfWord(address, data[i]);
    if (hw_status != FLASH_COMPLETE) {
      result = FLASH_STORAGE_ERROR_PROGRAM;
      goto flash_write_done;
    }
    address += sizeof(uint16_t);
  }

flash_write_done:
  FLASH_Lock();

  if (primask == 0U) {
    __enable_irq();
  }

  __DSB();
  __ISB();

  if (result != FLASH_STORAGE_OK) {
    Flash_Set_Status(result);
    return;
  }

  memcpy(&verify, (const void*)FLASH_STORAGE_PAGE_ADDRESS, sizeof(verify));

  if ((Flash_Validate_Config(&verify) != FLASH_STORAGE_OK) ||
      (memcmp(&verify, cfg, sizeof(Config_t)) != 0)) {
    Flash_Set_Status(FLASH_STORAGE_ERROR_VERIFY);
    return;
  }

  Flash_Set_Status(FLASH_STORAGE_OK);
}

/**
  * @brief  Read and validate configuration from Flash.
  * @note   Invalid current-format data falls back to safe defaults. A valid
  *         pre-versioned V0 record is converted in RAM and marked for migration.
  */
static void Flash_Read_Config(Config_t* cfg)
{
  FlashStorage_Status_t status;

  memcpy(cfg, (const void*)FLASH_STORAGE_PAGE_ADDRESS, sizeof(Config_t));
  status = Flash_Validate_Config(cfg);

  if (status == FLASH_STORAGE_OK) {
    flash_needs_rewrite = 0U;
    Flash_Set_Status(FLASH_STORAGE_OK);
    return;
  }

  /* Preserve settings from the project version that existed before version/CRC. */
  if (Flash_Try_Migrate_V0(cfg)) {
    flash_needs_rewrite = 1U;
    Flash_Set_Status(FLASH_STORAGE_MIGRATED_V0);
    return;
  }

  *cfg = defaultConfigTemplate;
  Flash_Finalize_Config(cfg);
  flash_needs_rewrite = 1U;
  Flash_Set_Status(status);
}

/**
  * @brief Read persistent settings without knowing any application module globals.
  */
FlashStorage_Status_t FlashStorage_Load(FlashStorage_Settings_t* settings)
{
  Config_t loaded;

  if (settings == NULL) {
    Flash_Set_Status(FLASH_STORAGE_ERROR_RANGE);
    return flash_last_status;
  }

  Flash_Read_Config(&loaded);
  currentConfig = loaded;

  settings->frequency_value = loaded.frequency_value;
  settings->duty_value = loaded.duty_value;
  settings->freq_step = loaded.freq_step;
  settings->duty_step = loaded.duty_step;

  return flash_last_status;
}

/**
  * @brief Persist a complete settings snapshot supplied by the configuration layer.
  */
FlashStorage_Status_t FlashStorage_Save(const FlashStorage_Settings_t* settings)
{
  Config_t newConfig = defaultConfigTemplate;

  if (settings == NULL) {
    Flash_Set_Status(FLASH_STORAGE_ERROR_RANGE);
    return flash_last_status;
  }

  newConfig.frequency_value = settings->frequency_value;
  newConfig.duty_value = settings->duty_value;
  newConfig.freq_step = settings->freq_step;
  newConfig.duty_step = settings->duty_step;

  if (!Flash_Config_Values_Valid(newConfig.frequency_value,
                                 newConfig.duty_value,
                                 newConfig.freq_step,
                                 newConfig.duty_step)) {
    Flash_Set_Status(FLASH_STORAGE_ERROR_RANGE);
    return flash_last_status;
  }

  Flash_Finalize_Config(&newConfig);

  if ((flash_needs_rewrite != 0U) ||
      (memcmp(&newConfig, &currentConfig, sizeof(Config_t)) != 0)) {
    DebugLogger_Log(DEBUG_LEVEL_INFO, "FLASH",
              "Save F=%luHz D=%u%% Fstep=%u Dstep=%u",
              (unsigned long)newConfig.frequency_value,
              (unsigned int)newConfig.duty_value,
              (unsigned int)newConfig.freq_step,
              (unsigned int)newConfig.duty_step);

    Flash_Write_Config(&newConfig);

    if (flash_last_status == FLASH_STORAGE_OK) {
      currentConfig = newConfig;
      flash_needs_rewrite = 0U;
    }
  }

  if (flash_last_status == FLASH_STORAGE_OK) {
    DebugLogger_Log(DEBUG_LEVEL_INFO, "FLASH", "Saved + verified");
  }

  return flash_last_status;
}

FlashStorage_Status_t FlashStorage_GetLastStatus(void)
{
  return flash_last_status;
}

const char* FlashStorage_StatusString(FlashStorage_Status_t status)
{
  switch (status) {
    case FLASH_STORAGE_OK:
      return "OK";
    case FLASH_STORAGE_MIGRATED_V0:
      return "MIGRATED_V0";
    case FLASH_STORAGE_ERROR_MAGIC:
      return "BAD_MAGIC";
    case FLASH_STORAGE_ERROR_VERSION:
      return "BAD_VERSION";
    case FLASH_STORAGE_ERROR_SIZE:
      return "BAD_SIZE";
    case FLASH_STORAGE_ERROR_CRC:
      return "BAD_CRC";
    case FLASH_STORAGE_ERROR_RANGE:
      return "BAD_RANGE";
    case FLASH_STORAGE_ERROR_ERASE:
      return "ERASE_FAILED";
    case FLASH_STORAGE_ERROR_PROGRAM:
      return "PROGRAM_FAILED";
    case FLASH_STORAGE_ERROR_VERIFY:
      return "VERIFY_FAILED";
    default:
      return "UNKNOWN";
  }
}
