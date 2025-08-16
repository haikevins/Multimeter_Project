#include "menu.h"
#include "adc.h"
#include "gpio.h"
#include "flash.h"
#include <stdio.h>
#include "usart1.h"
#include <string.h>
#include "systick.h"
#include "measure.h"
#include "transmit.h"
#include "i2c2_lcd.h"
#include "pwm_input.h"
#include "pwm_output.h"

uint8_t duty_step = 0;
uint16_t freq_step = 0;

/* Internal state */
static uint8_t currentIndex = 0;
static uint8_t menuNeedClear = 1;
static uint8_t measureAllStep = 0;
static MenuLevel currentMenu = MENU_MAIN;

/* Menu items table */
const char* mainMenu[] = {
  LCD_MENU_MEASURE,
  LCD_MENU_TRANSMIT
};

const char* measureMenu[] = {
  LCD_MEASURE_SINGLE,
  LCD_MEASURE_ALL,
  LCD_MENU_BACK
};

const char* transmitMenu[] = {
  LCD_TRANSMIT_FREQ,
  LCD_TRANSMIT_DUTY,
  LCD_TRANSMIT_SETTING,
  LCD_MENU_BACK
};

const char* measureSingleMenu[] = {
  LCD_SINGLE_RESISTOR,
  LCD_SINGLE_CAPACITOR,
  LCD_SINGLE_FREQ,
  LCD_SINGLE_DUTY,
  LCD_MENU_BACK
};

/*Dai dien cho 1 menu*/
const MenuData menuTable[] = {
  { MENU_MAIN, mainMenu, 2 },
  { MENU_MEASURE, measureMenu, 3 },
  { MENU_TRANSMIT, transmitMenu, 4 },
  { MENU_MEASURE_SINGLE, measureSingleMenu, 5 }
};

/*Dai dien cho cac menu do*/
const MeasureDisplay measureDisplays[] = {
  { MENU_MEASURE_RESISTOR, LCD_RESISTOR_MEASURE, &measured_resistor, RESISTOR_UNIT },
  { MENU_MEASURE_CAPACITOR, LCD_CAPACITOR_MEASURE, &measured_capacitance, CAPACITOR_UNIT },
  { MENU_MEASURE_FREQUENCY, LCD_FREQ_MEASURE, &measured_frequency, FREQUENCY_UNIT },
  { MENU_MEASURE_DUTY, LCD_DUTY_MEASURE, &measured_duty, DUTY_CYCLE_UNIT }
};

/*Danh dau la cac menu tinh -> khong the thuc hien dieu huong menu: len hoac xuong*/
static inline uint8_t Menu_Static(MenuLevel level) {
  return (
    level == MENU_MEASURE_RESISTOR || level == MENU_MEASURE_CAPACITOR || level == MENU_MEASURE_FREQUENCY || level == MENU_MEASURE_DUTY || level == MENU_MEASURE_ALL);
}

static inline void Menu_Print_Char(uint8_t col, uint8_t row, const char text) {
  I2C2_LCD_Set_Cursor(col, row);
  I2C2_LCD_Send_Char(text);
}

void Menu_Print_String(uint8_t col, uint8_t row, const char* text) {
  char buffer[17];
  snprintf(buffer, sizeof(buffer), "%-16s", text);
  I2C2_LCD_Set_Cursor(col, row);
  I2C2_LCD_Send_String(buffer);
}

static void Menu_Clear(void) {
  I2C2_LCD_Clear();
}

/*Hien thi tieu de va ten cua project trong 2s khi khoi dong lcd*/
static void Menu_Display_Project_Title(void) {
  Menu_Print_String(3, 0, PROJECT_TITLE);
  Menu_Print_String(2, 1, PROJECT_NAME);
  Delay_Ms(2000);
  Menu_Clear();
}

