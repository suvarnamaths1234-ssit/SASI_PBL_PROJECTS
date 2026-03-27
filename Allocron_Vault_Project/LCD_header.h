#ifndef LCD_H
#define LCD_H

#include <LPC17xx.h>
#include <stdint.h>

#define RS      (1u << 10)     // P0.10
#define ENABLE  (1u << 11)     // P0.11
#define DATA    (0x0Fu << 19)  // P0.19..P0.22 = D4..D7

/* Renamed to avoid collisions */
void lcd_delay_ms(uint16_t ms);

void command(uint8_t addr);
void initLCD(void);
void sendData(char data);
void sendString(const char *str);
void sendString1(const char *str);

#endif
