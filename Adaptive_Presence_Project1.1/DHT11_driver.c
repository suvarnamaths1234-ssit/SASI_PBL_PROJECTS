#include "DHT11_header.h"

// This delay function matches your working code
void delay_dht(int us) {
	LPC_TIM0->TCR = 0x02;
	LPC_TIM0->PR = 0;
	LPC_TIM0->MR0 = 25*us - 1;  // 25 cycles per microsecond (matches your working code)
	LPC_TIM0->TCR = 0x01;
	while(LPC_TIM0->TC < LPC_TIM0->MR0);
	LPC_TIM0->TCR = 0x02;
}

void dht11Start(void) {
	LPC_GPIO0->FIODIR |= PIN;
	LPC_GPIO0->FIOCLR = PIN;
	delay_dht(18000);
	LPC_GPIO0->FIOSET = PIN;
	delay_dht(40);
	LPC_GPIO0->FIODIR &= ~(PIN);
}

uint8_t readBit(void) {
	while(!(LPC_GPIO0->FIOPIN & PIN));
	delay_dht(40);
	return (LPC_GPIO0->FIOPIN & PIN) ? 1 : 0;
}

uint8_t readByte(void) {
	uint8_t i, byte = 0;
	for(i = 0; i < 8; i++) {
		byte <<= 1;
		byte |= readBit();
		while(LPC_GPIO0->FIOPIN & PIN); 
	}
	return byte;
}

uint8_t dht11Read(uint8_t *humidity, uint8_t *temperature) {
	uint8_t humInt, humDec, tempInt, tempDec, checksum;
	
	dht11Start();
	delay_dht(40);
	
	if (!(LPC_GPIO0->FIOPIN & PIN)) { // LOW (80us)
		delay_dht(80);
		if (LPC_GPIO0->FIOPIN & PIN) { // HIGH (80us)
			delay_dht(80);
		}
	}
	
	humInt = readByte();
	humDec = readByte();
	tempInt = readByte();
	tempDec = readByte();
	checksum = readByte();
	
	if((humInt + humDec + tempInt + tempDec) == checksum) {
		*humidity = humInt;
		*temperature = tempInt;
		return 1;
	}
	return 0;
}

// Test function to check if DHT11 is connected
uint8_t dht11Test(void) {
	uint32_t timeout = 0;
	
	// Set as output and pull LOW
	LPC_GPIO0->FIODIR |= PIN;
	LPC_GPIO0->FIOCLR = PIN;
	delay_dht(18000);
	
	// Pull HIGH and switch to input
	LPC_GPIO0->FIOSET = PIN;
	delay_dht(40);
	LPC_GPIO0->FIODIR &= ~(PIN);
	
	// Check if DHT11 pulls line LOW (response signal)
	delay_dht(40);
	while(LPC_GPIO0->FIOPIN & PIN) {
		timeout++;
		if(timeout > 10000) return 0;  // No response
	}
	
	return 1;  // DHT11 responded
}