#include "uart_port.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/**
 * @brief Ring buffer for USART1 RX data.
 *
 * Stores incoming characters from the USART1 interrupt handler.
 */
static volatile char usart1_rx_buffer[UART_PORT_RX_BUFFER_SIZE];

/**
 * @brief Current write index in the USART1 RX buffer.
 *
 * Points to the position where the next received byte will be stored.
 */
static volatile uint8_t usart1_rx_index = 0;

/**
 * @brief Flag indicating if new data is available in USART1 RX buffer.
 *
 * - 0: No new data  
 * - 1: New data received and ready for processing
 */
static volatile uint8_t usart1_rx_data_ready = 0;
static volatile uint32_t usart1_tx_timeout_count = 0U;
static uint8_t usart1_initialized = 0U;

/**
  * @brief  Configures USART1 with specified baudrate.
  * @note   USART1 is mapped to:
  *         - TX: PA9 (Alternate Function Push-Pull)
  *         - RX: PA10 (Input Floating)
  *         Enables clock for GPIOA and USART1.
  * @param  baudrate: Desired baudrate (e.g. 9600).
  * @retval None
  */
void UartPort_Init(uint16_t baudrate) {
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART1_InitStructure;
	
  // TX - PA9
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
  // RX - PA10
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

  USART1_InitStructure.USART_BaudRate = baudrate;
  USART1_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART1_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
  USART1_InitStructure.USART_Parity = USART_Parity_No;
  USART1_InitStructure.USART_StopBits = USART_StopBits_1;
  USART1_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_Init(USART1, &USART1_InitStructure);

  // Enable interrupt
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  NVIC_EnableIRQ(USART1_IRQn);

  USART_Cmd(USART1, ENABLE);
  usart1_initialized = 1U;
}

/**
  * @brief  Sends a single character via USART1.
  * @param  chr: Character to send.
  * @retval None
  */
uint8_t UartPort_TrySendChar(char chr) {
  uint32_t timeout = 0U;

  if (!usart1_initialized) {
    return 0U;
  }

  while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
    if (++timeout >= UART_PORT_TX_TIMEOUT_LOOPS) {
      usart1_tx_timeout_count++;
      return 0U;
    }
  }

  USART_SendData(USART1, (uint16_t)chr);
  return 1U;
}

void UartPort_SendChar(char chr) {
  (void)UartPort_TrySendChar(chr);
}

/**
  * @brief  Sends a null-terminated string via USART1.
  * @param  str: Pointer to the string to send.
  * @retval None
  */
void UartPort_SendString(const char *str) {
  if (!str) return;

  while (*str != '\0') {
    if (!UartPort_TrySendChar(*str++)) {
      break;
    }
  }
}

uint32_t UartPort_GetTxTimeoutCount(void) {
  return usart1_tx_timeout_count;
}

void UartPort_SendFormat(const char *fmt, ...) {
  char buffer[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);  // Build string
  va_end(args);
  UartPort_SendString(buffer);
}

/**
 * @brief Redirects the standard output stream (e.g., printf) to USART1.
 *
 * This function overrides the `fputc()` function used by `printf()` and 
 * sends each character to USART1. It enables debugging or data output 
 * over the serial port.
 *
 * @param chr The character to send.
 * @param f Pointer to the FILE structure (ignored in this implementation).
 * @return The character that was sent.
 */
FILE __stdout;

int fputc(int chr, FILE *f) {
  (void)f;
  UartPort_SendChar((char)chr);
  return chr;
}

/**
  * @brief  USART1 interrupt handler.
  * @note
  *   - Stores characters into `usart1_rx_buffer`
  *   - Sets `usart1_rx_data_ready = 1` when '!' is received
  *   - Resets index on overflow
  * @retval None
  */
void USART1_IRQHandler(void) {
  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
    char ch = USART_ReceiveData(USART1);  // RXNE is automatically deleted after reading DL

    if (usart1_rx_index < sizeof(usart1_rx_buffer) - 1) {
      usart1_rx_buffer[usart1_rx_index++] = ch;

      if (ch == '!') {
        usart1_rx_buffer[usart1_rx_index] = '\0';
        usart1_rx_data_ready = 1;
        usart1_rx_index = 0;
      }
    } else {
      // Buffer overflow: reset
      usart1_rx_index = 0;
      usart1_rx_data_ready = 0;
      memset((char *)usart1_rx_buffer, 0, sizeof(usart1_rx_buffer));  // Clear garbage
    }
  }
}