/*Hien thi danh sach menu len man hinh LCD*/
static void Menu_Display_General(const char** items, uint8_t totalItems, uint8_t selectedIndex, uint8_t isClear) {
  if (isClear) {
    Menu_Clear();
  }

  /*Tranh loi tran mang*/
  if (selectedIndex >= totalItems) {
    selectedIndex = 0;
  }

  /*Muc dau tien se hien thi len man*/
  uint8_t topIndex = (selectedIndex == 0) ? 0 : (selectedIndex - 1);

  /*Hien thi 2 dong menu len LCD*/
  for (uint8_t line = 0; line < 2; line++) {
    uint8_t idx = topIndex + line;
    I2C2_LCD_Set_Cursor(0, line);

    char buffer[17];
    if (idx < totalItems) {
			char prefix = (idx == selectedIndex) ? '>' : ' ';
      snprintf(buffer, sizeof(buffer), "%c%-15s", prefix, items[idx]);
    } else {
      memset(buffer, ' ', 16);
    }

    buffer[16] = '\0';
    I2C2_LCD_Send_String(buffer);
  }

  if (totalItems > 2) {
    for (int i = 0; i < 2; i++) {
      Menu_Print_Char(15, i, ' ');
    }
    if (topIndex > 0) {
      Menu_Print_Char(15, 0, 0);
    }

    if (topIndex + 2 < totalItems) {
      Menu_Print_Char(15, 1, 1);
    }
  }
}

/*Chuyen tu menu cu sang menu moi*/
static void Menu_Change(MenuLevel newMenu, const char** items, uint8_t itemCount) {
  currentMenu = newMenu;
  if (currentIndex >= itemCount) {
    currentIndex = 0;
  }

  menuNeedClear = 1;
  Menu_Display_General(items, itemCount, currentIndex, menuNeedClear);
  menuNeedClear = 0;
}

/*Hien thi tieu de cua cac menu do*/
static void Menu_Display_Title(const char* title) {
  Menu_Clear();
  Menu_Print_String(0, 0, title);
}

static void Menu_Format_Value_Unit(char* buffer, size_t bufferSize, float value, const char* unit) {
  if (strcmp(unit, "\x02") == 0) {
    if (value >= 1e6) {
      snprintf(buffer, bufferSize, "%.2f M\x02", value / 1e6);
    } else if (value >= 1e3) {
      snprintf(buffer, bufferSize, "%.2f k\x02", value / 1e3);
    } else {
      snprintf(buffer, bufferSize, "%.2f \x02", value);
    }
  } else if (strcmp(unit, CAPACITOR_UNIT) == 0) {
    if (value >= 1.0) {
        snprintf(buffer, bufferSize, "%.2f F", value);
    } else if (value >= 1e-3) {
        snprintf(buffer, bufferSize, "%.2f mF", value * 1e3);
    } else if (value >= 1e-6) {
        snprintf(buffer, bufferSize, "%.2f uF", value * 1e6);
    } else if (value >= 1e-9) {
        snprintf(buffer, bufferSize, "%.2f nF", value * 1e9);
    } else {
        snprintf(buffer, bufferSize, "%.2f pF", value * 1e12);
    }
	} else if (strcmp(unit, FREQUENCY_UNIT) == 0) {
    if (value < 1e3) {
      snprintf(buffer, bufferSize, "%.2f Hz", value);
    } else if (value < 1e6) {
      snprintf(buffer, bufferSize, "%.2f kHz", value / 1e3);
    } else {
      snprintf(buffer, bufferSize, "%.2f MHz", value / 1e6);
    }
  } else if (strcmp(unit, DUTY_CYCLE_UNIT) == 0) {
    snprintf(buffer, bufferSize, "%.2f %%", value);
  } else {
    snprintf(buffer, bufferSize, "%.2f %s", value, unit);
  }
}

/*Hien thi gia tri do*/
static void Menu_Measure_Display_Value(float value, const char* unit) {
  char buffer[17];
  Menu_Format_Value_Unit(buffer, sizeof(buffer), value, unit);
  Menu_Print_String(0, 1, buffer);
}

/*Ham hien thi cap nhat gia tri phat*/
static void Menu_Transmit_Display_Parameter_Update(uint32_t value, const char* unit) {
  char buffer[17];
  char valueStr[16];
  snprintf(valueStr, sizeof(valueStr), "%u %s", value, unit);
  int len = strlen(valueStr);
  int leftPadding = (14 - len) / 2;

  memset(buffer, ' ', sizeof(buffer));
  buffer[0] = '<';
  memcpy(&buffer[1 + leftPadding], valueStr, len);
  buffer[15] = '>';
  buffer[16] = '\0';

  Menu_Print_String(0, 1, buffer);
}

/*Ham hien thi cap nhat gia tri step*/
static void Menu_Transmit_Display_Step_Update(const char* label, uint16_t value) {
  char buffer[17];
  char valueStr[9];
  snprintf(valueStr, sizeof(valueStr), "%u", value);

  int len = strlen(valueStr);
  int leftPadding = (14 - len) / 2;

  Menu_Print_String(0, 0, label);

  memset(buffer, ' ', sizeof(buffer));
  buffer[0] = '<';
  memcpy(&buffer[1 + leftPadding], valueStr, len);
  buffer[15] = '>';
  buffer[16] = '\0';

  Menu_Print_String(0, 1, buffer);
}

