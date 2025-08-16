#include "i2c2_lcd.h"
#include "usart1.h"
#include "systick.h"

uint8_t i2ic2_lcd_address; /**< Global variable for storing detected LCD I2C address */

/**
  * @brief Custom character bitmap for "Arrow Up" icon.
  * @note  Each element represents a 5x8 dot matrix row (bit pattern).
  */
uint8_t arrow_up[8] = {
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b00000,
  0b00000
};

/**
  * @brief Custom character bitmap for "Arrow Down" icon.
  * @note  Each element represents a 5x8 dot matrix row (bit pattern).
  */
uint8_t arrow_down[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00000,
  0b00000
};


/**
  * @brief Custom character bitmap for the "Ohm (O)" symbol.
  * @note  Designed for LCDs supporting custom 5x8 character definitions.
  */
uint8_t char_ohm[8] = {
  0b01110,  /**<  ***  */
  0b10001,  /**< *   * */
  0b10001,  /**< *   * */
  0b10001,  /**< *   * */
  0b01110,  /**<  ***  */
  0b01010,  /**<  * *  */
  0b11011,  /**< ** ** */
  0b00000   /**<       */    
};

/**
  * @brief  Scans I2C2 bus for LCD address in range 0x20–0x3F.
  * @retval Found LCD address or default (0x27) if not found.
  */
uint8_t I2C2_Scan_Address(void) {
  uint8_t address;
  uint8_t found = 0;
#ifdef I2C2_LCD_DEBUG
  Usart1_Send_String("Scanning I2C2 LCD addresses...\r\n");
#endif
  for (address = 0x20; address <= 0x3F; address++) {
    if (address == 0x28) {
      address = 0x38;  // Skip reserved address
    }

    uint32_t timeout = I2C2_LCD_TIMEOUT;
    I2C2_Send_Start();

    I2C2->DR = address << 1;

    while (!(I2C2->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF))) {
      if (--timeout == 0) {
        break;
      }
    }

    if (I2C2->SR1 & I2C_SR1_ADDR) {
      (void)I2C2->SR2;  // Clear ADDR flag
#ifdef I2C2_LCD_DEBUG
      Usart1_Send_String("LCD found at address: 0x");
      Usart1_Send_Format("%04X", address);
      Usart1_Send_String("\r\n");
#endif
      found = address;
      break;
    }

    I2C2_Send_Stop();
    I2C2->SR1 &= ~I2C_SR1_AF;  // Clear ACK failure flag
    Delay_Ms(5);
  }

  if (!found) {
#ifdef I2C2_LCD_DEBUG
    Usart1_Send_String("LCD not found, using default address: 0x");
    Usart1_Send_Format("%04X", 0x27);
    Usart1_Send_String("\r\n");
#endif
    return 0x27;
  }

  return found;
}

/**
  * @brief  Initializes I2C2 and LCD in 4-bit mode.
  * @note   Performs LCD reset sequence and sets LCD address.
  * @retval None
  */
void I2C2_LCD_Init(void) {
  RCC->APB1ENR |= (1 << 22);  // Enable I2C2 clock
  RCC->APB2ENR |= (1 << 3);   // Enable GPIOB clock

  // Configure PB10 (SCL) and PB11 (SDA) as alternate function open-drain (I2C2 pins)
  GPIOB->CRH &= ~((0xF << 8) | (0xF << 12));
  GPIOB->CRH |= ((0xB << 8) | (0xB << 12));

  // Reset I2C2
  I2C2->CR1 |= (1 << 15);
  I2C2->CR1 &= ~(1 << 15);

  // Configure I2C2 timing
  I2C2->CR2 = (36 & 0x3F);
  I2C2->CCR = 180;
  I2C2->TRISE = 37;

  // Set I2C2 Own Address1 and enable ACK
  I2C2->OAR1 = (1 << 14);
  I2C2->CR1 = (1 << 0);

  // Detect LCD address on I2C bus
  i2ic2_lcd_address = I2C2_Scan_Address();
  if (!i2ic2_lcd_address) {
#ifdef I2C2_LCD_DEBUG
    Usart1_Send_String("LCD not found!\r\n");
#endif
    i2ic2_lcd_address = 0x27;
  }
  i2ic2_lcd_address <<= 1;  // Shift left for 8-bit addressing

  // LCD Initialization sequence
  I2C2_LCD_Control_Write(0x33);
  Delay_Ms(10);
  I2C2_LCD_Control_Write(0x32);
  Delay_Ms(50);
  I2C2_LCD_Control_Write(0x28);
  Delay_Ms(50);
  I2C2_LCD_Control_Write(0x01);
  Delay_Ms(50);
  I2C2_LCD_Control_Write(0x06);
  Delay_Ms(50);
  I2C2_LCD_Control_Write(0x0C);
  Delay_Ms(50);
  I2C2_LCD_Control_Write(0x02);
  Delay_Ms(50);

  // Load icon custom 
  I2C2_LCD_Load_Icons();
  Delay_Ms(50);
}

/**
  * @brief  Sends a data byte over I2C2.
  * @param  data: Byte to send.
  * @retval None
  */
inline void I2C2_Send_Data(uint8_t data) {
  I2C2->DR = data;
  uint32_t timeout = I2C2_LCD_TIMEOUT;
  while (!(I2C2->SR1 & I2C_SR1_TXE)) {
    if (--timeout == 0) {
      break;  // tránh treo
    }
  }
}

/**
  * @brief  Sends I2C device address and waits for ACK.
  * @param  addr: 7-bit address left-shifted (including R/W bit = 0).
  * @retval None
  */
inline void I2C2_Send_Address(uint8_t addr) {
  I2C2->DR = addr;
  while (!(I2C2->SR1 & I2C_SR1_ADDR))
    ;
  volatile uint32_t temp = I2C2->SR2;  // Clear ADDR flag
  (void)temp;
}

