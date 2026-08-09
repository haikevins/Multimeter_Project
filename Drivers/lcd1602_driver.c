#include "lcd1602_driver.h"
#include "stm32f10x.h"
#include "debug_logger.h"
#include "error_manager.h"
#include "system_time.h"

static uint8_t i2c2_lcd_address = 0U; /**< LCD address in 8-bit write format after init. */

static uint8_t i2c2_lcd_ready = 0U;
static Lcd1602Driver_Status_t i2c2_lcd_last_error = LCD1602_ERROR_NOT_READY;

static Lcd1602Driver_Status_t I2C2_Send_Start(void);
static Lcd1602Driver_Status_t I2C2_Send_Address(uint8_t addr);
static Lcd1602Driver_Status_t I2C2_Send_Data(uint8_t data);
static Lcd1602Driver_Status_t I2C2_Send_Stop(void);
static Lcd1602Driver_Status_t I2C2_Scan_Address(uint8_t* address);
static Lcd1602Driver_Status_t I2C2_LCD_Write_byte(char data);
static Lcd1602Driver_Status_t I2C2_LCD_Data_Write(char data);
static Lcd1602Driver_Status_t I2C2_LCD_Control_Write(char data);
static Lcd1602Driver_Status_t I2C2_LCD_Create_Char(uint8_t location, uint8_t* charmap);
static Lcd1602Driver_Status_t I2C2_LCD_Load_Icons(void);

static uint8_t arrow_up[8] = {
  0b00100, 0b01110, 0b10101, 0b00100,
  0b00100, 0b00100, 0b00000, 0b00000
};

static uint8_t arrow_down[8] = {
  0b00100, 0b00100, 0b00100, 0b10101,
  0b01110, 0b00100, 0b00000, 0b00000
};

static uint8_t char_ohm[8] = {
  0b01110, 0b10001, 0b10001, 0b10001,
  0b01110, 0b01010, 0b11011, 0b00000
};

static Error_Code_t I2C2_LCD_Status_To_Error_Code(Lcd1602Driver_Status_t status) {
  switch (status) {
    case LCD1602_ERROR_NOT_READY:     return ERROR_CODE_LCD_NOT_READY;
    case LCD1602_ERROR_BUS_BUSY:      return ERROR_CODE_LCD_BUS_BUSY;
    case LCD1602_ERROR_START_TIMEOUT: return ERROR_CODE_LCD_START_TIMEOUT;
    case LCD1602_ERROR_ADDR_TIMEOUT:  return ERROR_CODE_LCD_ADDR_TIMEOUT;
    case LCD1602_ERROR_NACK:          return ERROR_CODE_LCD_NACK;
    case LCD1602_ERROR_DATA_TIMEOUT:  return ERROR_CODE_LCD_DATA_TIMEOUT;
    case LCD1602_ERROR_BUS:           return ERROR_CODE_LCD_BUS;
    case LCD1602_ERROR_NOT_FOUND:     return ERROR_CODE_LCD_NOT_FOUND;
    case LCD1602_OK:
    default:                           return ERROR_CODE_NONE;
  }
}

static void I2C2_LCD_Report_Error(Lcd1602Driver_Status_t status) {
  Error_Code_t code = I2C2_LCD_Status_To_Error_Code(status);
  Error_Severity_t severity;

  if (code == ERROR_CODE_NONE) {
    return;
  }

  severity = (status == LCD1602_ERROR_NOT_READY ||
              status == LCD1602_ERROR_NACK ||
              status == LCD1602_ERROR_NOT_FOUND)
               ? ERROR_SEVERITY_WARNING
               : ERROR_SEVERITY_ERROR;

  ErrorManager_Report(ERROR_SOURCE_I2C_LCD, code, severity);
}

static Lcd1602Driver_Status_t I2C2_LCD_Set_Error(Lcd1602Driver_Status_t status) {
  i2c2_lcd_last_error = status;

  /* NOT_READY is a consequence of a previously reported LCD failure. NACK is
     expected during address scan and is reported separately on runtime loss. */
  if (status != LCD1602_ERROR_NOT_READY &&
      status != LCD1602_ERROR_NACK) {
    I2C2_LCD_Report_Error(status);
  }

  return status;
}

static uint8_t I2C2_LCD_Timeout(uint32_t start_tick) {
  return ((uint32_t)(SystemTime_GetTick() - start_tick) >= LCD1602_TIMEOUT_MS);
}

static uint8_t I2C2_LCD_Has_Bus_Error(void) {
  return (I2C2->SR1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)) != 0U;
}

static void Lcd1602Driver_Clear_Error_Flags(void) {
  I2C2->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR | I2C_SR1_AF);
}