static inline uint32_t Clamp_Update(uint32_t value, int8_t delta, uint32_t step, uint32_t minVal, uint32_t maxVal) {
  int32_t temp = (int32_t)value + (int32_t)delta * (int32_t)step;
  if (temp < (int32_t)minVal) temp = minVal;
  if (temp > (int32_t)maxVal) temp = maxVal;
  return (uint32_t)temp;
}

/*Ham dieu huong menu: Len hoac xuong*/
static void Menu_Handle_Navigation(Direction dir) {
  uint8_t max = 0;
  const char** menu = NULL;
  int8_t delta = (dir == DIRECTION_UP) ? 1 : -1;

  /* Handle adjustment modes */
  switch (currentMenu) {
    case MENU_FREQ_ADJUST:
      transmited_frequency = (uint32_t)Clamp_Update(transmited_frequency, delta, freq_step, MENU_FREQ_MIN_VAL, MENU_FREQ_MAX_VAL);
      Menu_Transmit_Display_Parameter_Update(transmited_frequency, FREQUENCY_UNIT);
      return;

    case MENU_DUTY_ADJUST:
      transmited_duty = (uint16_t)Clamp_Update(transmited_duty, delta, duty_step, MENU_DUTY_MIN_VAL, MENU_DUTY_MAX_VAL);
      Menu_Transmit_Display_Parameter_Update(transmited_duty, DUTY_CYCLE_UNIT);
      return;

    case MENU_SETTING_FREQ:
      freq_step = (uint16_t)Clamp_Update(freq_step, delta, MENU_FREQ_STEP, MENU_FREQ_ADJUST_MIN_VAL, MENU_FREQ_ADJUST_MAX_VAL);
      Menu_Transmit_Display_Step_Update(LCD_SETTING_FREQ_STEP, freq_step);
      return;

    case MENU_SETTING_DUTY:
      duty_step = (uint8_t)Clamp_Update(duty_step, delta, MENU_DUTY_STEP, MENU_DUTY_ADJUST_MIN_VAL, MENU_DUTY_ADJUST_MAX_VAL);
      Menu_Transmit_Display_Step_Update(LCD_SETTING_DUTY_STEP, duty_step);
      return;

    default:
      break;
  }

  if (Menu_Static(currentMenu)) return;

  /* Find menu items based on current menu level */
  for (uint8_t i = 0; i < MENU_COUNT; ++i) {
    if (menuTable[i].level == currentMenu) {
      menu = menuTable[i].items;
      max = menuTable[i].itemCount;
      break;
    }
  }

  /*Neu khong co menu hop le*/
  if (!menu || max == 0) {
    Menu_Change(MENU_MAIN, mainMenu, 2);
    return;
  }

  if (dir == DIRECTION_UP) {
    if (currentIndex == 0) {
      currentIndex = max - 1;
    } else {
      currentIndex = currentIndex - 1;
    }
  } else {
    currentIndex = (currentIndex + 1) % max;
  }

  Menu_Display_General(menu, max, currentIndex, 0);
}

static void Render_Current_Menu(void) {
  switch (currentMenu) {
    case MENU_MAIN:
      Menu_Display_General(mainMenu, 2, currentIndex, menuNeedClear);
      break;
    case MENU_MEASURE:
      Menu_Display_General(measureMenu, 3, currentIndex, menuNeedClear);
      break;
    case MENU_MEASURE_SINGLE:
      Menu_Display_General(measureSingleMenu, 5, currentIndex, menuNeedClear);
      break;
    case MENU_TRANSMIT:
      Menu_Display_General(transmitMenu, 4, currentIndex, menuNeedClear);
      break;
    default:
      break;
  }
  menuNeedClear = 0;
}

static uint8_t Handle_Transmit_Settings(void) {
  if (currentMenu == MENU_TRANSMIT && currentIndex == 2) {
    currentMenu = MENU_SETTING_FREQ;
    Menu_Transmit_Display_Step_Update(LCD_SETTING_FREQ_STEP, freq_step);
    return 1;
  }
  if (currentMenu == MENU_SETTING_FREQ) {
    currentMenu = MENU_SETTING_DUTY;
    Menu_Transmit_Display_Step_Update(LCD_SETTING_DUTY_STEP, duty_step);
    return 1;
  }
  if (currentMenu == MENU_SETTING_DUTY) {
    Flash_Save();
    Menu_Change(MENU_TRANSMIT, transmitMenu, 4);
    return 1;
  }
  return 0;
}

