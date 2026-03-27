#include "LCD.h"
#include "DELAY.h"

// ============================================
// LCD Initialization
// ============================================
void lcd_init(void) {
    // Set all LCD pins as outputs
    LPC_GPIO0->FIODIR |= (RS | EN | D4 | D5 | D6 | D7);
    
    // Ensure all pins start LOW
    LPC_GPIO0->FIOCLR = (RS | EN | D4 | D5 | D6 | D7);
    
    // Wait for LCD power-up (minimum 40ms, using 100ms to be safe)
    delay_ms(100);
    
    // ============================================
    // 4-bit Initialization Sequence
    // ============================================
    
    // Step 1: Send 0x03 three times (8-bit mode initialization)
    lcd_nibble(0x03);
    delay_ms(5);
    
    lcd_nibble(0x03);
    delay_ms(5);
    
    lcd_nibble(0x03);
    delay_ms(5);
    
    // Step 2: Switch to 4-bit mode
    lcd_nibble(0x02);
    delay_ms(5);
    
    // ============================================
    // Configuration Commands
    // ============================================
    
    // Function Set: 4-bit mode, 2 lines, 5x8 font
    lcd_command(0x28);
    delay_ms(5);
    
    // Display Control: Display ON, Cursor OFF, Blink OFF
    lcd_command(0x0C);
    delay_ms(5);
    
    // Entry Mode: Increment cursor, No shift
    lcd_command(0x06);
    delay_ms(5);
    
    // Clear Display
    lcd_command(0x01);
    delay_ms(5);
    
    // Home Cursor
    lcd_command(0x02);
    delay_ms(5);
}

// ============================================
// Send 4-bit nibble to LCD
// ============================================
void lcd_nibble(unsigned char nibble) {
    // Set data pins based on nibble value
    if(nibble & 0x01) 
        LPC_GPIO0->FIOSET = D4; 
    else 
        LPC_GPIO0->FIOCLR = D4;
    
    if(nibble & 0x02) 
        LPC_GPIO0->FIOSET = D5; 
    else 
        LPC_GPIO0->FIOCLR = D5;
    
    if(nibble & 0x04) 
        LPC_GPIO0->FIOSET = D6; 
    else 
        LPC_GPIO0->FIOCLR = D6;
    
    if(nibble & 0x08) 
        LPC_GPIO0->FIOSET = D7; 
    else 
        LPC_GPIO0->FIOCLR = D7;
    
    // Create enable pulse (minimum 450ns high time)
    LPC_GPIO0->FIOSET = EN;
    delay_us(10);
    LPC_GPIO0->FIOCLR = EN;
    delay_us(100);
}

// ============================================
// Send command to LCD
// ============================================
void lcd_command(unsigned char cmd) {
    LPC_GPIO0->FIOCLR = RS;  // RS = 0 for command
    lcd_nibble(cmd >> 4);     // Send upper nibble
    lcd_nibble(cmd & 0x0F);   // Send lower nibble
    delay_ms(2);              // Wait for command execution
}

// ============================================
// Send data (character) to LCD
// ============================================
void lcd_data(unsigned char data) {
    LPC_GPIO0->FIOSET = RS;  // RS = 1 for data
    lcd_nibble(data >> 4);    // Send upper nibble
    lcd_nibble(data & 0x0F);  // Send lower nibble
    delay_us(200);
}

// ============================================
// Send string to LCD
// ============================================
void lcd_string(const char *str) {
    while(*str) {
        lcd_data(*str++);
    }
}

// ============================================
// Clear LCD display
// ============================================
void lcd_clear(void) {
    lcd_command(0x01);
    delay_ms(5);
}

// ============================================
// Set cursor position
// ============================================
void lcd_goto(unsigned char row, unsigned char col) {
    unsigned char position;
    if(row == 0)
        position = 0x80 + col;  // First row starts at 0x80
    else
        position = 0xC0 + col;  // Second row starts at 0xC0
    lcd_command(position);
}

// ============================================
// LCD Test Function
// ============================================
void lcd_test(void) {
    lcd_clear();
    
    // Test 1: Full display test
    lcd_goto(0, 0);
    lcd_string("0123456789ABCDEF");
    lcd_goto(1, 0);
    lcd_string("LCD TEST SUCCESS");
    delay_ms(2000);
    
    // Test 2: Character by character
    lcd_clear();
    lcd_goto(0, 0);
    for(char c = 'A'; c <= 'P'; c++) {
        lcd_data(c);
        delay_ms(100);
    }
    delay_ms(2000);
    
    // Test 3: Blinking test
    for(int i = 0; i < 3; i++) {
        lcd_command(0x08);  // Display OFF
        delay_ms(500);
        lcd_command(0x0C);  // Display ON
        delay_ms(500);
    }
    
    lcd_clear();
}
