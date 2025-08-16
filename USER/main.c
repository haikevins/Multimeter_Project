#include "app.h"

/* processing */
int main(void) {
  App_Init();
  while (1) {
    App_Logic();
  }
}