static Lcd1602Driver_Status_t I2C2_LCD_Abort(Lcd1602Driver_Status_t status) {
  uint8_t was_ready = i2c2_lcd_ready;

  I2C2->CR1 |= I2C_CR1_STOP;
  Lcd1602Driver_Clear_Error_Flags();
  i2c2_lcd_ready = 0U;

  /* NACK during bus scan is normal. NACK after the LCD was already ready means
     a runtime disconnect/failure and must be visible to Error Manager. */
  if (status == LCD1602_ERROR_NACK && was_ready) {
    I2C2_LCD_Report_Error(status);
  }

  return I2C2_LCD_Set_Error(status);
}

const char *Lcd1602Driver_StatusString(Lcd1602Driver_Status_t status) {
  switch (status) {
    case LCD1602_OK:                  return "OK";
    case LCD1602_ERROR_NOT_READY:     return "NOT_READY";
    case LCD1602_ERROR_BUS_BUSY:      return "BUS_BUSY";
    case LCD1602_ERROR_START_TIMEOUT: return "START_TIMEOUT";
    case LCD1602_ERROR_ADDR_TIMEOUT:  return "ADDR_TIMEOUT";
    case LCD1602_ERROR_NACK:          return "NACK";
    case LCD1602_ERROR_DATA_TIMEOUT:  return "DATA_TIMEOUT";
    case LCD1602_ERROR_BUS:           return "BUS_ERROR";
    case LCD1602_ERROR_NOT_FOUND:     return "NOT_FOUND";
    default:                           return "UNKNOWN";
  }
}

uint8_t Lcd1602Driver_IsReady(void) {
  return i2c2_lcd_ready;
}

Lcd1602Driver_Status_t Lcd1602Driver_GetLastError(void) {
  return i2c2_lcd_last_error;
}

