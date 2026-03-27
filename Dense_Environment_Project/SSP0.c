/**
 * ============================================
 * SSP0 (SPI) Driver for RC522
 * ============================================
 */

#include "LPC17xx.h"
#include "SSP0.h"

void SSP0_init(void) {
    // Power on SSP0
    LPC_SC->PCONP |= (1 << 21);
    
    // Configure pins for SSP0
    // P0.15 = SCK0
    LPC_PINCON->PINSEL0 &= ~(3 << 30);
    LPC_PINCON->PINSEL0 |= (2 << 30);
    
    // P0.17 = MISO0, P0.18 = MOSI0
    LPC_PINCON->PINSEL1 &= ~((3 << 2) | (3 << 4));
    LPC_PINCON->PINSEL1 |= ((2 << 2) | (2 << 4));
    
    // P0.16 SSEL as GPIO (manual CS control)
    LPC_PINCON->PINSEL1 &= ~(3 << 0);
    LPC_GPIO0->FIODIR |= (1 << 16);
    LPC_GPIO0->FIOSET = (1 << 16);  // CS high initially
    
    // Configure SSP0 for RC522
    LPC_SSP0->CR0 = 0x07;  // 8-bit, SPI mode, CPOL=0, CPHA=0
    LPC_SSP0->CPSR = 8;    // Clock prescaler
    LPC_SSP0->CR1 = 0x02;  // Enable SSP, Master mode
    
    // Clear RX FIFO
    uint8_t dummy;
    while(LPC_SSP0->SR & (1 << 2)) {
        dummy = LPC_SSP0->DR;
    }
    (void)dummy;
}

void SelSlave(void) {
    LPC_GPIO0->FIOCLR = (1 << 16);  // CS low (active)
}

void DeselSlave(void) {
    LPC_GPIO0->FIOSET = (1 << 16);  // CS high (inactive)
}

uint8_t SSP0_TRANSFER(uint8_t data) {
    // Wait until TX FIFO is not full
    while(!(LPC_SSP0->SR & (1 << 1)));
    
    // Send data
    LPC_SSP0->DR = data;
    
    // Wait until RX FIFO has data
    while(!(LPC_SSP0->SR & (1 << 2)));
    
    // Read and return received data
    return (uint8_t)LPC_SSP0->DR;
}

void SSP0_Write(uint8_t addr, uint8_t value) {
    SelSlave();
    SSP0_TRANSFER((addr << 1) & 0x7E);  // Address, write mode
    SSP0_TRANSFER(value);
    DeselSlave();
}

uint8_t SSP0_Read(uint8_t addr) {
    uint8_t data;
    SelSlave();
    SSP0_TRANSFER(((addr << 1) & 0x7E) | 0x80);  // Address, read mode
    data = SSP0_TRANSFER(0x00);  // Dummy byte to read
    DeselSlave();
    return data;
}