static void Handle_Main_Menu(void) {
  currentMenu = (currentIndex == 0) ? MENU_MEASURE : MENU_TRANSMIT;
  currentIndex = 0;
}

static void Handle_Measure_Menu(void) {
  switch (currentIndex) {
    case 0: Menu_Change(MENU_MEASURE_SINGLE, measureSingleMenu, 5); break;
    case 1:
      currentMenu = MENU_MEASURE_ALL;
      measureAllStep = 1;
      Menu_Display_Title(LCD_RESISTOR_MEASURE);
      Adc_Enable();
      break;
    default:
      Menu_Change(MENU_MAIN, mainMenu, 2);
      break;
  }
}

static uint8_t Handle_Measure_Single_Menu(void) {
  switch (currentIndex) {
    case 0:
      currentMenu = MENU_MEASURE_RESISTOR;
      Menu_Display_Title(LCD_RESISTOR_MEASURE);
      Adc_Enable();
      return 1;
    case 1:
      currentMenu = MENU_MEASURE_CAPACITOR;
      Menu_Display_Title(LCD_CAPACITOR_MEASURE);
      Adc_Enable();
      Menu_Print_String(0, 1, "Press Charge");
      Gpio_Button_Reset_Once(BUTTON_CHARGE);
      Gpio_Button_Enable(BUTTON_CHARGE);
      Measure_Charge_Pin_Init();
      return 1;
    case 2:
      currentMenu = MENU_MEASURE_FREQUENCY;
      Menu_Display_Title(LCD_FREQ_MEASURE);
      Pwm_Output_Enable();
      Pwm_Input_Enable();
      return 1;
    case 3:
      currentMenu = MENU_MEASURE_DUTY;
      Menu_Display_Title(LCD_DUTY_MEASURE);
      Pwm_Output_Enable();
      Pwm_Input_Enable();
      return 1;
    case 4:
      Menu_Change(MENU_MEASURE, measureMenu, 3);
      return 1;
  }
  return 0;
}

static uint8_t Handle_Measure_All_Mode(void) {
  measureAllStep++;
  switch (measureAllStep) {
    case 1:
      Menu_Display_Title(LCD_RESISTOR_MEASURE);
      break;
    case 2:
      Menu_Display_Title(LCD_CAPACITOR_MEASURE);
      Menu_Print_String(0, 1, "Press Charge");
      Gpio_Button_Reset_Once(BUTTON_CHARGE);
      Gpio_Button_Enable(BUTTON_CHARGE);
      Measure_Charge_Pin_Init();
      break;
    case 3:
      Menu_Display_Title(LCD_FREQ_MEASURE);
      Adc_Disable();
      Pwm_Output_Enable();
      Pwm_Input_Enable();
      Gpio_Button_Disable(BUTTON_CHARGE);
      Measure_Charge_Pin_DeInit();
      break;
    case 4:
      Menu_Display_Title(LCD_DUTY_MEASURE);
      break;
    default:
      measureAllStep = 0;
      Menu_Change(MENU_MEASURE, measureMenu, 3);
      Pwm_Output_Disable();
      Pwm_Input_Disable();
      return 1;
  }
  return 1;
}

static uint8_t Handle_Transmit_Menu(void) {
  switch (currentIndex) {
    case 0:
      currentMenu = MENU_FREQ_ADJUST;
      Menu_Display_Title(LCD_FREQ_TRANSMIT);
      Menu_Transmit_Display_Parameter_Update(transmited_frequency, FREQUENCY_UNIT);
      return 1;
    case 1:
      currentMenu = MENU_DUTY_ADJUST;
      Menu_Display_Title(LCD_DUTY_TRANSMIT);
      Menu_Transmit_Display_Parameter_Update(transmited_duty, DUTY_CYCLE_UNIT);
      return 1;
    case 2:
      currentMenu = MENU_SETTING_FREQ;
      currentIndex = 0;
      return 0;
    case 3:
      currentIndex = 1;
      Menu_Change(MENU_MAIN, mainMenu, 2);
      return 1;
  }
  return 0;
}

static void Handle_Adjustment_Exit(void) {
  Flash_Save();
  Transmit_Freq_Duty(transmited_frequency, transmited_duty);
  Menu_Change(MENU_TRANSMIT, transmitMenu, 4);
}

