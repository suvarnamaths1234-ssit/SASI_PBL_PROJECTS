/**
 * ============================================
 * DELAY FUNCTIONS
 * WORKING VERSION
 * ============================================
 */

#include "DELAY.h"

void delay_ms(unsigned int ms) {
    for(unsigned int i = 0; i < ms; i++) {
        for(volatile unsigned int j = 0; j < 10000; j++);
    }
}

void delay_us(unsigned int us) {
    for(volatile unsigned int i = 0; i < us * 10; i++);
}
