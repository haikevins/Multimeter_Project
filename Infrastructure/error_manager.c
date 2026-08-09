#include "error_manager.h"
#include "debug_logger.h"
#include "system_time.h"
#include <string.h>

typedef struct
{
  Error_Record_t record;
  uint8_t logged;
} Error_History_Entry_t;

static Error_History_Entry_t error_history[ERROR_MANAGER_HISTORY_SIZE];
static uint8_t error_write_index = 0U;
static uint8_t error_count = 0U;
static uint32_t error_total_count = 0U;

static Debug_Level_t ErrorManager_Debug_Level(Error_Severity_t severity)
{
  if (severity == ERROR_SEVERITY_WARNING) {
    return DEBUG_LEVEL_WARN;
  }
  return DEBUG_LEVEL_ERROR;
}

static void ErrorManager_Log_Entry(Error_History_Entry_t* entry)
{
  if (entry == 0 || entry->logged || !DebugLogger_IsReady()) {
    return;
  }

  if (entry->record.repeat_count > 1U) {
    DebugLogger_Log(ErrorManager_Debug_Level(entry->record.severity),
              ErrorManager_Source_String(entry->record.source),
              "0x%04X %s x%u",
              (unsigned int)entry->record.code,
              ErrorManager_Code_String(entry->record.code),
              (unsigned int)entry->record.repeat_count);
  } else {
    DebugLogger_Log(ErrorManager_Debug_Level(entry->record.severity),
              ErrorManager_Source_String(entry->record.source),
              "0x%04X %s",
              (unsigned int)entry->record.code,
              ErrorManager_Code_String(entry->record.code));
  }

  entry->logged = 1U;
}

void ErrorManager_Init(void)
{
  memset(error_history, 0, sizeof(error_history));
  error_write_index = 0U;
  error_count = 0U;
  error_total_count = 0U;
}

void ErrorManager_Report(Error_Source_t source,
                         Error_Code_t code,
                         Error_Severity_t severity)
{
  Error_History_Entry_t* entry;
  uint8_t last_index;
  uint32_t now;

  if (code == ERROR_CODE_NONE) {
    return;
  }

  error_total_count++;
  now = SystemTime_GetTick();

  /* Coalesce a short burst of identical errors so a failed peripheral cannot
     flood UART. The same error occurring later is logged as a new event. */
  if (error_count > 0U) {
    last_index = (error_write_index == 0U)
                   ? (ERROR_MANAGER_HISTORY_SIZE - 1U)
                   : (uint8_t)(error_write_index - 1U);
    entry = &error_history[last_index];

    if (entry->record.source == source &&
        entry->record.code == code &&
        entry->record.severity == severity &&
        (uint32_t)(now - entry->record.timestamp_ms) <= ERROR_MANAGER_COALESCE_MS) {
      if (entry->record.repeat_count < 0xFFFFU) {
        entry->record.repeat_count++;
      }
      return;
    }
  }

  entry = &error_history[error_write_index];
  entry->record.source = source;
  entry->record.code = code;
  entry->record.severity = severity;
  entry->record.timestamp_ms = now;
  entry->record.repeat_count = 1U;
  entry->logged = 0U;

  error_write_index = (uint8_t)((error_write_index + 1U) % ERROR_MANAGER_HISTORY_SIZE);
  if (error_count < ERROR_MANAGER_HISTORY_SIZE) {
    error_count++;
  }

  ErrorManager_Log_Entry(entry);
}

void ErrorManager_Flush(void)
{
  uint8_t i;
  uint8_t index;

  if (!DebugLogger_IsReady() || error_count == 0U) {
    return;
  }

  index = (error_count < ERROR_MANAGER_HISTORY_SIZE)
            ? 0U
            : error_write_index;

  for (i = 0U; i < error_count; ++i) {
    ErrorManager_Log_Entry(&error_history[index]);
    index = (uint8_t)((index + 1U) % ERROR_MANAGER_HISTORY_SIZE);
  }
}

void ErrorManager_Clear(void)
{
  ErrorManager_Init();
}

