#ifndef UI_VIEW_H
#define UI_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "ui_formatter.h"

typedef enum
{
  UI_VIEW_MESSAGE_EMPTY = 0,
  UI_VIEW_MESSAGE_MEASURING,
  UI_VIEW_MESSAGE_PRESS_CHARGE,
  UI_VIEW_MESSAGE_DISCHARGING,
  UI_VIEW_MESSAGE_CHARGING,
  UI_VIEW_MESSAGE_NO_SIGNAL,
  UI_VIEW_MESSAGE_NO_RESISTOR,
  UI_VIEW_MESSAGE_NO_CAPACITOR,
  UI_VIEW_MESSAGE_ERROR
} UiView_Message_t;

void UiView_ShowSplash(const char* title, const char* subtitle);
void UiView_ShowList(const char* const* items,
                     uint8_t total_items,
                     uint8_t selected_index,
                     uint8_t clear_first);
void UiView_ShowTitle(const char* title);
void UiView_ShowMessage(UiView_Message_t message);
void UiView_ShowMeasurementValue(float value, UiFormatter_Unit_t unit);
void UiView_ShowAdjustParameter(uint32_t value, const char* unit);
void UiView_ShowStep(const char* label, uint16_t value);
void UiView_ShowSaveFeedback(const char* title, uint8_t success);

#ifdef __cplusplus
}
#endif

#endif /* UI_VIEW_H */
