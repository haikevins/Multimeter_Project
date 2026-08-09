#include "ui_view.h"
#include "lcd1602_driver.h"
#include <stdio.h>
#include <string.h>

#define LCD_LINE_LENGTH 16U

/* LCD shadow buffer. Every visible row is represented as exactly 16 chars.
 * This prevents stale characters and avoids repeatedly rewriting the same row. */
static char uiLineShadow[2][LCD_LINE_LENGTH + 1U];
static uint8_t uiLineShadowValid[2] = {0U, 0U};

static void UiView_ShadowSetBlank(void)
{
  uint8_t row;

  for (row = 0U; row < 2U; ++row) {
    memset(uiLineShadow[row], ' ', LCD_LINE_LENGTH);
    uiLineShadow[row][LCD_LINE_LENGTH] = '\0';
    uiLineShadowValid[row] = 1U;
  }
}

static void UiView_Clear(void)
{
  if (Lcd1602Driver_Clear() == LCD1602_OK) {
    UiView_ShadowSetBlank();
  } else {
    uiLineShadowValid[0] = 0U;
    uiLineShadowValid[1] = 0U;
  }
}

static void UiView_WriteChar(uint8_t col, uint8_t row, char value)
{
  if (row >= 2U || col >= LCD_LINE_LENGTH) {
    return;
  }

  if (uiLineShadowValid[row] && uiLineShadow[row][col] == value) {
    return;
  }

  if (Lcd1602Driver_SetCursor(col, row) != LCD1602_OK) {
    uiLineShadowValid[row] = 0U;
    return;
  }

  if (Lcd1602Driver_WriteChar(value) != LCD1602_OK) {
    uiLineShadowValid[row] = 0U;
    return;
  }

  if (!uiLineShadowValid[row]) {
    /* The rest of the row is unknown, so do not claim a complete shadow. */
    return;
  }

  uiLineShadow[row][col] = value;
}

static void UiView_WriteLine(uint8_t row, const char* text)
{
  char buffer[LCD_LINE_LENGTH + 1U];
  size_t length;

  if (row >= 2U) {
    return;
  }

  memset(buffer, ' ', LCD_LINE_LENGTH);
  buffer[LCD_LINE_LENGTH] = '\0';

  if (text != NULL) {
    length = strlen(text);
    if (length > LCD_LINE_LENGTH) {
      length = LCD_LINE_LENGTH;
    }
    memcpy(buffer, text, length);
  }

  if (uiLineShadowValid[row] &&
      memcmp(uiLineShadow[row], buffer, LCD_LINE_LENGTH) == 0) {
    return;
  }

  if (Lcd1602Driver_SetCursor(0U, row) != LCD1602_OK) {
    uiLineShadowValid[row] = 0U;
    return;
  }

  /* Always write exactly 16 display cells so a shorter status string can
   * never leave characters from the previous numeric value behind. */
  if (Lcd1602Driver_WriteString(buffer) != LCD1602_OK) {
    uiLineShadowValid[row] = 0U;
    return;
  }

  memcpy(uiLineShadow[row], buffer, LCD_LINE_LENGTH + 1U);
  uiLineShadowValid[row] = 1U;
}

void UiView_ShowSplash(const char* title, const char* subtitle)
{
  char line0[LCD_LINE_LENGTH + 1U];
  char line1[LCD_LINE_LENGTH + 1U];
  size_t len;

  memset(line0, ' ', LCD_LINE_LENGTH);
  memset(line1, ' ', LCD_LINE_LENGTH);
  line0[LCD_LINE_LENGTH] = '\0';
  line1[LCD_LINE_LENGTH] = '\0';

  if (title != NULL) {
    len = strlen(title);
    if (len > 13U) len = 13U;
    memcpy(&line0[3U], title, len);
  }
  if (subtitle != NULL) {
    len = strlen(subtitle);
    if (len > 14U) len = 14U;
    memcpy(&line1[2U], subtitle, len);
  }

  UiView_Clear();
  UiView_WriteLine(0U, line0);
  UiView_WriteLine(1U, line1);
}

