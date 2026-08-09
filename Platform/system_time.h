#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void SystemTime_Init(void);
void SystemTime_DelayMs(uint16_t time_ms);
uint32_t SystemTime_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_TIME_H */
