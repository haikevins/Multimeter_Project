#include "app.h"
#include "adc.h"
#include "gpio.h"
#include "menu.h"
#include "flash.h"
#include <stdio.h>
#include "usart1.h"
#include "systick.h"
#include "measure.h"
#include "transmit.h"
#include "i2c2_lcd.h"
#include "pwm_input.h"
#include "pwm_output.h"

void App_Init(void) {
  SystemInit();
  Flash_Init();
  SysTick_Init();
  Gpio_Button_Init_All();
  Usart1_Init(9600);
  Adc_Init();
  Pwm_Output_Init();
  Transmit_Freq_Duty(currentConfig.frequency_value, currentConfig.duty_value);
  Pwm_Input_Init();
  I2C2_LCD_Init();
  Menu_Init();
#ifdef APP_DEBUG
  printf("Init Done!\r\n");
#endif
}

void App_Logic(void) {
  if (Gpio_Button_Is_Pressed_Once(BUTTON_DOWN)) {
    Gpio_Button_Reset_Once(BUTTON_DOWN);
#ifdef APP_DEBUG
    printf("\r\nButton Down\r\n");
#endif
    Menu_Handle_Down();
  }

  if (Gpio_Button_Is_Pressed_Once(BUTTON_UP)) {
    Gpio_Button_Reset_Once(BUTTON_UP);
#ifdef APP_DEBUG
    printf("\r\nButton Up\r\n");
#endif
    Menu_Handle_Up();
  }

  if (Gpio_Button_Is_Pressed_Once(BUTTON_SELECT)) {
    Gpio_Button_Reset_Once(BUTTON_SELECT);
#ifdef APP_DEBUG
    printf("\r\nButton Select\r\n");
#endif
    Menu_Handle_Select();
  }

  if (Gpio_Button_Is_Enabled(BUTTON_CHARGE) && Gpio_Button_Is_Pressed_Once(BUTTON_CHARGE)) {
    Gpio_Button_Reset_Once(BUTTON_CHARGE);
#ifdef APP_DEBUG
    printf("\r\nButton Charge\r\n");
#endif
    Measure_Capacitor_Start();
  }

  Menu_Update_Measure();
	Gpio_Button_Update_All();
}
