#include "LPC17xx.h"
#include "LCD_header.h"



void delay_ms(int ms);
void delay_us(int us);

/* Helper: pulse the E line */
static void lcd_pulse_enable(void) {
    LPC_GPIO0->FIOSET = ENABLE;
    delay_ms(1);
    LPC_GPIO0->FIOCLR = ENABLE;
}

/* Send a 4-bit nibble (to D4..D7 on P0.19..P0.22) with RS already set/cleared */
static void lcd_send_nibble(uint8_t nib) {
    // Clear data lines
    LPC_GPIO0->FIOCLR = DATA;
    // Put nibble on D4..D7
    LPC_GPIO0->FIOSET = ((uint32_t)(nib & 0x0F)) << 19;
    lcd_pulse_enable();
}

/* Command write */
void command(uint8_t addr) {
    LPC_GPIO0->FIOCLR = RS;               // RS = 0 for command
    lcd_send_nibble((addr >> 4) & 0x0F);   // upper
    lcd_send_nibble(addr & 0x0F);          // lower
    delay_ms(2);                           // allow command to settle
}

/* Data write (one character) */
void sendData(char data) {
    LPC_GPIO0->FIOSET = RS;               // RS = 1 for data
    lcd_send_nibble((data >> 4) & 0x0F);
    lcd_send_nibble(data & 0x0F);
}

/* Init sequence (4-bit mode, 2 lines) */
void initLCD(void) {
    delay_ms(40);          // LCD power-up
    command(0x02);         // return home, 4-bit
    command(0x28);         // 2 lines, 5x8, 4-bit
    command(0x0C);         // display on, cursor off, blink off
    command(0x06);         // entry mode: increment
    command(0x01);         // clear display
    delay_ms(2);
}

/* Print string at current cursor */
void sendString(const char *str) {
    while (*str) sendData(*str++);
}

/* Print string on second line (0xC0) */
void sendString1(const char *str) {
    command(0xC0);
    while (*str) sendData(*str++);
}