/**
  * @brief  Sends I2C START condition and waits for SB bit.
  * @retval None
  */
inline void I2C2_Send_Start(void) {
  I2C2->CR1 |= I2C_CR1_START;
  while (!(I2C2->SR1 & I2C_SR1_SB))
    ;
}

/**
  * @brief  Sends I2C STOP condition.
  * @retval None
  */
void I2C2_Send_Stop(void) {
  I2C2->CR1 |= I2C_CR1_STOP;
}

/**
  * @brief  Sends a raw byte to the LCD via I2C2.
  * @param  data: Byte to send.
  * @retval None
  */
void I2C2_LCD_Write_byte(char data) {
  I2C2_Send_Start();
  I2C2_Send_Address(i2ic2_lcd_address);
  I2C2_Send_Data(data);
  I2C2_Send_Stop();
}

/**
  * @brief  Writes a character data byte to LCD.
  * @param  data: Data byte to send.
  * @retval None
  */
void I2C2_LCD_Data_Write(char data) {
  char data_u = data & 0xf0;
  char data_l = (data << 4) & 0xf0;

  uint8_t data_t[4] = {
    data_u | 0x0D,  // Enable high, RS=1, backlight on
    data_u | 0x09,  // Enable low, RS=1, backlight on
    data_l | 0x0D,  // Enable high, RS=1, backlight on
    data_l | 0x09   // Enable low, RS=1, backlight on
  };

  for (uint8_t i = 0; i < 4; i++) {
    I2C2_LCD_Write_byte(data_t[i]);
  }
}

/**
  * @brief  Writes a control command byte to LCD.
  * @param  data: Command byte to send.
  * @retval None
  */
void I2C2_LCD_Control_Write(char data) {
  char data_u = data & 0xf0;
  char data_l = (data << 4) & 0xf0;

  uint8_t data_t[4] = {
    data_u | 0x04,  // Enable high, RS=0, backlight on
    data_u,         // Enable low, RS=0, backlight on
    data_l | 0x04,  // Enable high, RS=0, backlight on
    data_l | 0x08,  // Enable low, RS=0, backlight off (or backlight bit different?)
  };

  for (uint8_t i = 0; i < 4; i++) {
    I2C2_LCD_Write_byte(data_t[i]);
  }
}

/**
  * @brief  Sends a single character to LCD.
  * @param  c: ASCII character to send.
  * @retval None
  */
void I2C2_LCD_Send_Char(char c) {
  I2C2_LCD_Data_Write(c);
}

/**
  * @brief  Sends a null-terminated string to LCD.
  * @param  str: Pointer to null-terminated string.
  * @retval None
  */
void I2C2_LCD_Send_String(const char *str) {
  while (*str) {
    I2C2_LCD_Data_Write(*str++);
  }
}

/**
  * @brief  Clears the LCD screen.
  * @retval None
  */
void I2C2_LCD_Clear(void) {
  I2C2_LCD_Control_Write(0x01);  // Clear display command
  Delay_Ms(10);
}

/**
  * @brief  Sets the cursor to specified column and row.
  * @param  col: Column (0–15).
  * @param  row: Row (0 or 1).
  * @retval None
  */
void I2C2_LCD_Set_Cursor(char col, char row) {
  char address = (row == 0) ? (0x80 + col) : (0xC0 + col);
  I2C2_LCD_Control_Write(address);
}

/**
  * @brief  Displays an unsigned integer number on LCD.
  * @param  num: Number to display.
  * @retval None
  */
void I2C2_LCD_Send_Number(uint16_t num) {
  char buffer[16];
  uint8_t length = 0;

  if (num == 0) {
    I2C2_LCD_Data_Write('0');
    return;
  }

  while (num > 0) {
    buffer[length++] = (num % 10) + '0';
    num /= 10;
  }

  for (int i = length - 1; i >= 0; i--) {
    I2C2_LCD_Data_Write(buffer[i]);
  }
}

/**
  * @brief  Displays a floating point number on LCD with 2 decimal places.
  * @param  number: Number to display.
  * @retval None
  */
void I2C2_LCD_Send_Float(float number) {
  if (number < 0) {
    I2C2_LCD_Send_String("-");
    number = -number;
  }

  uint16_t integer = (uint16_t)number;
  float decimal = number - (uint16_t)number;
  I2C2_LCD_Send_Number(integer);
  I2C2_LCD_Send_String(".");
  decimal = (number - integer) * 100 + 0.5f;
  I2C2_LCD_Send_Number((uint16_t)decimal);
}

/**
  * @brief  Creates a custom character in CGRAM.
  * @param  location: CGRAM location (0–7).
  * @param  charmap: Pointer to 8-byte character pattern.
  * @retval None
  */
void I2C2_LCD_Create_Char(uint8_t location, uint8_t *charmap) {
  location &= 0x07;
  I2C2_LCD_Control_Write(0x40 | (location << 3));  // Set CGRAM address

  for (int i = 0; i < 8; i++) {
    I2C2_LCD_Data_Write(charmap[i]);
  }

  I2C2_LCD_Control_Write(0x80);
}

/**
  * @brief  Load predefined custom icons into LCD CGRAM.
  * @note   This function uploads the custom characters (arrow up, 
  *         arrow down, and ohm symbol) into the LCD controller 
  *         so they can be displayed with their assigned codes (0–2).
  * @retval None
  */
void I2C2_LCD_Load_Icons(void) {
  I2C2_LCD_Create_Char(0, arrow_up);
  I2C2_LCD_Create_Char(1, arrow_down);
	I2C2_LCD_Create_Char(2, char_ohm);
}
