#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  DEBUG_LEVEL_ERROR = 0,
  DEBUG_LEVEL_WARN,
  DEBUG_LEVEL_INFO,
  DEBUG_LEVEL_TRACE
} Debug_Level_t;

#ifndef DEBUG_LOG_LEVEL
#define DEBUG_LOG_LEVEL DEBUG_LEVEL_INFO
#endif

void DebugLogger_SetReady(uint8_t ready);
uint8_t DebugLogger_IsReady(void);
void DebugLogger_Log(Debug_Level_t level, const char* module, const char* fmt, ...);
const char* DebugLogger_LevelString(Debug_Level_t level);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_LOGGER_H */
