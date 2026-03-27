/**
* ============================================
* DHT11 HEADER - PIN: 0.7 
* ============================================
*/

#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

// Function prototypes
void DHT11_Init(void);
uint8_t read_dht11(void);

#endif // DHT11_H
