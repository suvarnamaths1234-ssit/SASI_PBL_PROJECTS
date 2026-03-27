#include "MQ135.h"

// ============================================
// Global Variables
// ============================================
uint16_t mq135_adc_value = 0;
AirQuality_Status_t mq135_status = AIR_CLEAN;

// ============================================
// Initialize MQ135 ADC for P0.24 (AD0.1)
// ============================================
void MQ135_Init(void) {
    // Enable ADC power (PCADC bit in PCONP)
    LPC_SC->PCONP |= (1 << 12);
    
    // Configure P0.24 as AD0.1
    // PINSEL1[17:16] = 01 for AD0.1 function
    LPC_PINCON->PINSEL1 &= ~(3 << 16);  // Clear bits 17:16
    LPC_PINCON->PINSEL1 |= (1 << 16);   // Set bit 16 = 01
    
    // Configure ADC Control Register (ADCR)
    // [7:0] SEL: Select Channel 1 (for AD0.1)
    // [15:8] CLKDIV: 10 (ADC clock divider)
    // [16] BURST: 0 (software controlled)
    // [21] PDN: 1 (ADC operational)
    // [24] START: 0 (no conversion yet)
    LPC_ADC->ADCR = (1 << 1) |   // Select Channel 1 (AD0.1 = P0.24)
                    (10 << 8) |  // CLKDIV = 10
                    (1 << 21);   // Power up ADC
}

// ============================================
// Read MQ135 Sensor Value
// ============================================
uint16_t MQ135_Read(void) {
    // Start ADC conversion (START field = 001)
    LPC_ADC->ADCR |= (1 << 24);
    
    // Wait for conversion to complete (DONE bit = 1)
    while (!(LPC_ADC->ADGDR & (1 << 31)));
    
    // Read 12-bit result from ADGDR[15:4]
    mq135_adc_value = (LPC_ADC->ADGDR >> 4) & 0xFFF;
    
    // Update status based on value
    MQ135_GetStatus();
    
    return mq135_adc_value;
}

// ============================================
// Get Air Quality Status
// ============================================
AirQuality_Status_t MQ135_GetStatus(void) {
    if (mq135_adc_value <= AIR_CLEAN_MAX) {
        mq135_status = AIR_CLEAN;
    } else if (mq135_adc_value <= AIR_MODERATE_MAX) {
        mq135_status = AIR_MODERATE;
    } else {
        mq135_status = AIR_POOR;
    }
    
    return mq135_status;
}

// ============================================
// Get Human-Readable Status String
// ============================================
const char* MQ135_GetStatusString(void) {
    switch(mq135_status) {
        case AIR_CLEAN:
            return "Clean";
        case AIR_MODERATE:
            return "Moderate";
        case AIR_POOR:
            return "Poor/Smoke";
        default:
            return "Unknown";
    }
}
