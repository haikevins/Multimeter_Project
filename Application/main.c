#include "app_controller.h"

/* processing */
int main(void) {
  AppController_Init();
  while (1) {
    AppController_RunOnce();
  }
}
