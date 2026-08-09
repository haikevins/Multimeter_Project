#include "ui_controller.h"
#include "ui_view.h"
#include "ui_formatter.h"
#include "button_driver.h"
#include "system_time.h"
#include "measurement_service.h"
#include "signal_generator_service.h"
#include "config_service.h"

#define PROJECT_TITLE "Multimeter"
#define PROJECT_NAME "D22  Project"

#define LCD_MENU_MEASURE "Measure"
#define LCD_MENU_TRANSMIT "Transmit"
#define LCD_MEASURE_SINGLE "Single Mode"
#define LCD_MEASURE_ALL "All Mode"
#define LCD_TRANSMIT_FREQ "Frequency"
#define LCD_TRANSMIT_DUTY "Duty Cycle"
#define LCD_TRANSMIT_SETTING "Setting"
#define LCD_SINGLE_RESISTOR "Resistor"
#define LCD_SINGLE_CAPACITOR "Capacitor"
#define LCD_SINGLE_FREQ "Frequency"
#define LCD_SINGLE_DUTY "Duty Cycle"
#define LCD_RESISTOR_MEASURE "Resistor:"
#define LCD_CAPACITOR_MEASURE "Capacitor:"
#define LCD_FREQ_MEASURE "Frequency:"
#define LCD_DUTY_MEASURE "Duty Cycle:"
#define LCD_FREQ_TRANSMIT "Frequency:"
#define LCD_DUTY_TRANSMIT "Duty Cycle:"
#define LCD_SETTING_FREQ_STEP "Fre Step:"
#define LCD_SETTING_DUTY_STEP "Duty Step:"
#define LCD_MENU_BACK "Back"

#define MENU_FREQ_STEP_INCREMENT 100U
#define MENU_DUTY_STEP_INCREMENT 1U
#define MENU_MEASURE_REFRESH_MS 300U
#define MENU_RESISTOR_REFRESH_MS 1000U
#define MENU_RESISTOR_CHANGE_REL 0.005f
#define MENU_RESISTOR_CHANGE_ABS 0.10f
#define MENU_FREQUENCY_CHANGE_REL 0.002f
#define MENU_FREQUENCY_CHANGE_ABS 0.50f
#define MENU_DUTY_CHANGE_ABS 0.10f
#define MENU_SAVE_FEEDBACK_MS 800U
#define MENU_SPLASH_DURATION_MS 2000U

typedef enum {
  MENU_SPLASH = 0, MENU_MAIN, MENU_MEASURE, MENU_MEASURE_SINGLE, MENU_MEASURE_ALL,
  MENU_MEASURE_RESISTOR, MENU_MEASURE_CAPACITOR, MENU_MEASURE_FREQUENCY, MENU_MEASURE_DUTY,
  MENU_TRANSMIT, MENU_FREQ_ADJUST, MENU_DUTY_ADJUST, MENU_SETTING_FREQ, MENU_SETTING_DUTY,
  MENU_SAVE_FEEDBACK
} MenuLevel;

typedef enum { DIRECTION_UP, DIRECTION_DOWN } Direction;

typedef struct { MenuLevel level; const char* const* items; uint8_t itemCount; } MenuData;
typedef struct { uint8_t index; uint8_t lastContentIndex; } MenuCursorSlot_t;
typedef struct { MenuCursorSlot_t main, measure, measureSingle, transmit; } MenuCursorMemory_t;

static MenuCursorMemory_t cursorMemory = {0};
static uint8_t menuNeedClear = 1U;
static uint8_t measureAllStep = 0U;
static MenuLevel currentMenu = MENU_SPLASH;
static uint32_t splashStartTick = 0U;
static Measurement_Status_t lastMeasurementStatus = MEASUREMENT_STATUS_IDLE;
static uint8_t measurementStatusInitialized = 0U;
static uint8_t measurementValueInitialized = 0U;
static float lastDisplayedMeasurementValue = 0.0f;
static uint32_t lastMeasurementRenderTick = 0U;

/* Resistor display intentionally follows the original firmware's sample-and-hold
 * behaviour: measurement may run continuously, but a numeric value is only
 * committed to the LCD after READY has remained stable for a full display
 * period. This prevents one transient READY sample from flashing a number over
 * "No Resistor". */
