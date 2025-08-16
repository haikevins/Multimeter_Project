/**
  ******************************************************************************
  * @file    menu.h
  * @author  Nguyen Ngoc Hai
  * @brief   Header for menu control logic (LCD navigation, data display)
  * @version 1.0
  * @date    25-April-2025
  ******************************************************************************
*/

#ifndef __MENU_H__
#define __MENU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

/* Menu Overview -------------------------------------------------------------
MAIN MENU 
+----------------+
¦ > Measure      ¦  -> currentIndex = 0
¦   Transmit     ¦
+----------------+
				| DOWN
				v
+----------------+
¦   Measure      ¦
¦ > Transmit     ¦  -> currentIndex = 1
+----------------+

MEASURE MENU
+----------------+
¦ > Single Mode  ¦  -> currentIndex = 0
¦   All Mode     ¦
+----------------+
				| DOWN
				v
+----------------+
¦   Single Mode  ¦
¦ > All Mode     ¦  -> currentIndex = 1
+----------------+
				| DOWN
				v
+----------------+
¦   All Mode     ¦
¦ > Back         ¦  -> currentIndex = 2
+----------------+

SINGLE MEASURE MENU
+----------------+
¦ > Resistor     ¦  -> currentIndex = 0
¦   Capacitor    ¦
+----------------+
				| DOWN
				v
+----------------+
¦   Capacitor    ¦
¦ > Frequency    ¦  -> currentIndex = 2
+----------------+
				| DOWN
				v
+----------------+
¦   Duty Cycle   ¦
¦ > Back         ¦  -> currentIndex = 4
+----------------+

MEASURE ALL MENU
+----------------+
¦ Resistor:      ¦   <- Step 1
¦ 1.23 Ohm       ¦
+----------------+
				| SELECT
				v
+----------------+
¦ Capacitor:     ¦   <- Step 2
¦ 0.45 uF        ¦
+----------------+
				| SELECT
				v
+----------------+
¦ Frequency:     ¦   <- Step 3
¦ 1234.00 Hz     ¦
+----------------+
				| SELECT
				v
+----------------+
¦ Duty Cycle:    ¦   <- Step 4
¦ 58.33 %        ¦
+----------------+

TRANSMIT MENU
+----------------+
¦ > Frequency    ¦  -> currentIndex = 0
¦   Duty Cycle   ¦
+----------------+
				| DOWN
				v
+----------------+
¦   Duty Cycle   ¦
¦ > Setting      ¦  -> currentIndex = 2
+----------------+
				| DOWN
				v
+----------------+
¦   Setting      ¦
¦ > Back         ¦  -> currentIndex = 3
+----------------+

FREQUENCY ADJUST MENU
+----------------+
¦ Frequency:     ¦   <- LCD_FREQ_TRANSMIT
¦ -   500 Hz   + ¦   <- UP/DOWN to change
+----------------+

measured_duty ADJUST MENU
+----------------+
¦ Duty Cycle:    ¦   <- LCD_DUTY_TRANSMIT
¦ -    30 %    + ¦   <- UP/DOWN to change
+----------------+

SETTING MENU
+----------------+
¦ Fre Step:      ¦
¦ -   100 Hz   + ¦
+----------------+
				| SELECT
				v
+----------------+
¦ Duty Step:     ¦
¦ -     1 %    + ¦
+----------------+

  Measure Menu (MENU_MEASURE)
    +-- Single Measure          MENU_MEASURE_SINGLE
    ¦   +-- Resistor            MENU_MEASURE_RESISTOR
    ¦   +-- Capacitor           MENU_MEASURE_CAPACITOR
    ¦   +-- Frequency           MENU_MEASURE_FREQUENCY
    ¦   +-- Duty Cycle          MENU_MEASURE_DUTY
    ¦   +-- Back                MENU_MEASURE
    +-- All Measure             MENU_MEASURE_ALL
    ¦   +-- Step 1: Resistor
    ¦							| select button
    ¦							v
    ¦   +-- Step 2: Capacitor
    ¦							| select button
    ¦							v
    ¦   +-- Step 3: Frequency
    ¦						  | select button
    ¦							V
    ¦   +-- Step 4: Duty Cycle
    ¦						  | select button
    ¦							v
    ¦   +-- Done ? Back to MENU_MEASURE
    +-- Back                    MENU_MAIN 

  Transmit Menu (MENU_TRANSMIT)
    +-- Frequency Adjust        MENU_FREQ_ADJUST
    ¦   +-- Up/Down to change frequency transmit
    ¦   +-- Select to back to TRANSMIT
    +-- Duty Cycle Adjust             MENU_DUTY_ADJUST
    ¦   +-- Up/Down to change duty transmit
    ¦   +-- Select to back to TRANSMIT
    +-- Settings
    ¦   +-- Set frequency step       MENU_SETTING_FREQ
    ¦   +-- Set duty Cycle step       MENU_SETTING_DUTY
    ¦       +-- Done ? Back to TRANSMIT
    +-- Back                    MENU_MAIN (currentIndex = 1 restored manually)

NOTES:
- `currentIndex` controls the cursor position within a menu.
-  When navigating back (e.g. from TRANSMIT to MAIN), you must **manually set currentIndex**
   if you want the cursor to return to the same item (e.g., Transmit).
- `Menu_Change()` is the central function for menu switching.
-  The LCD shows 2 lines: the selected item and one adjacent item (scrollable).
- `measureAllStep` is used to handle multi-step measurement in MENU_MEASURE_ALL.

TIPS:
-  Keep this map updated when adding/removing menu items.
-  Consider moving it into a separate documentation file for maintainability.
------------------------------------------------------------------------------*/

