#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "button_driver.h"

void UiController_Init(void);
void UiController_HandleEvent(ButtonEvent_t event);
void UiController_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONTROLLER_H */
