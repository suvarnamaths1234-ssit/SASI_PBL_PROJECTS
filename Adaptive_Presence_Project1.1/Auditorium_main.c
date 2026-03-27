#include <lpc17xx.h>
#include "LCD_header.h"
#include "DHT11_header.h"
#include <stdio.h>
#include <string.h>

/* ================= DEFINES ================= */
#define IR_ENTRY_PIN    (1 << 23)  // P1.23 - Entry IR sensor
#define IR_EXIT_PIN     (1 << 24)  // P1.24 - Exit IR sensor
#define BUZZER_PIN      (1 << 27)  // P1.27 - Buzzer
#define RESET_BTN_PIN   (1 << 25)  // P1.25 - Reset button (manual override)
#define MQ135_CH        0          // AD0.0 P0.23

#define MAX_CAPACITY    50         // Maximum audience capacity
#define SCROLL_DELAY    300        // Scrolling speed in ms
#define DEBOUNCE_DELAY  500        // Debounce time for sensors

/* ================= GLOBAL VARIABLES ================= */
volatile uint32_t audience_count = 0;
volatile uint32_t total_entries = 0;
volatile uint32_t total_exits = 0;
volatile uint32_t system_time_ms = 0;
volatile uint32_t last_entry_time = 0;
volatile uint32_t last_exit_time = 0;
volatile uint32_t session_start_time = 0;
volatile uint8_t capacity_full_flag = 0;

uint8_t temperature = 0;
uint8_t humidity = 0;
uint16_t air_quality = 0;
uint32_t sensor_timer = 0;

/* Welcome messages for scrolling */
const char* welcome_messages[] = {
    "Welcome to Event!",
    "Enjoy Your Time!",
    "Have a Great Day!",
    "Thank You!"
};
#define MSG_COUNT 4

/* ================= FUNCTION PROTOTYPES ================= */
void delay_ms(unsigned int ms);
void Timer0_Init(void);
void Timer1_Init(void);
void GPIO_Init(void);
void ADC_Init(void);
uint16_t ADC_Read(void);
void UART0_Init(void);
void UART3_Init(void);
void UART0_SendChar(char c);
void UART3_SendChar(char c);
void UART0_SendString(const char *s);
void UART3_SendString(const char *s);
void displayStatus(void);
void scrollMessage(const char *msg);
void buzzer_alert(void);
void checkCapacity(void);
void handleEntry(void);
void handleExit(void);
void handleReset(void);
char* formatTime(uint32_t ms);

/* ================= TIMER0 FOR DHT11 ================= */
void Timer0_Init(void){
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->PR  = 0;
    LPC_TIM0->TCR = 0x01;
}

/* ================= TIMER1 FOR SYSTEM TIME ================= */
void Timer1_Init(void){
    LPC_SC->PCONP |= (1 << 2);        // Power Timer1
    LPC_TIM1->TCR = 0x02;             // Reset timer
    LPC_TIM1->PR = 25000 - 1;         // Prescaler for 1ms tick
    LPC_TIM1->MR0 = 1;                // Match every 1ms
    LPC_TIM1->MCR = 0x03;             // Interrupt and reset on MR0
    NVIC_EnableIRQ(TIMER1_IRQn);
    LPC_TIM1->TCR = 0x01;             // Start timer
}

void TIMER1_IRQHandler(void){
    if(LPC_TIM1->IR & 0x01){
        LPC_TIM1->IR = 0x01;
        system_time_ms++;
    }
}

/* ================= ADC FOR MQ135 ================= */
void ADC_Init(void){
    LPC_SC->PCONP |= (1 << 12);
    LPC_PINCON->PINSEL1 &= ~(3 << 14);
    LPC_PINCON->PINSEL1 |=  (1 << 14);
    LPC_ADC->ADCR = (4 << 8) | (1 << 21);
}

uint16_t ADC_Read(void){
    LPC_ADC->ADCR &= ~0xFF;
    LPC_ADC->ADCR |= (1 << MQ135_CH);
    LPC_ADC->ADCR |= (1 << 24);
    while(!(LPC_ADC->ADGDR & (1U << 31)));
    return (LPC_ADC->ADGDR >> 4) & 0xFFF;
}