static uint8_t resistorReadyHoldActive = 0U;
static uint32_t resistorReadySinceTick = 0U;
static uint32_t resistorLastDisplayTick = 0U;
static MenuLevel saveFeedbackReturnMenu = MENU_MAIN;
static uint32_t saveFeedbackStartTick = 0U;
static uint8_t saveFeedbackSuccess = 0U;
static const char* saveFeedbackTitle = "Setting:";

static void Menu_Handle_Up(void);
static void Menu_Handle_Down(void);
static void Menu_Handle_Select(void);
static void Menu_Transition(MenuLevel newMenu);
static void Menu_Start_Save_Feedback(const char* title, MenuLevel returnMenu, uint8_t saveOk);
static void Menu_Update_Save_Feedback(void);
static void Menu_Update_Splash(void);

static const char* mainMenu[] = { LCD_MENU_MEASURE, LCD_MENU_TRANSMIT };
static const char* measureMenu[] = { LCD_MEASURE_SINGLE, LCD_MEASURE_ALL, LCD_MENU_BACK };
static const char* transmitMenu[] = { LCD_TRANSMIT_FREQ, LCD_TRANSMIT_DUTY, LCD_TRANSMIT_SETTING, LCD_MENU_BACK };
static const char* measureSingleMenu[] = { LCD_SINGLE_RESISTOR, LCD_SINGLE_CAPACITOR, LCD_SINGLE_FREQ, LCD_SINGLE_DUTY, LCD_MENU_BACK };

static const MenuData menuTable[] = {
  { MENU_MAIN, mainMenu, 2U }, { MENU_MEASURE, measureMenu, 3U },
  { MENU_TRANSMIT, transmitMenu, 4U }, { MENU_MEASURE_SINGLE, measureSingleMenu, 5U }
};
#define MENU_COUNT (sizeof(menuTable) / sizeof(menuTable[0]))

static uint8_t Menu_Static(MenuLevel level)
{
  return (level == MENU_MEASURE_RESISTOR || level == MENU_MEASURE_CAPACITOR ||
          level == MENU_MEASURE_FREQUENCY || level == MENU_MEASURE_DUTY ||
          level == MENU_MEASURE_ALL) ? 1U : 0U;
}

static void Menu_Render_Measurement_Status(Measurement_Status_t status)
{
  switch (status) {
    case MEASUREMENT_STATUS_MEASURING:
      UiView_ShowMessage(UI_VIEW_MESSAGE_MEASURING);
      break;
    case MEASUREMENT_STATUS_WAIT_CHARGE:
      UiView_ShowMessage(UI_VIEW_MESSAGE_PRESS_CHARGE);
      break;
    case MEASUREMENT_STATUS_DISCHARGING:
      UiView_ShowMessage(UI_VIEW_MESSAGE_DISCHARGING);
      break;
    case MEASUREMENT_STATUS_CHARGING:
      UiView_ShowMessage(UI_VIEW_MESSAGE_CHARGING);
      break;
    case MEASUREMENT_STATUS_NO_SIGNAL:
      UiView_ShowMessage(UI_VIEW_MESSAGE_NO_SIGNAL);
      break;
    case MEASUREMENT_STATUS_NO_COMPONENT:
      /* Component-specific text is rendered by resistor/capacitor update. */
      break;
    case MEASUREMENT_STATUS_ERROR:
      UiView_ShowMessage(UI_VIEW_MESSAGE_ERROR);
      break;
    case MEASUREMENT_STATUS_IDLE:
      UiView_ShowMessage(UI_VIEW_MESSAGE_EMPTY);
      break;
    case MEASUREMENT_STATUS_READY:
    default:
      break;
  }
}

static void Menu_Reset_Measure_Status(void) {
  measurementStatusInitialized = 0U;
  lastMeasurementStatus = MEASUREMENT_STATUS_IDLE;
  measurementValueInitialized = 0U;
  lastDisplayedMeasurementValue = 0.0f;
  lastMeasurementRenderTick = 0U;
  resistorReadyHoldActive = 0U;
  resistorReadySinceTick = 0U;
  resistorLastDisplayTick = 0U;
}

static float Menu_Abs_Float(float value) {
  return (value < 0.0f) ? -value : value;
}

