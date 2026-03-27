#include "LPC17xx.h"
#include "uart.h"
#include <stdint.h>

/* ================= UART0 Functions ================= */
void UART0_Init(void){
    LPC_SC->PCONP |= (1 << 3);
    LPC_PINCON->PINSEL0 |= (0x5 << 4);
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLL = 0xA2;
    LPC_UART0->LCR = 0x03;
}

void UART0_SendChar(char c){
    while(!(LPC_UART0->LSR & (1 << 5)));
    LPC_UART0->THR = c;
}

void UART0_SendString(const char *s){
    while(*s) UART0_SendChar(*s++);
}

void UART0_SendHex(uint8_t value){
    const char hex[] = "0123456789ABCDEF";
    UART0_SendChar(hex[(value >> 4) & 0x0F]);
    UART0_SendChar(hex[value & 0x0F]);
}

/* ================= UART3 Functions ================= */
void UART3_Init(void){
    LPC_SC->PCONP |= (1 << 25);
    LPC_PINCON->PINSEL0 |= (0xA << 0); 
    LPC_UART3->LCR = 0x83;
    LPC_UART3->DLL = 0xA2;
		LPC_UART3->DLM = 0x00;
    LPC_UART3->LCR = 0x03;
}

void UART3_SendChar(char c){
    while(!(LPC_UART3->LSR & (1 << 5)));
    LPC_UART3->THR = c;
}

void UART3_SendString(const char *s){
    while(*s) UART3_SendChar(*s++);
}

void UART3_SendHex(uint8_t value){
    const char hex[] = "0123456789ABCDEF";
    UART3_SendChar(hex[(value >> 4) & 0x0F]);
    UART3_SendChar(hex[value & 0x0F]);
}

/* ================= Wrapper Functions ================= */
void init_uart0(void){
    UART0_Init();
}

void init_uart3(void){
    UART3_Init();
}

/* ================= DUAL UART - Sends to BOTH! ================= */
void uart_dual_send_char(char c){
    UART0_SendChar(c);      // ? UART0 (P0.2/P0.3)
    UART3_SendChar(c);      // ? UART3 (P0.0/P0.1)
}

void uart_dual_send_string(const char *str){
    while(*str){
        uart_dual_send_char(*str++);
    }
}

void uart_dual_send_hex(uint8_t value){
    UART0_SendHex(value);   // ? UART0
    UART3_SendHex(value);   // ? UART3
}