/* ================= UART ================= */
void UART0_Init(void){
    LPC_SC->PCONP |= (1 << 3);
    LPC_PINCON->PINSEL0 |= (0x5 << 4);
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLL = 0xA2;
    LPC_UART0->LCR = 0x03;
}

void UART3_Init(void){
    LPC_SC->PCONP |= (1 << 25);
    LPC_PINCON->PINSEL0 |= (0xA << 0);
    LPC_UART3->LCR = 0x83;
    LPC_UART3->DLL = 0xA2;
    LPC_UART3->LCR = 0x03;
}

void UART0_SendChar(char c){
    while(!(LPC_UART0->LSR & (1 << 5)));
    LPC_UART0->THR = c;
}

void UART3_SendChar(char c){
    while(!(LPC_UART3->LSR & (1 << 5)));
    LPC_UART3->THR = c;
}

void UART0_SendString(const char *s){
    while(*s) UART0_SendChar(*s++);
}

void UART3_SendString(const char *s){
    while(*s) UART3_SendChar(*s++);
}

/* ================= GPIO INITIALIZATION ================= */
void GPIO_Init(void){
    // LCD pins (already configured in your LCD_header)
    LPC_PINCON->PINSEL0 &= ~(3 << 20);   // P0.10 RS
    LPC_PINCON->PINSEL0 &= ~(3 << 22);   // P0.11 E
    LPC_PINCON->PINSEL1 &= ~(0xFF << 6); // P0.19..22 D4..D7
    LPC_GPIO0->FIODIR |= RS | ENABLE | DATA;
    LPC_GPIO0->FIOCLR = RS | ENABLE | DATA;
   
    // IR Sensors as input with pull-up
    LPC_GPIO1->FIODIR &= ~(IR_ENTRY_PIN | IR_EXIT_PIN);
    LPC_PINCON->PINMODE3 &= ~((3 << 14) | (3 << 16)); // Pull-up for P1.23, P1.24
   
    // Reset button as input with pull-up
    LPC_GPIO1->FIODIR &= ~RESET_BTN_PIN;
    LPC_PINCON->PINMODE3 &= ~(3 << 18); // Pull-up for P1.25
   
    // Buzzer as output
    LPC_GPIO1->FIODIR |= BUZZER_PIN;
    LPC_GPIO1->FIOCLR = BUZZER_PIN;
}

/* ================= DELAY FUNCTIONS ================= */
void delay_ms(unsigned int ms){
    for(unsigned int i = 0; i < ms; i++)
        for(unsigned int j = 0; j < 10000; j++);
}

/* ================= FORMAT TIME (HH:MM:SS) ================= */
char* formatTime(uint32_t ms){
    static char time_str[12];
    uint32_t seconds = ms / 1000;
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    sprintf(time_str, "%02lu:%02lu:%02lu", hours, minutes, secs);
    return time_str;
}

/* ================= BUZZER ALERT ================= */
void buzzer_alert(void){
    for(int i = 0; i < 3; i++){
        LPC_GPIO1->FIOSET = BUZZER_PIN;
        delay_ms(200);
        LPC_GPIO1->FIOCLR = BUZZER_PIN;
        delay_ms(200);
    }
}

/* ================= SCROLL MESSAGE ON LCD ================= */
void scrollMessage(const char *msg){
    char buffer[40];
    int len = strlen(msg);
   
    // Add spaces for smooth scrolling
    sprintf(buffer, "    %s    ", msg);
    int total_len = strlen(buffer);
   
    for(int i = 0; i <= total_len - 16; i++){
        command(0x80);  // First line
        for(int j = 0; j < 16; j++){
            if(i + j < total_len)
                sendData(buffer[i + j]);
            else
                sendData(' ');
        }
        delay_ms(SCROLL_DELAY);
    }
}