/* Decide whether writing a new numeric value to LCD is useful.
 * Status changes are rendered immediately elsewhere. Numeric values are
 * rate-limited to protect the LCD/I2C path and are redrawn only after the
 * value changed enough compared with the last value actually shown. */
static uint8_t Menu_Measure_Should_Render_Value(float value,
                                                uint32_t currentTime,
                                                uint32_t refreshMs,
                                                float relativeThreshold,
                                                float absoluteThreshold,
                                                uint8_t forceRender) {
  float delta;
  float threshold;

  if (forceRender || !measurementValueInitialized) {
    measurementValueInitialized = 1U;
    lastDisplayedMeasurementValue = value;
    lastMeasurementRenderTick = currentTime;
    return 1U;
  }

  if ((uint32_t)(currentTime - lastMeasurementRenderTick) < refreshMs) {
    return 0U;
  }

  delta = Menu_Abs_Float(value - lastDisplayedMeasurementValue);
  threshold = Menu_Abs_Float(lastDisplayedMeasurementValue) * relativeThreshold;
  if (threshold < absoluteThreshold) {
    threshold = absoluteThreshold;
  }

  if (delta < threshold) {
    return 0U;
  }

  lastDisplayedMeasurementValue = value;
  lastMeasurementRenderTick = currentTime;
  return 1U;
}

static uint8_t Menu_Measure_Status_Changed(Measurement_Status_t status) {
  if (!measurementStatusInitialized || status != lastMeasurementStatus) {
    lastMeasurementStatus = status;
    measurementStatusInitialized = 1U;
    return 1U;
  }
  return 0U;
}

static void Menu_Start_Save_Feedback(const char* title, MenuLevel returnMenu, uint8_t saveOk) {
  saveFeedbackTitle = (title != NULL) ? title : "Setting:";
  saveFeedbackReturnMenu = returnMenu;
  saveFeedbackSuccess = saveOk ? 1U : 0U;
  saveFeedbackStartTick = SystemTime_GetTick();

  Menu_Transition(MENU_SAVE_FEEDBACK);
}

static void Menu_Update_Save_Feedback(void) {
  uint32_t now = SystemTime_GetTick();

  if ((uint32_t)(now - saveFeedbackStartTick) >= MENU_SAVE_FEEDBACK_MS) {
    Menu_Transition(saveFeedbackReturnMenu);
  }
}

static void Menu_Update_Splash(void) {
  uint32_t now = SystemTime_GetTick();

  if ((uint32_t)(now - splashStartTick) >= MENU_SPLASH_DURATION_MS) {
    Menu_Transition(MENU_MAIN);
  }
}

/* Return number of selectable items for list-style menu states. */
static uint8_t Menu_Get_Item_Count(MenuLevel level) {
  for (uint8_t i = 0; i < MENU_COUNT; ++i) {
    if (menuTable[i].level == level) {
      return menuTable[i].itemCount;
    }
  }
  return 0;
}

/* Each list-style menu owns its cursor position. Child states never overwrite
 * their parent's selection, so returning to a menu restores the last item. */
static MenuCursorSlot_t* Menu_Get_Cursor_Slot(MenuLevel level) {
  switch (level) {
    case MENU_MAIN: return &cursorMemory.main;
    case MENU_MEASURE: return &cursorMemory.measure;
    case MENU_MEASURE_SINGLE: return &cursorMemory.measureSingle;
    case MENU_TRANSMIT: return &cursorMemory.transmit;
    default: return NULL;
  }
}

static uint8_t Menu_Has_Back_Item(MenuLevel level) {
  return (level == MENU_MEASURE ||
          level == MENU_MEASURE_SINGLE ||
          level == MENU_TRANSMIT) ? 1U : 0U;
}

static uint8_t Menu_Get_Cursor(MenuLevel level) {
  MenuCursorSlot_t* slot = Menu_Get_Cursor_Slot(level);
  uint8_t itemCount = Menu_Get_Item_Count(level);

  if (slot == NULL || itemCount == 0U) {
    return 0U;
  }

  if (slot->index >= itemCount) {
    slot->index = 0U;
  }
  if (slot->lastContentIndex >= itemCount) {
    slot->lastContentIndex = 0U;
  }

  return slot->index;
}

