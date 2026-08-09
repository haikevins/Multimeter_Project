#include "system_time.h"
#include "stm32f10x.h"

static volatile uint32_t systick_ms = 0;

/**
  * @brief  Initializes the SysTick timer to generate an interrupt every 1ms.
  * @note   Uses the system core clock (HCLK). Ensure SystemCoreClock is set correctly.
  * @retval None
  */
void SystemTime_Init(void) {
  SysTick_Config(SystemCoreClock / 1000);
}

/**
  * @brief  Generates a blocking delay in milliseconds.
  * @param  time_ms: Duration of the delay in milliseconds.
  * @note   Uses the millisecond counter updated by the SysTick interrupt.
  * @retval None
  */
void SystemTime_DelayMs(uint16_t time_ms) {
  uint32_t start = systick_ms;
  while ((systick_ms - start) < time_ms)
    ;
}

/**
  * @brief  Get the current SysTick tick count in milliseconds.
  * @retval Current value of systick_ms counter.
  */
uint32_t SystemTime_GetTick(void) {
  return systick_ms;
}

/**
  * @brief  SysTick interrupt handler.
  * @note   Called every 1ms. Increments @ref systick_ms and sets periodic flags.
  * @retval None
  */
void SysTick_Handler(void) {
  systick_ms++;
}
