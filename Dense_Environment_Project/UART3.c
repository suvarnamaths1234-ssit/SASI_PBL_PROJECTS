#include "uart.h"
#include "LPC17xx.h"

// ============================================
// UART3 FUNCTIONS (P0.0=TXD3, P0.1=RXD3)
// ============================================

void init_uart3(void) {
    // Configure pins P0.0 (TXD3) and P0.1 (RXD3)
    LPC_PINCON->PINSEL0 &= ~(0x0F << 0);
    LPC_PINCON->PINSEL0 |= (0x0A << 0);  // Function 10 = UART3
    
    // Enable UART3 power
    LPC_SC->PCONP |= (1 << 25);
    
    // Set UART3 clock divider to CCLK/4
    LPC_SC->PCLKSEL1 &= ~(3 << 18);
    LPC_SC->PCLKSEL1 |= (1 << 18);
    
    // Configure UART3 for 9600 baud, 8N1
    LPC_UART3->LCR = 0x83;
    
    
    LPC_UART3->DLL = 0xA3;
    LPC_UART3->DLM = 0;
    LPC_UART3->LCR = 0x03;
    
    LPC_UART3->FCR = 0x07;
}

void uart3_send_char(char c) {
    while(!(LPC_UART3->LSR & (1 << 5)));
    LPC_UART3->THR = c;
}

void uart3_send_string(const char *str) {
    while(*str) {
        uart3_send_char(*str++);
    }
}

void uart3_send_hex(uint8_t value) {
    char hex[] = "0123456789ABCDEF";
    uart3_send_char(hex[(value >> 4) & 0x0F]);
    uart3_send_char(hex[value & 0x0F]);
}

char uart3_receive_char(void) {
    while(!(LPC_UART3->LSR & 0x01));
    return LPC_UART3->RBR;
}

// ============================================
// DUAL UART FUNCTIONS (Send to both UART0 and UART3)
// ============================================

void uart_dual_send_char(char c) {
    uart_send_char(c);      // UART0
    uart3_send_char(c);     // UART3
}

void uart_dual_send_string(const char *str) {
    uart_send_string(str);      // UART0
    uart3_send_string(str);     // UART3
}

void uart_dual_send_hex(uint8_t value) {
    uart_send_hex(value);       // UART0
    uart3_send_hex(value);      // UART3
}