static void Handle_Measure_Exit(void) {
  if (currentMenu == MENU_MEASURE_RESISTOR) Adc_Disable();
  if (currentMenu == MENU_MEASURE_CAPACITOR) {
    Adc_Disable();
    Gpio_Button_Disable(BUTTON_CHARGE);
    Measure_Charge_Pin_DeInit();
  }
  if (currentMenu == MENU_MEASURE_FREQUENCY || currentMenu == MENU_MEASURE_DUTY) {
    Pwm_Output_Disable();
    Pwm_Input_Disable();
  }
  Menu_Change(MENU_MEASURE_SINGLE, measureSingleMenu, 5);
}

static void Update_Resistor(void) {
  static uint32_t lastTime = 0;
  uint32_t currentTime = SysTick_Get_Tick();

  Measure_Resistor();
  if (currentTime - lastTime > MENU_TIME_UPDATE_MEASURE) {
    Menu_Measure_Display_Value(measured_resistor, RESISTOR_UNIT);
    lastTime = currentTime;
  }
}

static void Update_Capacitor(void) {
  Measure_Capacitor();
  if (measure_cap_done) {
    measure_cap_done = 0;
    Menu_Measure_Display_Value(measured_capacitance, CAPACITOR_UNIT);
  }
}

static void Update_Frequency(void) {
  static uint32_t lastTime = 0;
  uint32_t currentTime = SysTick_Get_Tick();

  Measure_Freq_Duty_Apdative();
  if (currentTime - lastTime > MENU_TIME_UPDATE_MEASURE) {
    Menu_Measure_Display_Value(measured_frequency, FREQUENCY_UNIT);
    lastTime = currentTime;
  }
}

static void Update_Duty(void) {
  static uint32_t lastTime = 0;
  uint32_t currentTime = SysTick_Get_Tick();

  Measure_Freq_Duty_Apdative();
  if (currentTime - lastTime > MENU_TIME_UPDATE_MEASURE) {
    Menu_Measure_Display_Value(measured_duty, DUTY_CYCLE_UNIT);
    lastTime = currentTime;
  }
}

void Menu_Init(void) {
  Menu_Display_Project_Title();
  Menu_Change(MENU_MAIN, mainMenu, 2);
}

void Menu_Handle_Up(void) {
  Menu_Handle_Navigation(DIRECTION_UP);
}

void Menu_Handle_Down(void) {
  Menu_Handle_Navigation(DIRECTION_DOWN);
}

void Menu_Handle_Select(void) {
  menuNeedClear = 1;

  if (Handle_Transmit_Settings()) return;

  switch (currentMenu) {
    case MENU_MAIN:
      Handle_Main_Menu();
      break;

    case MENU_MEASURE:
      Handle_Measure_Menu();
      break;

    case MENU_MEASURE_SINGLE:
      if (Handle_Measure_Single_Menu()) return;
      break;

    case MENU_MEASURE_ALL:
      if (Handle_Measure_All_Mode()) return;
      break;

    case MENU_TRANSMIT:
      if (Handle_Transmit_Menu()) return;
      break;

    case MENU_FREQ_ADJUST:
    case MENU_DUTY_ADJUST:
      Handle_Adjustment_Exit();
      return;

    case MENU_MEASURE_RESISTOR:
    case MENU_MEASURE_CAPACITOR:
    case MENU_MEASURE_FREQUENCY:
    case MENU_MEASURE_DUTY:
      Handle_Measure_Exit();
      return;

    default:
      break;
  }

  Render_Current_Menu();
}

void Menu_Update_Measure(void) {
  switch (currentMenu) {
    case MENU_MEASURE_RESISTOR: Update_Resistor(); break;
    case MENU_MEASURE_CAPACITOR: Update_Capacitor(); break;
    case MENU_MEASURE_FREQUENCY: Update_Frequency(); break;
    case MENU_MEASURE_DUTY: Update_Duty(); break;

    case MENU_MEASURE_ALL:
      switch (measureAllStep) {
        case 1: Update_Resistor(); break;
        case 2: Update_Capacitor(); break;
        case 3: Update_Frequency(); break;
        case 4: Update_Duty(); break;
        default: break;
      }
      break;

    default:
      break;
  }
}

/*Update
	Scrollbar                     done
	Che do demo menu              no
	Flash                         done
	Bat tat ngoai vi khi can      done
	Toi uu chi tiet tung thu vien done
 */

