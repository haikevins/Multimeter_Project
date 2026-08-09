#include "debug_logger.h"
#include "system_time.h"
#include "uart_port.h"
#include <stdarg.h>
#include <stdio.h>

#define DEBUG_LOG_MESSAGE_SIZE 96U
#define DEBUG_LOG_LINE_SIZE    144U

static uint8_t debug_log_ready = 0U;

void DebugLogger_SetReady(uint8_t ready)
{
  debug_log_ready = (ready != 0U) ? 1U : 0U;
}

uint8_t DebugLogger_IsReady(void)
{
  return debug_log_ready;
}

const char* DebugLogger_LevelString(Debug_Level_t level)
{
  switch (level) {
    case DEBUG_LEVEL_ERROR: return "ERROR";
    case DEBUG_LEVEL_WARN:  return "WARN";
    case DEBUG_LEVEL_INFO:  return "INFO";
    case DEBUG_LEVEL_TRACE: return "TRACE";
    default:                return "UNK";
  }
}

void DebugLogger_Log(Debug_Level_t level, const char* module, const char* fmt, ...)
{
  char message[DEBUG_LOG_MESSAGE_SIZE];
  char line[DEBUG_LOG_LINE_SIZE];
  va_list args;
  int written;

  if (!debug_log_ready || level > DEBUG_LOG_LEVEL || fmt == 0) {
    return;
  }

  if (module == 0) {
    module = "SYS";
  }

  va_start(args, fmt);
  (void)vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  written = snprintf(line,
                     sizeof(line),
                     "[%010lu][%s][%s] %s\r\n",
                     (unsigned long)SystemTime_GetTick(),
                     DebugLogger_LevelString(level),
                     module,
                     message);

  if (written > 0) {
    UartPort_SendString(line);
  }
}