/* Project Title Display -----------------------------------------------------*/
#define PROJECT_TITLE "Multimeter"
#define PROJECT_NAME "D22  Project"

/* Main Menu -----------------------------------------------------------------*/
#define LCD_MENU_MEASURE "Measure"
#define LCD_MENU_TRANSMIT "Transmit"

/* Measure Menu --------------------------------------------------------------*/
#define LCD_MEASURE_SINGLE "Single Mode"
#define LCD_MEASURE_ALL "All Mode"

/* Transmit Menu -------------------------------------------------------------*/
#define LCD_TRANSMIT_FREQ "Frequency"
#define LCD_TRANSMIT_DUTY "Duty Cycle"
#define LCD_TRANSMIT_SETTING "Setting"

/* Single Measure Menu -------------------------------------------------------*/
#define LCD_SINGLE_RESISTOR "Resistor"
#define LCD_SINGLE_CAPACITOR "Capacitor"
#define LCD_SINGLE_FREQ "Frequency"
#define LCD_SINGLE_DUTY "Duty Cycle"

/* Measurement Display Titles ------------------------------------------------*/
#define LCD_RESISTOR_MEASURE "Resistor:"
#define LCD_CAPACITOR_MEASURE "Capacitor:"
#define LCD_FREQ_MEASURE "Frequency:"
#define LCD_DUTY_MEASURE "Duty Cycle:"

/* Transmit Adjustment Titles ------------------------------------------------*/
#define LCD_FREQ_TRANSMIT "Frequency:"
#define LCD_DUTY_TRANSMIT "Duty Cycle:"
#define LCD_SETTING_FREQ_STEP "Fre Step:"
#define LCD_SETTING_DUTY_STEP "Duty Step:"

/* Common Menu Entries -------------------------------------------------------*/
#define LCD_MENU_BACK "Back"

#define RESISTOR_UNIT "\x02"
#define CAPACITOR_UNIT "F"
#define FREQUENCY_UNIT "Hz"
#define DUTY_CYCLE_UNIT "%"

/* Menu Configuration Constants ----------------------------------------------*/
#define MENU_COUNT (sizeof(menuTable) / sizeof(menuTable[0]))

#define MENU_FREQ_MIN_VAL 1
#define MENU_FREQ_MAX_VAL 100000
#define MENU_DUTY_MIN_VAL 1
#define MENU_DUTY_MAX_VAL 100

#define MENU_FREQ_ADJUST_MIN_VAL 1
#define MENU_FREQ_ADJUST_MAX_VAL 10000
#define MENU_DUTY_ADJUST_MIN_VAL 1
#define MENU_DUTY_ADJUST_MAX_VAL 10

#define MENU_FREQ_STEP 100
#define MENU_DUTY_STEP 1

#define MENU_TIME_UPDATE_MEASURE 1000

  /* Menu Enumeration ----------------------------------------------------------*/
  typedef enum {
    MENU_MAIN = 0,

    MENU_MEASURE,
    MENU_MEASURE_SINGLE,
    MENU_MEASURE_ALL,

    MENU_MEASURE_RESISTOR,
    MENU_MEASURE_CAPACITOR,
    MENU_MEASURE_FREQUENCY,
    MENU_MEASURE_DUTY,

    MENU_TRANSMIT,
    MENU_FREQ_ADJUST,
    MENU_DUTY_ADJUST,
    MENU_SETTING_FREQ,
    MENU_SETTING_DUTY,
  } MenuLevel;

  /* Navigation Direction ------------------------------------------------------*/
  typedef enum {
    DIRECTION_UP,
    DIRECTION_DOWN
  } Direction;

  /* Menu Data Structure -------------------------------------------------------*/
  typedef struct
  {
    MenuLevel level;
    const char** items;
    uint8_t itemCount;
  } MenuData;

  /* Measurement Data Display Mapping ------------------------------------------*/
  typedef struct
  {
    MenuLevel level;
    const char* title;
    float* value;
    const char* unit;
  } MeasureDisplay;

  /* Adjustment Settings -------------------------------------------------------*/
  extern uint8_t duty_step;
  extern uint16_t freq_step;

  /* Menu Action ---------------------------------------------------------------*/
  void Menu_Init(void);
  void Menu_Handle_Up(void);
  void Menu_Handle_Down(void);
  void Menu_Handle_Select(void);
  void Menu_Update_Measure(void);
  void Menu_Print_String(uint8_t col, uint8_t row, const char* text);

#ifdef __cplusplus
}
#endif

#endif /* __MENU_H__ */

/************************ (C) COPYRIGHT Nguyen Ngoc Hai *****END OF FILE******/

