#ifndef UI_FORMATTER_H
#define UI_FORMATTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  UI_FORMATTER_UNIT_RESISTANCE = 0,
  UI_FORMATTER_UNIT_CAPACITANCE,
  UI_FORMATTER_UNIT_FREQUENCY,
  UI_FORMATTER_UNIT_DUTY
} UiFormatter_Unit_t;

void UiFormatter_FormatMeasurement(char* buffer,
                                   size_t buffer_size,
                                   float value,
                                   UiFormatter_Unit_t unit);

void UiFormatter_FormatUnsignedWithUnit(char* buffer,
                                        size_t buffer_size,
                                        uint32_t value,
                                        const char* unit);

void UiFormatter_FormatCenteredEditor(char* buffer,
                                      size_t buffer_size,
                                      const char* value_text);

#ifdef __cplusplus
}
#endif

#endif /* UI_FORMATTER_H */
