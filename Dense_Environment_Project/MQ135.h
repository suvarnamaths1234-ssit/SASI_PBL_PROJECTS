#ifndef MQ135_H
#define MQ135_H

#include "LPC17xx.h"
#include <stdint.h>

// ============================================
// MQ135 Pin Configuration
// ============================================
#define MQ135_ADC_CHANNEL 1  // ADC Channel 1 (was 0)
#define MQ135_PIN 24         // P0.24 (AD0.1)

// ============================================
// Air Quality Thresholds (12-bit ADC: 0-4095)
// ============================================
#define AIR_CLEAN_MAX 1000     // 0-1000: Clean air
#define AIR_MODERATE_MAX 2500  // 1001-2500: Moderate
                               // 2501-4095: Poor/Smoke

// ============================================
// Air Quality Status Enum
// ============================================
typedef enum {
    AIR_CLEAN = 0,      // Good air quality
    AIR_MODERATE = 1,   // Acceptable air quality
    AIR_POOR = 2        // Poor air quality / Smoke detected
} AirQuality_Status_t;

// ============================================
// Global Variables (extern)
// ============================================
extern uint16_t mq135_adc_value;
extern AirQuality_Status_t mq135_status;

// ============================================
// Function Prototypes
// ============================================
void MQ135_Init(void);
uint16_t MQ135_Read(void);
AirQuality_Status_t MQ135_GetStatus(void);
const char* MQ135_GetStatusString(void);

#endif // MQ135_H
