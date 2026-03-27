#ifndef LCD_H
#define LCD_H

#include "LPC17xx.h"

// ============================================
// LCD Pin Definitions (YOUR CONFIGURATION)
// ============================================
// Control Pins
#define RS (1<<10)  // P0.10
#define EN (1<<11)  // P0.11

// Data Pins - 4-bit mode
#define D4 (1<<19)  // P0.19 (was commented as P0.15)
#define D5 (1<<20)  // P0.20 (was commented as P0.16)
#define D6 (1<<21)  // P0.21 (was commented as P0.17)
#define D7 (1<<22)  // P0.22 (was commented as P0.18)

// ============================================
// LCD Functions
// ============================================
void lcd_init(void);
void lcd_command(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_string(const char *str);
void lcd_clear(void);
void lcd_goto(unsigned char row, unsigned char col);
void lcd_nibble(unsigned char nibble);
void lcd_test(void);

#endif