/* ================= DISPLAY STATUS ================= */
void displayStatus(void){
    char line1[17], line2[17];
   
    if(capacity_full_flag){
        command(0x01);  // Clear
        delay_ms(2);
       
        command(0x80);  // Line 1
        sendString("  HALL  FULL!  ");
       
        command(0xC0);  // Line 2
        sprintf(line2, "Count: %lu/%d   ", audience_count, MAX_CAPACITY);
        sendString(line2);
    }
    else{
        command(0x01);  // Clear
        delay_ms(2);
       
        command(0x80);  // Line 1
        sprintf(line1, "Audience: %lu   ", audience_count);
        sendString(line1);
       
        command(0xC0);  // Line 2
        sprintf(line2, "Max: %d Seats  ", MAX_CAPACITY);
        sendString(line2);
    }
}

/* ================= CHECK CAPACITY ================= */
void checkCapacity(void){
    if(audience_count >= MAX_CAPACITY && !capacity_full_flag){
        capacity_full_flag = 1;
        buzzer_alert();
       
        command(0x01);
        delay_ms(2);
        command(0x80);
        sendString("*** CAPACITY ***");
        command(0xC0);
        sendString("*** REACHED! ***");
        delay_ms(2000);
    }
    else if(audience_count < MAX_CAPACITY && capacity_full_flag){
        capacity_full_flag = 0;
    }
}

/* ================= HANDLE ENTRY ================= */
void handleEntry(void){
    uint32_t current_time = system_time_ms;
   
    // Debounce check
    if((current_time - last_entry_time) < DEBOUNCE_DELAY)
        return;
   
    last_entry_time = current_time;
   
    if(audience_count < MAX_CAPACITY){
        audience_count++;
        total_entries++;
       
        command(0x01);
        delay_ms(2);
        command(0x80);
        sendString("  ENTRY DETECTED");
        command(0xC0);
        char temp[17];
        sprintf(temp, "Count: %lu/%d   ", audience_count, MAX_CAPACITY);
        sendString(temp);
       
        // Short beep
        LPC_GPIO1->FIOSET = BUZZER_PIN;
        delay_ms(100);
        LPC_GPIO1->FIOCLR = BUZZER_PIN;
       
        delay_ms(1500);
    }
    else{
        // Already at capacity
        buzzer_alert();
        command(0x01);
        delay_ms(2);
        command(0x80);
        sendString("  ENTRY DENIED  ");
        command(0xC0);
        sendString("  HALL IS FULL! ");
        delay_ms(2000);
    }
   
    checkCapacity();
}

/* ================= HANDLE EXIT ================= */
void handleExit(void){
    uint32_t current_time = system_time_ms;
   
    // Debounce check
    if((current_time - last_exit_time) < DEBOUNCE_DELAY)
        return;
   
    last_exit_time = current_time;
   
    if(audience_count > 0){
        audience_count--;
        total_exits++;
       
        command(0x01);
        delay_ms(2);
        command(0x80);
        sendString("  EXIT DETECTED ");
        command(0xC0);
        char temp[17];
        sprintf(temp, "Count: %lu/%d   ", audience_count, MAX_CAPACITY);
        sendString(temp);
       
        // Short beep
        LPC_GPIO1->FIOSET = BUZZER_PIN;
        delay_ms(100);
        LPC_GPIO1->FIOCLR = BUZZER_PIN;
       
        delay_ms(1500);
    }
   
    checkCapacity();
}

/* ================= HANDLE RESET ================= */
void handleReset(void){
    audience_count = 0;
    capacity_full_flag = 0;
   
    command(0x01);
    delay_ms(2);
    command(0x80);
    sendString(" SYSTEM  RESET  ");
    command(0xC0);
    sendString("  Count: 0      ");
   
    // Long beep
    LPC_GPIO1->FIOSET = BUZZER_PIN;
    delay_ms(500);
    LPC_GPIO1->FIOCLR = BUZZER_PIN;
   
    delay_ms(2000);
}