void UiView_ShowList(const char* const* items,
                     uint8_t total_items,
                     uint8_t selected_index,
                     uint8_t clear_first)
{
  uint8_t top_index;
  uint8_t line;

  if (items == NULL || total_items == 0U) {
    return;
  }

  if (clear_first) {
    UiView_Clear();
  }

  if (selected_index >= total_items) {
    selected_index = 0U;
  }

  top_index = (selected_index == 0U) ? 0U : (uint8_t)(selected_index - 1U);

  for (line = 0U; line < 2U; ++line) {
    uint8_t index = (uint8_t)(top_index + line);
    char buffer[LCD_LINE_LENGTH + 1U];

    if (index < total_items) {
      char prefix = (index == selected_index) ? '>' : ' ';
      snprintf(buffer, sizeof(buffer), "%c%-15.15s", prefix, items[index]);
    } else {
      memset(buffer, ' ', LCD_LINE_LENGTH);
      buffer[LCD_LINE_LENGTH] = '\0';
    }

    UiView_WriteLine(line, buffer);
  }

  if (total_items > 2U) {
    UiView_WriteChar(15U, 0U, ' ');
    UiView_WriteChar(15U, 1U, ' ');

    if (top_index > 0U) {
      UiView_WriteChar(15U, 0U, 0);
    }
    if ((uint8_t)(top_index + 2U) < total_items) {
      UiView_WriteChar(15U, 1U, 1);
    }
  }
}

void UiView_ShowTitle(const char* title)
{
  UiView_Clear();
  UiView_WriteLine(0U, title);
}

void UiView_ShowMessage(UiView_Message_t message)
{
  const char* text = "";

  switch (message) {
    case UI_VIEW_MESSAGE_MEASURING:    text = "Measuring..."; break;
    case UI_VIEW_MESSAGE_PRESS_CHARGE: text = "Press Charge"; break;
    case UI_VIEW_MESSAGE_DISCHARGING:  text = "Discharging..."; break;
    case UI_VIEW_MESSAGE_CHARGING:     text = "Charging..."; break;
    case UI_VIEW_MESSAGE_NO_SIGNAL:    text = "No Signal"; break;
    case UI_VIEW_MESSAGE_NO_RESISTOR:  text = "No Resistor"; break;
    case UI_VIEW_MESSAGE_NO_CAPACITOR: text = "No Capacitor"; break;
    case UI_VIEW_MESSAGE_ERROR:        text = "Error"; break;
    case UI_VIEW_MESSAGE_EMPTY:
    default:                           text = ""; break;
  }

  UiView_WriteLine(1U, text);
}

void UiView_ShowMeasurementValue(float value, UiFormatter_Unit_t unit)
{
  char buffer[LCD_LINE_LENGTH + 1U];
  UiFormatter_FormatMeasurement(buffer, sizeof(buffer), value, unit);
  UiView_WriteLine(1U, buffer);
}

void UiView_ShowAdjustParameter(uint32_t value, const char* unit)
{
  char value_text[LCD_LINE_LENGTH + 1U];
  char line[LCD_LINE_LENGTH + 1U];

  UiFormatter_FormatUnsignedWithUnit(value_text, sizeof(value_text), value, unit);
  UiFormatter_FormatCenteredEditor(line, sizeof(line), value_text);
  UiView_WriteLine(1U, line);
}

void UiView_ShowStep(const char* label, uint16_t value)
{
  char value_text[LCD_LINE_LENGTH + 1U];
  char line[LCD_LINE_LENGTH + 1U];

  UiView_WriteLine(0U, label);
  UiFormatter_FormatUnsignedWithUnit(value_text, sizeof(value_text), value, "");
  UiFormatter_FormatCenteredEditor(line, sizeof(line), value_text);
  UiView_WriteLine(1U, line);
}

void UiView_ShowSaveFeedback(const char* title, uint8_t success)
{
  UiView_ShowTitle(title);
  UiView_WriteLine(1U, success ? "Saved!" : "Save Error");
}
