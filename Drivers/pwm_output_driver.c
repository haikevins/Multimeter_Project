#include "pwm_output_driver.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "debug_logger.h"

static const uint16_t PWM_OUTPUT_PIN = GPIO_Pin_1;

/**
 * @brief  Initialize PWM output on TIM2 Channel 2 (PA1).
 * 
 * This function configures TIM2 to generate PWM signals on GPIOA Pin 1.  
 * It sets up the timer base, PWM mode, and output compare configuration.  
 * The PWM is disabled by default after initialization.
 *
 * Timer configuration:
 * - Clock source: APB1 (72 MHz)
 * - Channel: TIM2_CH2 (PA1)
 * - Prescaler: 0 (timer clock = 72 MHz)
 * - Period: 720 - 1 (=> base frequency = 100 kHz before prescaler adjustment)
 * - PWM mode: PWM1
 * - Output polarity: Active HIGH
 *
 * @note Call @ref PwmOutputDriver_Enable() to start PWM output.
 */
void PwmOutputDriver_Init(void) {
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = PWM_OUTPUT_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* Timer base configuration */
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_TimeBaseStructure.TIM_Prescaler = 0;
  TIM_TimeBaseStructure.TIM_Period = 720 - 1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
	
	/* Output Compare configuration for PWM */
  TIM_OCInitTypeDef OC_InitStructure;
  OC_InitStructure.TIM_OCMode = TIM_OCMode_PWM1; /** CNT < CCRx  -> chan output HIGH
																									   CNT >= CCRx -> chan output LOW
																									*/
  OC_InitStructure.TIM_OutputState = TIM_OutputState_Enable;
  OC_InitStructure.TIM_Pulse = 0;								/**< Initial duty cycle = 0% */
  OC_InitStructure.TIM_OCPolarity = TIM_OCPolarity_High; /*HIGH -> CNT < CCR2, LOW -> CNT >= CCR2*/
  TIM_OC2Init(TIM2, &OC_InitStructure);
  TIM_OC2PreloadConfig(TIM2, ENABLE);

  TIM_ARRPreloadConfig(TIM2, ENABLE);
	
	/* PWM is disabled by default */
  PwmOutputDriver_Disable();
}

/**
 * @brief  Enable PWM output on TIM2 Channel 2 (PA1).
 * 
 * This function enables TIM2, starting PWM signal generation.
 *
 * @note Make sure PWM is properly initialized before enabling.
 */
void PwmOutputDriver_Enable(void) {
  TIM_Cmd(TIM2, ENABLE);
  DebugLogger_Log(DEBUG_LEVEL_TRACE, "PWM_OUT", "Enable");
}

/**
 * @brief  Disable PWM output on TIM2 Channel 2 (PA1).
 * 
 * This function disables TIM2, stopping PWM signal generation.
 */
void PwmOutputDriver_Disable(void) {
  TIM_Cmd(TIM2, DISABLE);
  DebugLogger_Log(DEBUG_LEVEL_TRACE, "PWM_OUT", "Disable");
}

void PwmOutputDriver_ApplyTimer(uint16_t prescaler, uint16_t arr, uint16_t compare)
{
  TIM_PrescalerConfig(TIM2, prescaler, TIM_PSCReloadMode_Immediate);
  TIM_SetAutoreload(TIM2, arr);
  TIM_SetCompare2(TIM2, compare);
}