static Lcd1602Driver_Status_t I2C2_Send_Start(void) {
  uint32_t start_tick = SystemTime_GetTick();

  /* Do not create START while the bus is permanently busy. */
  while (I2C2->SR2 & I2C_SR2_BUSY) {
    if (I2C2_LCD_Has_Bus_Error()) {
      return I2C2_LCD_Abort(LCD1602_ERROR_BUS);
    }
    if (I2C2_LCD_Timeout(start_tick)) {
      return I2C2_LCD_Abort(LCD1602_ERROR_BUS_BUSY);
    }
  }

  I2C2->CR1 |= I2C_CR1_START;
  start_tick = SystemTime_GetTick();

  while (!(I2C2->SR1 & I2C_SR1_SB)) {
    if (I2C2_LCD_Has_Bus_Error()) {
      return I2C2_LCD_Abort(LCD1602_ERROR_BUS);
    }
    if (I2C2_LCD_Timeout(start_tick)) {
      return I2C2_LCD_Abort(LCD1602_ERROR_START_TIMEOUT);
    }
  }

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_Send_Address(uint8_t addr) {
  uint32_t start_tick;

  I2C2->DR = addr;
  start_tick = SystemTime_GetTick();

  while (!(I2C2->SR1 & I2C_SR1_ADDR)) {
    if (I2C2->SR1 & I2C_SR1_AF) {
      I2C2->SR1 &= ~I2C_SR1_AF;
      return I2C2_LCD_Abort(LCD1602_ERROR_NACK);
    }
    if (I2C2_LCD_Has_Bus_Error()) {
      return I2C2_LCD_Abort(LCD1602_ERROR_BUS);
    }
    if (I2C2_LCD_Timeout(start_tick)) {
      return I2C2_LCD_Abort(LCD1602_ERROR_ADDR_TIMEOUT);
    }
  }

  (void)I2C2->SR2; /* Read SR2 after SR1 to clear ADDR. */
  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_Send_Data(uint8_t data) {
  uint32_t start_tick;

  I2C2->DR = data;
  start_tick = SystemTime_GetTick();

  /* BTF guarantees the byte has actually left the data register/shift register. */
  while (!(I2C2->SR1 & I2C_SR1_BTF)) {
    if (I2C2->SR1 & I2C_SR1_AF) {
      I2C2->SR1 &= ~I2C_SR1_AF;
      return I2C2_LCD_Abort(LCD1602_ERROR_NACK);
    }
    if (I2C2_LCD_Has_Bus_Error()) {
      return I2C2_LCD_Abort(LCD1602_ERROR_BUS);
    }
    if (I2C2_LCD_Timeout(start_tick)) {
      return I2C2_LCD_Abort(LCD1602_ERROR_DATA_TIMEOUT);
    }
  }

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_Send_Stop(void) {
  I2C2->CR1 |= I2C_CR1_STOP;
  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_Scan_Address(uint8_t *address) {
  static const uint8_t ranges[][2] = {
    {0x20U, 0x27U}, /* PCF8574 */
    {0x38U, 0x3FU}  /* PCF8574A */
  };
  uint8_t range;
  uint8_t addr;
  Lcd1602Driver_Status_t status;

  if (address == 0) {
    return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_FOUND);
  }

  *address = 0U;

  DebugLogger_Log(DEBUG_LEVEL_INFO, "LCD", "Scanning I2C2 addresses");

  for (range = 0U; range < 2U; range++) {
    for (addr = ranges[range][0]; addr <= ranges[range][1]; addr++) {
      /* Scan is allowed before lcd_ready becomes true. */
      status = I2C2_Send_Start();
      if (status != LCD1602_OK) {
        return status;
      }

      status = I2C2_Send_Address((uint8_t)(addr << 1));
      if (status == LCD1602_OK) {
        I2C2_Send_Stop();
        *address = addr;
        DebugLogger_Log(DEBUG_LEVEL_INFO, "LCD", "Found at 0x%02X", addr);
        return I2C2_LCD_Set_Error(LCD1602_OK);
      }

      if (status == LCD1602_ERROR_NACK) {
        /* NACK is normal while scanning: continue with next candidate. */
        SystemTime_DelayMs(1);
        continue;
      }

      /* Timeout/bus error is not a normal "address not present" result. */
      return status;
    }
  }

  return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_FOUND);
}

Lcd1602Driver_Status_t Lcd1602Driver_Init(void) {
  uint8_t detected_address = 0U;
  Lcd1602Driver_Status_t status;

  i2c2_lcd_ready = 0U;
  i2c2_lcd_address = 0U;
  i2c2_lcd_last_error = LCD1602_ERROR_NOT_READY;

  RCC->APB1ENR |= (1U << 22); /* I2C2 clock */
  RCC->APB2ENR |= (1U << 3);  /* GPIOB clock */

  /* PB10 = SCL, PB11 = SDA: alternate-function open-drain, 50 MHz. */
  GPIOB->CRH &= ~((0xFU << 8) | (0xFU << 12));
  GPIOB->CRH |=  ((0xBU << 8) | (0xBU << 12));

  /* Reset I2C2 peripheral. */
  I2C2->CR1 |= I2C_CR1_SWRST;
  I2C2->CR1 &= ~I2C_CR1_SWRST;

  /* APB1 = 36 MHz, standard mode 100 kHz. */
  I2C2->CR2 = 36U & 0x3FU;
  I2C2->CCR = 180U;
  I2C2->TRISE = 37U;
  I2C2->OAR1 = (1U << 14);
  I2C2->CR1 = I2C_CR1_PE;

  Lcd1602Driver_Clear_Error_Flags();

  status = I2C2_Scan_Address(&detected_address);
  if (status != LCD1602_OK) {
    return status;
  }

  i2c2_lcd_address = (uint8_t)(detected_address << 1);
  i2c2_lcd_ready = 1U;

#define LCD_INIT_CMD(cmd, delay_ms)                  \
  do {                                               \
    status = I2C2_LCD_Control_Write((char)(cmd));   \
    if (status != LCD1602_OK) return status;       \
    SystemTime_DelayMs((delay_ms));                            \
  } while (0)

  LCD_INIT_CMD(0x33U, 10U);
  LCD_INIT_CMD(0x32U, 50U);
  LCD_INIT_CMD(0x28U, 50U);
  LCD_INIT_CMD(0x01U, 50U);
  LCD_INIT_CMD(0x06U, 50U);
  LCD_INIT_CMD(0x0CU, 50U);
  LCD_INIT_CMD(0x02U, 50U);

#undef LCD_INIT_CMD

  status = I2C2_LCD_Load_Icons();
  if (status != LCD1602_OK) {
    return status;
  }
  SystemTime_DelayMs(50U);

  DebugLogger_Log(DEBUG_LEVEL_INFO, "LCD", "Init OK");

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_LCD_Write_byte(char data) {
  Lcd1602Driver_Status_t status;

  if (!i2c2_lcd_ready) {
    return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_READY);
  }

  status = I2C2_Send_Start();
  if (status != LCD1602_OK) return status;

  status = I2C2_Send_Address(i2c2_lcd_address);
  if (status != LCD1602_OK) return status;

  status = I2C2_Send_Data((uint8_t)data);
  if (status != LCD1602_OK) return status;

  return I2C2_Send_Stop();
}

static Lcd1602Driver_Status_t I2C2_LCD_Data_Write(char data) {
  uint8_t data_u;
  uint8_t data_l;
  uint8_t data_t[4];
  uint8_t i;
  Lcd1602Driver_Status_t status;

  if (!i2c2_lcd_ready) {
    return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_READY);
  }

  data_u = (uint8_t)data & 0xF0U;
  data_l = ((uint8_t)data << 4) & 0xF0U;

  data_t[0] = data_u | 0x0DU;
  data_t[1] = data_u | 0x09U;
  data_t[2] = data_l | 0x0DU;
  data_t[3] = data_l | 0x09U;

  for (i = 0U; i < 4U; i++) {
    status = I2C2_LCD_Write_byte((char)data_t[i]);
    if (status != LCD1602_OK) return status;
  }

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_LCD_Control_Write(char data) {
  uint8_t data_u;
  uint8_t data_l;
  uint8_t data_t[4];
  uint8_t i;
  Lcd1602Driver_Status_t status;

  if (!i2c2_lcd_ready) {
    return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_READY);
  }

  data_u = (uint8_t)data & 0xF0U;
  data_l = ((uint8_t)data << 4) & 0xF0U;

  /* Keep the original PCF8574 control-byte mapping unchanged. */
  data_t[0] = data_u | 0x04U;
  data_t[1] = data_u;
  data_t[2] = data_l | 0x04U;
  data_t[3] = data_l | 0x08U;

  for (i = 0U; i < 4U; i++) {
    status = I2C2_LCD_Write_byte((char)data_t[i]);
    if (status != LCD1602_OK) return status;
  }

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

Lcd1602Driver_Status_t Lcd1602Driver_WriteChar(char c) {
  return I2C2_LCD_Data_Write(c);
}

Lcd1602Driver_Status_t Lcd1602Driver_WriteString(const char *str) {
  Lcd1602Driver_Status_t status;

  if (!i2c2_lcd_ready) {
    return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_READY);
  }
  if (str == 0) {
    return I2C2_LCD_Set_Error(LCD1602_OK);
  }

  while (*str) {
    status = I2C2_LCD_Data_Write(*str++);
    if (status != LCD1602_OK) return status;
  }

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

Lcd1602Driver_Status_t Lcd1602Driver_Clear(void) {
  Lcd1602Driver_Status_t status = I2C2_LCD_Control_Write(0x01);
  if (status != LCD1602_OK) return status;
  SystemTime_DelayMs(10U);
  return I2C2_LCD_Set_Error(LCD1602_OK);
}

Lcd1602Driver_Status_t Lcd1602Driver_SetCursor(char col, char row) {
  char address = (row == 0) ? (char)(0x80 + col) : (char)(0xC0 + col);
  return I2C2_LCD_Control_Write(address);
}

static Lcd1602Driver_Status_t I2C2_LCD_Send_Number(uint16_t num) {
  char buffer[16];
  uint8_t length = 0U;
  int i;
  Lcd1602Driver_Status_t status;

  if (num == 0U) {
    return I2C2_LCD_Data_Write('0');
  }

  while (num > 0U) {
    buffer[length++] = (char)((num % 10U) + '0');
    num /= 10U;
  }

  for (i = (int)length - 1; i >= 0; i--) {
    status = I2C2_LCD_Data_Write(buffer[i]);
    if (status != LCD1602_OK) return status;
  }

  return I2C2_LCD_Set_Error(LCD1602_OK);
}

static Lcd1602Driver_Status_t I2C2_LCD_Send_Float(float number) {
  uint16_t integer;
  float decimal;
  Lcd1602Driver_Status_t status;

  if (number < 0.0f) {
    status = Lcd1602Driver_WriteString("-");
    if (status != LCD1602_OK) return status;
    number = -number;
  }

  integer = (uint16_t)number;
  status = I2C2_LCD_Send_Number(integer);
  if (status != LCD1602_OK) return status;

  status = Lcd1602Driver_WriteString(".");
  if (status != LCD1602_OK) return status;

  decimal = (number - (float)integer) * 100.0f + 0.5f;
  return I2C2_LCD_Send_Number((uint16_t)decimal);
}

static Lcd1602Driver_Status_t I2C2_LCD_Create_Char(uint8_t location, uint8_t *charmap) {
  uint8_t i;
  Lcd1602Driver_Status_t status;

  if (charmap == 0) {
    return I2C2_LCD_Set_Error(LCD1602_ERROR_NOT_READY);
  }

  location &= 0x07U;
  status = I2C2_LCD_Control_Write((char)(0x40U | (location << 3)));
  if (status != LCD1602_OK) return status;

  for (i = 0U; i < 8U; i++) {
    status = I2C2_LCD_Data_Write((char)charmap[i]);
    if (status != LCD1602_OK) return status;
  }

  return I2C2_LCD_Control_Write((char)0x80U);
}

static Lcd1602Driver_Status_t I2C2_LCD_Load_Icons(void) {
  Lcd1602Driver_Status_t status;

  status = I2C2_LCD_Create_Char(0U, arrow_up);
  if (status != LCD1602_OK) return status;

  status = I2C2_LCD_Create_Char(1U, arrow_down);
  if (status != LCD1602_OK) return status;

  return I2C2_LCD_Create_Char(2U, char_ohm);
}