/* ================= MAIN ================= */
int main(void){
    uint8_t msg_index = 0;
    uint32_t last_scroll_time = 0;
    uint32_t last_status_time = 0;
    char uart_buf[150];
   
    SystemInit();
   
    // Initialize Timer0 for DHT11
    LPC_SC->PCONP |= (1 << 1);
    Timer0_Init();
   
    // Initialize Timer1 for system time
    Timer1_Init();
   
    // Initialize GPIO
    GPIO_Init();
   
    // Initialize ADC
    ADC_Init();
   
    // Initialize UART
    UART0_Init();
    UART3_Init();
   
    // Initialize LCD
    initLCD();
   
    // Record session start time
    session_start_time = system_time_ms;
   
    // Welcome screen
    command(0x80);
    sendString("Audience Counter");
    command(0xC0);
    sendString("System Ready...");
    delay_ms(2000);
   
    UART0_SendString("=== AUDITORIUM SYSTEM STARTED ===\r\n");
    UART3_SendString("=== AUDITORIUM SYSTEM STARTED ===\r\n");
   
    // Show initial status
    displayStatus();
   
    while(1){
        uint32_t current_time = system_time_ms;
       
        // READ SENSORS EVERY 3 SECONDS
        if(sensor_timer++ >= 30){
            sensor_timer = 0;
            
            uint8_t t = 0, h = 0;
            if(dht11Read(&h, &t)){
                if(t > 0 && h > 0){
                    temperature = t;
                    humidity = h;
                }
            }
            
            air_quality = ADC_Read();
            
            // Send in JSON format for ESP32/Zoho Cloud
            sprintf(uart_buf, "ENV,{\"temp\":%d,\"hum\":%d,\"air\":%u,\"inside\":%lu}\r\n",
                    (int)temperature, (int)humidity, (unsigned int)air_quality, (unsigned long)audience_count);
            UART0_SendString(uart_buf);
            UART3_SendString(uart_buf);
        }
       
        /* ===== CHECK IR SENSORS ===== */
        // Entry sensor (active LOW when triggered)
        if(!(LPC_GPIO1->FIOPIN & IR_ENTRY_PIN)){
            handleEntry();
            displayStatus();
        }
       
        // Exit sensor (active LOW when triggered)
        if(!(LPC_GPIO1->FIOPIN & IR_EXIT_PIN)){
            handleExit();
            displayStatus();
        }
       
        /* ===== CHECK RESET BUTTON ===== */
        if(!(LPC_GPIO1->FIOPIN & RESET_BTN_PIN)){
            delay_ms(50);  // Debounce
            if(!(LPC_GPIO1->FIOPIN & RESET_BTN_PIN)){
                while(!(LPC_GPIO1->FIOPIN & RESET_BTN_PIN));  // Wait for release
                handleReset();
                displayStatus();
                session_start_time = system_time_ms;  // Reset session time
                total_entries = 0;
                total_exits = 0;
            }
        }
       
        /* ===== SCROLL WELCOME MESSAGE (every 5 seconds) ===== */
        if((current_time - last_scroll_time) >= 5000 && audience_count < MAX_CAPACITY){
            scrollMessage(welcome_messages[msg_index]);
            msg_index = (msg_index + 1) % MSG_COUNT;
            last_scroll_time = current_time;
            displayStatus();
        }
       
        /* ===== UPDATE STATUS (every 10 seconds) ===== */
        if((current_time - last_status_time) >= 10000){
            // Show environmental data
            command(0x01);
            delay_ms(2);
            command(0x80);
            char env[17];
            sprintf(env, "T:%dC H:%d%%     ", temperature, humidity);
            sendString(env);
            command(0xC0);
            sprintf(env, "Air: %u       ", air_quality);
            sendString(env);
            delay_ms(3000);
            
            // Show session time
            command(0x01);
            delay_ms(2);
            command(0x80);
            sendString("Session Time:");
            command(0xC0);
            sendString(formatTime(current_time - session_start_time));
            delay_ms(3000);
           
            // Show statistics
            command(0x01);
            delay_ms(2);
            command(0x80);
            char stats[17];
            sprintf(stats, "In:%lu Out:%lu  ", total_entries, total_exits);
            sendString(stats);
            command(0xC0);
            sprintf(stats, "Current: %lu    ", audience_count);
            sendString(stats);
            delay_ms(3000);
           
            displayStatus();
            last_status_time = current_time;
        }
       
        delay_ms(100);  // Small delay to reduce CPU load
    }
}