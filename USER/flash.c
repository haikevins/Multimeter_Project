#include "flash.h"
#include "menu.h"
#include <stdio.h>
#include <string.h>
#include "transmit.h"

/**
  * @brief  Default safe configuration used if flash is empty or corrupted.
  */
Config_t currentConfig;

static const Config_t defaultConfig = {
  .magic = FLASH_MAGIC,
  .frequency_value = 50000,
  .duty_value = 50,
  .freq_step = 1000,
  .duty_step = 1,
  .reserved = 0
};

/**
  * @brief  Unlocks the Flash memory for write/erase operations.
  * @note   Uses STM32 flash key sequence.
  * @retval None
  */
inline void Flash_Unlock(void) {
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
}

/**
  * @brief  Locks the Flash memory to prevent further modifications.
  * @retval None
  */
inline void Flash_Lock(void) {
  FLASH->CR |= FLASH_CR_LOCK;
}

/**
  * @brief  Erases a Flash memory page at the specified address.
  * @param  address: Start address of the page to erase.
  * @retval None
  */
inline void Flash_Erase_Page(uint32_t address) {
  while (FLASH->SR & FLASH_SR_BSY)
    ;                         // Wait if busy
  FLASH->CR |= FLASH_CR_PER;  // Page erase mode
  FLASH->AR = address;
  FLASH->CR |= FLASH_CR_STRT;  // Start erase
  while (FLASH->SR & FLASH_SR_BSY)
    ;  // Wait complete
  FLASH->CR &= ~FLASH_CR_PER;
}

/**
  * @brief  Writes a configuration structure to Flash memory.
  * @param  cfg: Pointer to configuration struct to write.
  * @retval None
  */
void Flash_Write_Config(const Config_t* cfg) {
  __disable_irq();  // Disable interrupts during flash write
  Flash_Unlock();
  Flash_Erase_Page(FLASH_PAGE_ADDRESS);

  const uint32_t* data = (const uint32_t*)cfg;
  uint32_t address = FLASH_PAGE_ADDRESS;
  uint32_t size = sizeof(Config_t) / 4;  // 4 bytes per entry

  for (uint32_t i = 0; i < size; i++) {
    uint32_t value = data[i];

    // Write lower half-word
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t*)address = (uint16_t)(value & 0xFFFF);
    while (FLASH->SR & FLASH_SR_BSY)
      ;
    FLASH->CR &= ~FLASH_CR_PG;
    address += 2;

    // Write upper half-word
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t*)address = (uint16_t)((value >> 16) & 0xFFFF);
    while (FLASH->SR & FLASH_SR_BSY)
      ;
    FLASH->CR &= ~FLASH_CR_PG;
    address += 2;
  }

  Flash_Lock();
  __enable_irq();

  __DSB();  // Data Sync Barrier
  __ISB();  // Instr Sync Barrier

  volatile uint32_t temp = *(volatile uint32_t*)FLASH_PAGE_ADDRESS;
  (void)temp;

  Config_t verify;
  memcpy(&verify, (void*)FLASH_PAGE_ADDRESS, sizeof(Config_t));
  if (verify.magic != FLASH_MAGIC) {
#ifdef FLASH_DEBUG
    printf("[ERROR] Flash verify failed!\r\n");
#endif
  }
}

/**
  * @brief  Reads the configuration from Flash memory.
  * @param  cfg: Pointer to structure where config will be loaded.
  * @note   If data is invalid (wrong magic), defaults are applied.
  * @retval None
  */
void Flash_Read_Config(Config_t* cfg) {
  const uint32_t* flashData = (const uint32_t*)FLASH_PAGE_ADDRESS;
  memcpy(cfg, flashData, sizeof(Config_t));

  // Validate magic
  if (cfg->magic != FLASH_MAGIC) {
    *cfg = defaultConfig;  // Load safe defaults
    return;
  }

  // Clamp values to valid ranges
  if (cfg->frequency_value < 1 || cfg->frequency_value > 100000) {
    cfg->frequency_value = defaultConfig.frequency_value;
  }

  if (cfg->duty_value < 1 || cfg->duty_value > 100) {
    cfg->duty_value = defaultConfig.duty_value;
  }

  if (cfg->freq_step < 1 || cfg->freq_step > 10000) {
    cfg->freq_step = defaultConfig.freq_step;
  }

  if (cfg->duty_step < 1 || cfg->duty_step > 10) {
    cfg->duty_step = defaultConfig.duty_step;
  }
}

/**
  * @brief  Loads configuration from Flash and applies to global variables.
  * @retval None
  */
void Flash_Init(void) {
  Config_t loaded;
  Flash_Read_Config(&loaded);
  currentConfig = loaded;

  transmited_frequency = currentConfig.frequency_value;
  transmited_duty = currentConfig.duty_value;
  freq_step = currentConfig.freq_step;
  duty_step = currentConfig.duty_step;
}

/**
  * @brief  Saves current global config to Flash if changed.
  * @note   Compares with last saved config to avoid unnecessary erase/write.
  * @retval None
  */
void Flash_Save(void) {
  Config_t newConfig = {
    .magic = FLASH_MAGIC,
    .frequency_value = transmited_frequency,
    .duty_value = transmited_duty,
    .freq_step = freq_step,
    .duty_step = duty_step,
    .reserved = 0
  };

  // Only write if values changed
  if (memcmp(&newConfig, &currentConfig, sizeof(Config_t)) != 0) {
    currentConfig = newConfig;
    Flash_Write_Config(&currentConfig);
  }
#ifdef FLASH_DEBUG
  printf("Flash Saved!\r\n");
#endif
}