static void Menu_Set_Cursor(MenuLevel level, uint8_t index) {
  MenuCursorSlot_t* slot = Menu_Get_Cursor_Slot(level);
  uint8_t itemCount = Menu_Get_Item_Count(level);

  if (slot == NULL || itemCount == 0U) {
    return;
  }

  slot->index = (index < itemCount) ? index : 0U;

  /* Back is a navigation command, not a useful resume position. */
  if (!Menu_Has_Back_Item(level) || slot->index != (uint8_t)(itemCount - 1U)) {
    slot->lastContentIndex = slot->index;
  }
}

static void Menu_Restore_Content_Cursor(MenuLevel level) {
  MenuCursorSlot_t* slot = Menu_Get_Cursor_Slot(level);
  uint8_t itemCount = Menu_Get_Item_Count(level);

  if (slot == NULL || itemCount == 0U || !Menu_Has_Back_Item(level)) {
    return;
  }

  if (slot->index == (uint8_t)(itemCount - 1U)) {
    slot->index = (slot->lastContentIndex < (uint8_t)(itemCount - 1U))
                    ? slot->lastContentIndex
                    : 0U;
  }
}

/* Enter one sub-state of All Measure mode. */
static void Menu_MeasureAll_EnterStep(uint8_t step) {
  switch (step) {
    case 1:
      UiView_ShowTitle(LCD_RESISTOR_MEASURE);
      MeasurementService_SetMode(MEASUREMENT_MODE_RESISTOR, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case 2:
      UiView_ShowTitle(LCD_CAPACITOR_MEASURE);
      ButtonDriver_Enable(BUTTON_CHARGE);
      MeasurementService_SetMode(MEASUREMENT_MODE_CAPACITOR, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case 3:
      UiView_ShowTitle(LCD_FREQ_MEASURE);
      MeasurementService_SetMode(MEASUREMENT_MODE_FREQUENCY_DUTY, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case 4:
      UiView_ShowTitle(LCD_DUTY_MEASURE);
      MeasurementService_SetMode(MEASUREMENT_MODE_FREQUENCY_DUTY, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    default:
      break;
  }
}

/* Exit one sub-state of All Measure mode.
 * Resources shared by the next step are intentionally kept enabled:
 * step 1 -> 2 keeps ADC, step 3 -> 4 keeps PWM capture/output.
 */
static void Menu_MeasureAll_ExitStep(uint8_t step) {
  switch (step) {
    case 2:
      ButtonDriver_Disable(BUTTON_CHARGE);
      break;

    case 4:
      MeasurementService_SetMode(MEASUREMENT_MODE_NONE, ConfigService_GetFrequency());
      break;

    default:
      break;
  }
}

/* Release resources owned by the state being left. */
static void Menu_Exit_State(MenuLevel state) {
  switch (state) {
    case MENU_MEASURE_RESISTOR:
      MeasurementService_SetMode(MEASUREMENT_MODE_NONE, ConfigService_GetFrequency());
      break;

    case MENU_MEASURE_CAPACITOR:
      MeasurementService_SetMode(MEASUREMENT_MODE_NONE, ConfigService_GetFrequency());
      ButtonDriver_Disable(BUTTON_CHARGE);
      break;

    case MENU_MEASURE_FREQUENCY:
    case MENU_MEASURE_DUTY:
      MeasurementService_SetMode(MEASUREMENT_MODE_NONE, ConfigService_GetFrequency());
      break;

    case MENU_MEASURE_ALL:
      MeasurementService_SetMode(MEASUREMENT_MODE_NONE, ConfigService_GetFrequency());
      if (measureAllStep == 2U) {
        ButtonDriver_Disable(BUTTON_CHARGE);
      }
      measureAllStep = 0U;
      break;

    default:
      break;
  }
}

/* Configure resources and render the state being entered. */
static void Menu_Enter_State(MenuLevel state) {
  switch (state) {
    case MENU_SPLASH:
      UiView_ShowSplash(PROJECT_TITLE, PROJECT_NAME);
      splashStartTick = SystemTime_GetTick();
      break;

    case MENU_MAIN:
      UiView_ShowList(mainMenu, 2, Menu_Get_Cursor(MENU_MAIN), menuNeedClear);
      break;

    case MENU_MEASURE:
      UiView_ShowList(measureMenu, 3, Menu_Get_Cursor(MENU_MEASURE), menuNeedClear);
      break;

    case MENU_MEASURE_SINGLE:
      UiView_ShowList(measureSingleMenu, 5, Menu_Get_Cursor(MENU_MEASURE_SINGLE), menuNeedClear);
      break;

    case MENU_MEASURE_ALL:
      measureAllStep = 1;
      Menu_MeasureAll_EnterStep(measureAllStep);
      break;

    case MENU_MEASURE_RESISTOR:
      UiView_ShowTitle(LCD_RESISTOR_MEASURE);
      MeasurementService_SetMode(MEASUREMENT_MODE_RESISTOR, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case MENU_MEASURE_CAPACITOR:
      UiView_ShowTitle(LCD_CAPACITOR_MEASURE);
      ButtonDriver_Enable(BUTTON_CHARGE);
      MeasurementService_SetMode(MEASUREMENT_MODE_CAPACITOR, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case MENU_MEASURE_FREQUENCY:
      UiView_ShowTitle(LCD_FREQ_MEASURE);
      MeasurementService_SetMode(MEASUREMENT_MODE_FREQUENCY_DUTY, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case MENU_MEASURE_DUTY:
      UiView_ShowTitle(LCD_DUTY_MEASURE);
      MeasurementService_SetMode(MEASUREMENT_MODE_FREQUENCY_DUTY, ConfigService_GetFrequency());
      Menu_Reset_Measure_Status();
      break;

    case MENU_TRANSMIT:
      UiView_ShowList(transmitMenu, 4, Menu_Get_Cursor(MENU_TRANSMIT), menuNeedClear);
      break;

    case MENU_FREQ_ADJUST:
      UiView_ShowTitle(LCD_FREQ_TRANSMIT);
      UiView_ShowAdjustParameter(ConfigService_GetFrequency(), "Hz");
      break;

    case MENU_DUTY_ADJUST:
      UiView_ShowTitle(LCD_DUTY_TRANSMIT);
      UiView_ShowAdjustParameter(ConfigService_GetDuty(), "%");
      break;

    case MENU_SETTING_FREQ:
      UiView_ShowStep(LCD_SETTING_FREQ_STEP, ConfigService_GetFrequencyStep());
      break;

    case MENU_SETTING_DUTY:
      UiView_ShowStep(LCD_SETTING_DUTY_STEP, ConfigService_GetDutyStep());
      break;

    case MENU_SAVE_FEEDBACK:
      UiView_ShowSaveFeedback(saveFeedbackTitle, saveFeedbackSuccess);
      break;

    default:
      break;
  }
}

/* The only function that changes currentMenu.
 * Every transition is: Exit old state -> switch state -> Enter new state.
 */
static void Menu_Transition(MenuLevel newMenu) {
  Menu_Exit_State(currentMenu);
  currentMenu = newMenu;

  /* Clamp only the cursor owned by the destination menu. */
  (void)Menu_Get_Cursor(currentMenu);

  menuNeedClear = 1;
  Menu_Enter_State(currentMenu);
  menuNeedClear = 0;
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
  uint8_t currentIndex;
  const char* const* menu = NULL;
  int8_t delta = (dir == DIRECTION_UP) ? 1 : -1;

  /* Handle adjustment modes */
  switch (currentMenu) {
    case MENU_FREQ_ADJUST:
      ConfigService_SetFrequency(Clamp_Update(ConfigService_GetFrequency(), delta,
                                            ConfigService_GetFrequencyStep(),
                                            CONFIG_SERVICE_FREQ_MIN_HZ,
                                            CONFIG_SERVICE_FREQ_MAX_HZ));
      UiView_ShowAdjustParameter(ConfigService_GetFrequency(), "Hz");
      return;

    case MENU_DUTY_ADJUST:
      ConfigService_SetDuty((uint16_t)Clamp_Update(ConfigService_GetDuty(), delta,
                                                ConfigService_GetDutyStep(),
                                                CONFIG_SERVICE_DUTY_MIN_PERCENT,
                                                CONFIG_SERVICE_DUTY_MAX_PERCENT));
      UiView_ShowAdjustParameter(ConfigService_GetDuty(), "%");
      return;

    case MENU_SETTING_FREQ:
      ConfigService_SetFrequencyStep((uint16_t)Clamp_Update(ConfigService_GetFrequencyStep(), delta,
                                                          MENU_FREQ_STEP_INCREMENT,
                                                          CONFIG_SERVICE_FREQ_STEP_MIN_HZ,
                                                          CONFIG_SERVICE_FREQ_STEP_MAX_HZ));
      UiView_ShowStep(LCD_SETTING_FREQ_STEP, ConfigService_GetFrequencyStep());
      return;

    case MENU_SETTING_DUTY:
      ConfigService_SetDutyStep((uint8_t)Clamp_Update(ConfigService_GetDutyStep(), delta,
                                                    MENU_DUTY_STEP_INCREMENT,
                                                    CONFIG_SERVICE_DUTY_STEP_MIN,
                                                    CONFIG_SERVICE_DUTY_STEP_MAX));
      UiView_ShowStep(LCD_SETTING_DUTY_STEP, ConfigService_GetDutyStep());
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
    Menu_Transition(MENU_MAIN);
    return;
  }

  currentIndex = Menu_Get_Cursor(currentMenu);

  if (dir == DIRECTION_UP) {
    if (currentIndex == 0) {
      currentIndex = max - 1;
    } else {
      currentIndex = currentIndex - 1;
    }
  } else {
    currentIndex = (currentIndex + 1) % max;
  }

  Menu_Set_Cursor(currentMenu, currentIndex);
  UiView_ShowList(menu, max, currentIndex, 0);
}

static uint8_t Handle_Transmit_Settings(void) {
  uint8_t saveOk;

  if (currentMenu == MENU_SETTING_FREQ) {
    saveOk = ConfigService_Save();
    Menu_Start_Save_Feedback(LCD_SETTING_FREQ_STEP,
                             saveOk ? MENU_SETTING_DUTY : MENU_SETTING_FREQ,
                             saveOk);
    return 1;
  }

  if (currentMenu == MENU_SETTING_DUTY) {
    saveOk = ConfigService_Save();
    Menu_Start_Save_Feedback(LCD_SETTING_DUTY_STEP,
                             saveOk ? MENU_TRANSMIT : MENU_SETTING_DUTY,
                             saveOk);
    return 1;
  }

  return 0;
}

static void Handle_Main_Menu(void) {
  uint8_t selectedIndex = Menu_Get_Cursor(MENU_MAIN);
  MenuLevel nextMenu = (selectedIndex == 0U) ? MENU_MEASURE : MENU_TRANSMIT;
  Menu_Transition(nextMenu);
}

static void Handle_Measure_Menu(void) {
  switch (Menu_Get_Cursor(MENU_MEASURE)) {
    case 0:
      Menu_Transition(MENU_MEASURE_SINGLE);
      break;

    case 1:
      Menu_Transition(MENU_MEASURE_ALL);
      break;

    default:
      Menu_Restore_Content_Cursor(MENU_MEASURE);
      Menu_Transition(MENU_MAIN);
      break;
  }
}

static uint8_t Handle_Measure_Single_Menu(void) {
  switch (Menu_Get_Cursor(MENU_MEASURE_SINGLE)) {
    case 0:
      Menu_Transition(MENU_MEASURE_RESISTOR);
      return 1;

    case 1:
      Menu_Transition(MENU_MEASURE_CAPACITOR);
      return 1;

    case 2:
      Menu_Transition(MENU_MEASURE_FREQUENCY);
      return 1;

    case 3:
      Menu_Transition(MENU_MEASURE_DUTY);
      return 1;

    case 4:
      Menu_Restore_Content_Cursor(MENU_MEASURE_SINGLE);
      Menu_Transition(MENU_MEASURE);
      return 1;
  }
  return 0;
}

static uint8_t Handle_Measure_All_Mode(void) {
  uint8_t nextStep;

  Menu_MeasureAll_ExitStep(measureAllStep);
  nextStep = measureAllStep + 1;

  if (nextStep <= 4) {
    measureAllStep = nextStep;
    Menu_MeasureAll_EnterStep(measureAllStep);
  } else {
    /* Step 4 already released PWM in Menu_MeasureAll_ExitStep(). */
    measureAllStep = 0;
    Menu_Transition(MENU_MEASURE);
  }

  return 1;
}

static uint8_t Handle_Transmit_Menu(void) {
  switch (Menu_Get_Cursor(MENU_TRANSMIT)) {
    case 0:
      Menu_Transition(MENU_FREQ_ADJUST);
      return 1;

    case 1:
      Menu_Transition(MENU_DUTY_ADJUST);
      return 1;

    case 2:
      Menu_Transition(MENU_SETTING_FREQ);
      return 1;

    case 3:
      Menu_Restore_Content_Cursor(MENU_TRANSMIT);
      Menu_Transition(MENU_MAIN);
      return 1;
  }
  return 0;
}

static void Handle_Adjustment_Exit(void) {
  MenuLevel sourceMenu = currentMenu;
  uint8_t saveOk = ConfigService_Save();
  const char* title = (sourceMenu == MENU_FREQ_ADJUST) ? LCD_FREQ_TRANSMIT : LCD_DUTY_TRANSMIT;

  SignalGeneratorService_Apply(ConfigService_GetFrequency(), (uint8_t)ConfigService_GetDuty());
  Menu_Start_Save_Feedback(title, saveOk ? MENU_TRANSMIT : sourceMenu, saveOk);
}

static void Handle_Measure_Exit(void) {
  Menu_Transition(MENU_MEASURE_SINGLE);
}

static void Update_Resistor(void) {
  uint32_t currentTime = SystemTime_GetTick();
  Measurement_Status_t status;
  uint8_t statusChanged;
  float value;

  MeasurementService_Process();
  status = MeasurementService_GetStatus();
  statusChanged = Menu_Measure_Status_Changed(status);

  /* No component/error/measurement states always cancel the numeric hold.
   * Most importantly, NO_COMPONENT remains visually latched; a transient READY
   * cannot immediately overwrite it with a number. */
  if (status != MEASUREMENT_STATUS_READY) {
    resistorReadyHoldActive = 0U;
    resistorReadySinceTick = 0U;

    if (statusChanged) {
      if (status == MEASUREMENT_STATUS_NO_COMPONENT) {
        UiView_ShowMessage(UI_VIEW_MESSAGE_NO_RESISTOR);
      } else {
        Menu_Render_Measurement_Status(status);
      }
    }
    return;
  }

  /* Original-style sample-and-hold. The service may calculate R as fast as it
   * wants, but the LCD only accepts a value after READY has been continuous for
   * one full 1-second period. This deliberately removes statusChanged as a
   * force-render condition. */
  if (!resistorReadyHoldActive) {
    resistorReadyHoldActive = 1U;
    resistorReadySinceTick = currentTime;
    return;
  }

  if ((uint32_t)(currentTime - resistorReadySinceTick) < MENU_RESISTOR_REFRESH_MS) {
    return;
  }

  if ((resistorLastDisplayTick != 0U) &&
      ((uint32_t)(currentTime - resistorLastDisplayTick) < MENU_RESISTOR_REFRESH_MS)) {
    return;
  }

  value = MeasurementService_GetResult(MEASUREMENT_RESULT_RESISTOR);
  UiView_ShowMeasurementValue(value, UI_FORMATTER_UNIT_RESISTANCE);

  resistorLastDisplayTick = currentTime;
  resistorReadySinceTick = currentTime;
  measurementValueInitialized = 1U;
  lastDisplayedMeasurementValue = value;
  lastMeasurementRenderTick = currentTime;
}

static void Update_Capacitor(void) {
  Measurement_Status_t status;
  uint8_t statusChanged;
  uint8_t done;

  MeasurementService_Process();
  done = MeasurementService_TakeCapacitorDone();
  status = MeasurementService_GetStatus();
  statusChanged = Menu_Measure_Status_Changed(status);

  if (status == MEASUREMENT_STATUS_READY) {
    if (statusChanged || done) {
      UiView_ShowMeasurementValue(MeasurementService_GetResult(MEASUREMENT_RESULT_CAPACITANCE), UI_FORMATTER_UNIT_CAPACITANCE);
    }
    return;
  }

  if (statusChanged) {
    if (status == MEASUREMENT_STATUS_NO_COMPONENT) {
      UiView_ShowMessage(UI_VIEW_MESSAGE_NO_CAPACITOR);
    } else {
      Menu_Render_Measurement_Status(status);
    }
  }
}

static void Update_Frequency(void) {
  uint32_t currentTime = SystemTime_GetTick();
  Measurement_Status_t status;
  uint8_t statusChanged;
  float value;

  MeasurementService_Process();
  status = MeasurementService_GetStatus();
  statusChanged = Menu_Measure_Status_Changed(status);

  if (status != MEASUREMENT_STATUS_READY) {
    if (statusChanged) {
      Menu_Render_Measurement_Status(status);
    }
    return;
  }

  value = MeasurementService_GetResult(MEASUREMENT_RESULT_FREQUENCY);
  if (Menu_Measure_Should_Render_Value(value, currentTime,
                                       MENU_MEASURE_REFRESH_MS,
                                       MENU_FREQUENCY_CHANGE_REL,
                                       MENU_FREQUENCY_CHANGE_ABS,
                                       statusChanged)) {
    UiView_ShowMeasurementValue(value, UI_FORMATTER_UNIT_FREQUENCY);
  }
}

static void Update_Duty(void) {
  uint32_t currentTime = SystemTime_GetTick();
  Measurement_Status_t status;
  uint8_t statusChanged;
  float value;

  MeasurementService_Process();
  status = MeasurementService_GetStatus();
  statusChanged = Menu_Measure_Status_Changed(status);

  if (status != MEASUREMENT_STATUS_READY) {
    if (statusChanged) {
      Menu_Render_Measurement_Status(status);
    }
    return;
  }

  value = MeasurementService_GetResult(MEASUREMENT_RESULT_DUTY);
  if (Menu_Measure_Should_Render_Value(value, currentTime,
                                       MENU_MEASURE_REFRESH_MS,
                                       0.0f,
                                       MENU_DUTY_CHANGE_ABS,
                                       statusChanged)) {
    UiView_ShowMeasurementValue(value, UI_FORMATTER_UNIT_DUTY);
  }
}

void UiController_Init(void) {
  cursorMemory.main.index = 0U;
  cursorMemory.main.lastContentIndex = 0U;
  cursorMemory.measure.index = 0U;
  cursorMemory.measure.lastContentIndex = 0U;
  cursorMemory.measureSingle.index = 0U;
  cursorMemory.measureSingle.lastContentIndex = 0U;
  cursorMemory.transmit.index = 0U;
  cursorMemory.transmit.lastContentIndex = 0U;
  currentMenu = MENU_SPLASH;
  menuNeedClear = 1U;
  Menu_Enter_State(MENU_SPLASH);
  menuNeedClear = 0U;
}

void UiController_HandleEvent(ButtonEvent_t event) {
  if (currentMenu == MENU_SPLASH || currentMenu == MENU_SAVE_FEEDBACK) {
    return;
  }

  switch (event) {
    case BUTTON_EVENT_UP_PRESSED:
      Menu_Handle_Up();
      break;

    case BUTTON_EVENT_DOWN_PRESSED:
      Menu_Handle_Down();
      break;

    case BUTTON_EVENT_SELECT_PRESSED:
      Menu_Handle_Select();
      break;

    case BUTTON_EVENT_CHARGE_PRESSED:
      /* CHARGE is emitted only while capacitor measurement owns the button. */
      MeasurementService_StartCapacitor();
      lastMeasurementStatus = MeasurementService_GetStatus();
      measurementStatusInitialized = 1U;
      Menu_Render_Measurement_Status(lastMeasurementStatus);
      break;

    case BUTTON_EVENT_NONE:
    default:
      break;
  }
}

static void Menu_Handle_Up(void) {
  Menu_Handle_Navigation(DIRECTION_UP);
}

static void Menu_Handle_Down(void) {
  Menu_Handle_Navigation(DIRECTION_DOWN);
}

static void Menu_Handle_Select(void) {
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

}

void UiController_Update(void) {
  switch (currentMenu) {
    case MENU_SPLASH: Menu_Update_Splash(); break;
    case MENU_MEASURE_RESISTOR: Update_Resistor(); break;
    case MENU_MEASURE_CAPACITOR: Update_Capacitor(); break;
    case MENU_MEASURE_FREQUENCY: Update_Frequency(); break;
    case MENU_MEASURE_DUTY: Update_Duty(); break;
    case MENU_SAVE_FEEDBACK: Menu_Update_Save_Feedback(); break;

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

