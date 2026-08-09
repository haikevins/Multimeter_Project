#ifndef FLASH_STORAGE_DRIVER_H
#define FLASH_STORAGE_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FLASH_STORAGE_MAGIC        0xA5A5A5A5UL
#define FLASH_STORAGE_VERSION      1U
#define FLASH_STORAGE_PAGE_ADDRESS ((uint32_t)0x0800FC00)

typedef enum
{
  FLASH_STORAGE_OK = 0,
  FLASH_STORAGE_MIGRATED_V0,
  FLASH_STORAGE_ERROR_MAGIC,
  FLASH_STORAGE_ERROR_VERSION,
  FLASH_STORAGE_ERROR_SIZE,
  FLASH_STORAGE_ERROR_CRC,
  FLASH_STORAGE_ERROR_RANGE,
  FLASH_STORAGE_ERROR_ERASE,
  FLASH_STORAGE_ERROR_PROGRAM,
  FLASH_STORAGE_ERROR_VERIFY
} FlashStorage_Status_t;

typedef struct
{
  uint32_t frequency_value;
  uint16_t duty_value;
  uint16_t freq_step;
  uint8_t duty_step;
} FlashStorage_Settings_t;

FlashStorage_Status_t FlashStorage_Load(FlashStorage_Settings_t* settings);
FlashStorage_Status_t FlashStorage_Save(const FlashStorage_Settings_t* settings);
FlashStorage_Status_t FlashStorage_GetLastStatus(void);
const char* FlashStorage_StatusString(FlashStorage_Status_t status);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_STORAGE_DRIVER_H */
