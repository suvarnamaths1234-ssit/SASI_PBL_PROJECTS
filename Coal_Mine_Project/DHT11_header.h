#ifndef DHT11_HEADER_H
#define DHT11_HEADER_H

#include <LPC17xx.h>
#include <stdio.h>

#define PIN (1<<4)  // P0.4

// Function prototypes
void delay_dht(int us);
void dht11Start(void);
uint8_t readBit(void);
uint8_t readByte(void);
uint8_t dht11Read(uint8_t *humidity, uint8_t *temperature);
uint8_t dht11Test(void);

#endif