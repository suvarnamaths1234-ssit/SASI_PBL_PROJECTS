/**
 * ============================================
 * DHT11 Temperature & Humidity Sensor Driver
 * Pin: P0.26 (Changed from P0.7)
 * 
 * EASY PIN CHANGE:
 * - Change DHT11_PIN define below
 * - Change DHT11_PINSEL_BIT accordingly
 * ============================================
 */

#include "LPC17xx.h"
#include "DHT11.h"
#include "DELAY.h"
#include "globals.h"

// ========== PIN CONFIGURATION (CHANGE HERE) ==========
#define DHT11_PIN (1<<7)           // P0.26 (bit 26)
#define DHT11_PINSEL_BIT 20         // PINSEL1 bits 21:20 for P0.26

/* OTHER PIN OPTIONS:
 * P0.25: #define DHT11_PIN (1<<25)  // DHT11_PINSEL_BIT 18
 * P0.26: #define DHT11_PIN (1<<26)  // DHT11_PINSEL_BIT 20
 * P0.4:  #define DHT11_PIN (1<<4)   // DHT11_PINSEL_BIT 8  (use PINSEL0)
 * P0.5:  #define DHT11_PIN (1<<5)   // DHT11_PINSEL_BIT 10 (use PINSEL0)
 * P1.26: Use LPC_GPIO1 instead of LPC_GPIO0
 */

uint8_t dht11_data[5];

void DHT11_Init(void) {
    // Configure P0.26 as GPIO (not special function)
    //LPC_PINCON->PINSEL1 &= ~(3 << DHT11_PINSEL_BIT);  // Clear PINSEL bits for P0.26
    
    // Set DHT11 pin as output initially
    LPC_GPIO0->FIODIR |= DHT11_PIN;
    LPC_GPIO0->FIOSET = DHT11_PIN;  // Pull HIGH
    delay_ms(2000);  // Wait 2 seconds for sensor to stabilize
}

uint8_t read_dht11(void) {
    uint8_t i, j;
    uint32_t timeout;
   
    // Clear data array
    for(i = 0; i < 5; i++) {
        dht11_data[i] = 0;
    }
   
    // ========== START SIGNAL ==========
    LPC_GPIO0->FIODIR |= DHT11_PIN;      // Set as output
    LPC_GPIO0->FIOCLR = DHT11_PIN;       // Pull LOW
    delay_ms(20);                         // 20ms LOW signal
    
    LPC_GPIO0->FIOSET = DHT11_PIN;       // Pull HIGH
    delay_us(40);                         // 40us HIGH signal
    
    LPC_GPIO0->FIODIR &= ~DHT11_PIN;     // Switch to input mode
   
    // ========== WAIT FOR DHT11 RESPONSE ==========
    // DHT11 should pull line LOW
    timeout = 0;
    while((LPC_GPIO0->FIOPIN & DHT11_PIN) && timeout++ < 5000);
    if(timeout >= 5000) {
        return 0;  // Timeout - no response
    }
   
    // DHT11 pulls LOW for 80us
    timeout = 0;
    while(!(LPC_GPIO0->FIOPIN & DHT11_PIN) && timeout++ < 5000);
    if(timeout >= 5000) {
        return 0;  // Timeout
    }
   
    // DHT11 pulls HIGH for 80us
    timeout = 0;
    while((LPC_GPIO0->FIOPIN & DHT11_PIN) && timeout++ < 5000);
    if(timeout >= 5000) {
        return 0;  // Timeout
    }
   
    // ========== READ 40 BITS (5 BYTES) ==========
    for(i = 0; i < 5; i++) {
        for(j = 0; j < 8; j++) {
            // Wait for bit start (50us LOW)
            timeout = 0;
            while(!(LPC_GPIO0->FIOPIN & DHT11_PIN) && timeout++ < 5000);
            if(timeout >= 5000) {
                return 0;  // Timeout
            }
           
            // Wait 40us and check if still HIGH
            delay_us(40);
           
            // If HIGH = bit 1, if LOW = bit 0
            if(LPC_GPIO0->FIOPIN & DHT11_PIN) {
                dht11_data[i] |= (1 << (7 - j));  // Set bit to 1
            }
           
            // Wait for HIGH signal to end
            timeout = 0;
            while((LPC_GPIO0->FIOPIN & DHT11_PIN) && timeout++ < 5000);
            if(timeout >= 5000) {
                return 0;  // Timeout
            }
        }
    }
   
    // ========== VERIFY CHECKSUM ==========
    uint8_t checksum = dht11_data[0] + dht11_data[1] + dht11_data[2] + dht11_data[3];
    if(checksum != dht11_data[4]) {
        return 0;  // Checksum mismatch
    }
   
    // ========== EXTRACT VALUES ==========
    humidity = (float)dht11_data[0] + ((float)dht11_data[1] / 10.0f);
    temperature = (float)dht11_data[2] + ((float)dht11_data[3] / 10.0f);
   
    // ========== SANITY CHECK ==========
    if(humidity > 100.0f || temperature > 60.0f || temperature < 0.0f) {
        return 0;  // Invalid reading
    }
   
    return 1;  // Success
}
