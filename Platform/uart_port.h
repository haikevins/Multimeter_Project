#ifndef UART_PORT_H
#define UART_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define UART_PORT_RX_BUFFER_SIZE    64U
#define UART_PORT_TX_TIMEOUT_LOOPS  100000UL

uint8_t UartPort_TrySendChar(char chr);
void UartPort_SendChar(char chr);
void UartPort_SendString(const char* str);
void UartPort_Init(uint16_t baudrate);
void UartPort_SendFormat(const char* fmt, ...);
uint32_t UartPort_GetTxTimeoutCount(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_PORT_H */
