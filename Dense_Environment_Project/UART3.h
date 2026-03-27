#ifndef UART_H
#define UART_H

#include <stdint.h>

// UART0 Functions
void init_uart0(void);
void uart_send_char(char c);
void uart_send_string(const char *str);
void uart_send_hex(uint8_t value);
char uart_receive_char(void);

// UART3 Functions
void init_uart3(void);
void uart3_send_char(char c);
void uart3_send_string(const char *str);
void uart3_send_hex(uint8_t value);
char uart3_receive_char(void);

// Dual UART Functions (Send to both UART0 and UART3)
void uart_dual_send_char(char c);
void uart_dual_send_string(const char *str);
void uart_dual_send_hex(uint8_t value);

#endif
