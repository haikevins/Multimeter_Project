#include "ui_formatter.h"
#include <stdio.h>
#include <string.h>

#define LCD_LINE_LENGTH 16U

void UiFormatter_FormatMeasurement(char* buffer,
                                   size_t buffer_size,
                                   float value,
                                   UiFormatter_Unit_t unit)
{
  if (buffer == NULL || buffer_size == 0U) {
    return;
  }

  switch (unit) {
    case UI_FORMATTER_UNIT_RESISTANCE:
      if (value >= 1e6f) {
        snprintf(buffer, buffer_size, "%.2f M\x02", value / 1e6f);
      } else if (value >= 1e3f) {
        snprintf(buffer, buffer_size, "%.2f k\x02", value / 1e3f);
      } else {
        snprintf(buffer, buffer_size, "%.2f \x02", value);
      }
      break;

    case UI_FORMATTER_UNIT_CAPACITANCE:
      if (value >= 1.0f) {
        snprintf(buffer, buffer_size, "%.2f F", value);
      } else if (value >= 1e-3f) {
        snprintf(buffer, buffer_size, "%.2f mF", value * 1e3f);
      } else if (value >= 1e-6f) {
        snprintf(buffer, buffer_size, "%.2f uF", value * 1e6f);
      } else if (value >= 1e-9f) {
        snprintf(buffer, buffer_size, "%.2f nF", value * 1e9f);
      } else {
        snprintf(buffer, buffer_size, "%.2f pF", value * 1e12f);
      }
      break;

    case UI_FORMATTER_UNIT_FREQUENCY:
      if (value < 1e3f) {
        snprintf(buffer, buffer_size, "%.2f Hz", value);
      } else if (value < 1e6f) {
        snprintf(buffer, buffer_size, "%.2f kHz", value / 1e3f);
      } else {
        snprintf(buffer, buffer_size, "%.2f MHz", value / 1e6f);
      }
      break;

    case UI_FORMATTER_UNIT_DUTY:
      snprintf(buffer, buffer_size, "%.2f %%", value);
      break;

    default:
      buffer[0] = '\0';
      break;
  }
}

void UiFormatter_FormatUnsignedWithUnit(char* buffer,
                                        size_t buffer_size,
                                        uint32_t value,
                                        const char* unit)
{
  if (buffer == NULL || buffer_size == 0U) {
    return;
  }

  if (unit == NULL || unit[0] == '\0') {
    snprintf(buffer, buffer_size, "%lu", (unsigned long)value);
  } else {
    snprintf(buffer, buffer_size, "%lu %s", (unsigned long)value, unit);
  }
}

void UiFormatter_FormatCenteredEditor(char* buffer,
                                      size_t buffer_size,
                                      const char* value_text)
{
  size_t len;
  size_t copy_len;
  size_t left_padding;

  if (buffer == NULL || buffer_size == 0U) {
    return;
  }

  if (buffer_size < (LCD_LINE_LENGTH + 1U)) {
    buffer[0] = '\0';
    return;
  }

  memset(buffer, ' ', LCD_LINE_LENGTH);
  buffer[LCD_LINE_LENGTH] = '\0';
  buffer[0] = '<';
  buffer[LCD_LINE_LENGTH - 1U] = '>';

  if (value_text == NULL) {
    return;
  }

  len = strlen(value_text);
  copy_len = (len > (LCD_LINE_LENGTH - 2U)) ? (LCD_LINE_LENGTH - 2U) : len;
  left_padding = ((LCD_LINE_LENGTH - 2U) - copy_len) / 2U;
  memcpy(&buffer[1U + left_padding], value_text, copy_len);
}
