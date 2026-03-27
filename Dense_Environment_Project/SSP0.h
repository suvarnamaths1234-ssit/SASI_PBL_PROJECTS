#ifndef SSP0_H
#define SSP0_H

#include <stdint.h>

void SSP0_init(void);
void SelSlave(void);
void DeselSlave(void);
uint8_t SSP0_TRANSFER(uint8_t data);
void SSP0_Write(uint8_t addr, uint8_t value);
uint8_t SSP0_Read(uint8_t addr);

#endif