uint8_t ErrorManager_Get_Last(Error_Record_t* record)
{
  uint8_t index;

  if (record == 0 || error_count == 0U) {
    return 0U;
  }

  index = (error_write_index == 0U)
            ? (ERROR_MANAGER_HISTORY_SIZE - 1U)
            : (uint8_t)(error_write_index - 1U);
  *record = error_history[index].record;
  return 1U;
}

uint32_t ErrorManager_Get_Total_Count(void)
{
  return error_total_count;
}

const char* ErrorManager_Source_String(Error_Source_t source)
{
  switch (source) {
    case ERROR_SOURCE_SYSTEM:     return "SYS";
    case ERROR_SOURCE_FLASH:      return "FLASH";
    case ERROR_SOURCE_I2C_LCD:    return "LCD";
    case ERROR_SOURCE_UART:       return "UART";
    case ERROR_SOURCE_ADC:        return "ADC";
    case ERROR_SOURCE_BUTTON:     return "BUTTON";
    case ERROR_SOURCE_MEASURE:    return "MEASURE";
    case ERROR_SOURCE_PWM_INPUT:  return "PWM_IN";
    case ERROR_SOURCE_PWM_OUTPUT: return "PWM_OUT";
    default:                      return "UNKNOWN";
  }
}

const char* ErrorManager_Severity_String(Error_Severity_t severity)
{
  switch (severity) {
    case ERROR_SEVERITY_WARNING: return "WARN";
    case ERROR_SEVERITY_ERROR:   return "ERROR";
    case ERROR_SEVERITY_FATAL:   return "FATAL";
    default:                     return "UNKNOWN";
  }
}

const char* ErrorManager_Code_String(Error_Code_t code)
{
  switch (code) {
    case ERROR_CODE_FLASH_MIGRATED_V0: return "MIGRATED_V0";
    case ERROR_CODE_FLASH_BAD_MAGIC:   return "BAD_MAGIC";
    case ERROR_CODE_FLASH_BAD_VERSION: return "BAD_VERSION";
    case ERROR_CODE_FLASH_BAD_SIZE:    return "BAD_SIZE";
    case ERROR_CODE_FLASH_BAD_CRC:     return "BAD_CRC";
    case ERROR_CODE_FLASH_BAD_RANGE:   return "BAD_RANGE";
    case ERROR_CODE_FLASH_ERASE:       return "ERASE_FAILED";
    case ERROR_CODE_FLASH_PROGRAM:     return "PROGRAM_FAILED";
    case ERROR_CODE_FLASH_VERIFY:      return "VERIFY_FAILED";

    case ERROR_CODE_LCD_NOT_READY:     return "NOT_READY";
    case ERROR_CODE_LCD_BUS_BUSY:      return "BUS_BUSY";
    case ERROR_CODE_LCD_START_TIMEOUT: return "START_TIMEOUT";
    case ERROR_CODE_LCD_ADDR_TIMEOUT:  return "ADDR_TIMEOUT";
    case ERROR_CODE_LCD_NACK:          return "NACK";
    case ERROR_CODE_LCD_DATA_TIMEOUT:  return "DATA_TIMEOUT";
    case ERROR_CODE_LCD_BUS:           return "BUS_ERROR";
    case ERROR_CODE_LCD_NOT_FOUND:     return "NOT_FOUND";

    case ERROR_CODE_UART_TX_TIMEOUT:          return "TX_TIMEOUT";
    case ERROR_CODE_BUTTON_QUEUE_FULL:        return "QUEUE_FULL";
    case ERROR_CODE_ADC_CONVERSION_TIMEOUT:   return "CONVERSION_TIMEOUT";
    case ERROR_CODE_PWM_CAPTURE_UNAVAILABLE:  return "CAPTURE_UNAVAILABLE";
    case ERROR_CODE_PWM_INVALID_CAPTURE:      return "INVALID_CAPTURE";
    case ERROR_CODE_NONE:                     return "NONE";
    default:                           return "UNKNOWN";
  }
}
