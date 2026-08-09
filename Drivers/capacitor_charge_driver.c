#include "capacitor_charge_driver.h"
#include "stm32f10x.h"

#define CAPACITOR_CHARGE_PORT GPIOA
#define CAPACITOR_CHARGE_PIN  GPIO_Pin_5

static void CapacitorChargeDriver_ConfigPin(GPIOMode_TypeDef mode)
{
  GPIO_InitTypeDef gpio;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
  gpio.GPIO_Pin = CAPACITOR_CHARGE_PIN;
  gpio.GPIO_Mode = mode;
  gpio.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(CAPACITOR_CHARGE_PORT, &gpio);
}

void CapacitorChargeDriver_Enter(void)
{
  CapacitorChargeDriver_ConfigPin(GPIO_Mode_Out_PP);
  GPIO_ResetBits(CAPACITOR_CHARGE_PORT, CAPACITOR_CHARGE_PIN);
}

void CapacitorChargeDriver_Exit(void)
{
  GPIO_ResetBits(CAPACITOR_CHARGE_PORT, CAPACITOR_CHARGE_PIN);
  CapacitorChargeDriver_ConfigPin(GPIO_Mode_IN_FLOATING);
}

void CapacitorChargeDriver_SetCharging(uint8_t charging)
{
  if (charging) {
    GPIO_SetBits(CAPACITOR_CHARGE_PORT, CAPACITOR_CHARGE_PIN);
  } else {
    GPIO_ResetBits(CAPACITOR_CHARGE_PORT, CAPACITOR_CHARGE_PIN);
  }
}
