#include <lpc17xx.h>
#include "MFRC.h"
#include "DHT11_header.h"
#include <stdio.h>
#include <string.h>

/* ================= DEFINES ================= */
#define MQ135_CH        0          // AD0.0 P0.23
#define BUZZER_PIN      (1 << 27)  // P1.27 in-built buzzer

#define TEMP_DANGER     45
#define HUM_DANGER      85
#define AIR_DANGER      3200
#define AIR_SAFE        2600       // hysteresis

/* ================= UART ================= */
void UART0_Init(unsigned int);
void UART3_Init(unsigned int);
void UART0_SendChar(char);
void UART3_SendChar(char);
void UART0_SendString(const char *);
void UART3_SendString(const char *);
void delay_ms(unsigned int);

/* ================= GLOBAL ================= */
uint8_t temperature = 0;
uint8_t humidity = 0;
uint32_t dhtCounter = 0;

uint8_t alarm_active = 0;
uint8_t admin_notified = 0;

/* ================= USER DATABASE ================= */
typedef struct {
    uint8_t uid[5];
    const char *name;
    uint8_t inside;
} User;

User users[] = {
    {{0xF3,0x52,0x22,0x2A,0xA9}, "Suvarna", 0},
    {{0xC3,0x0E,0x15,0x08,0xD0}, "Sudheer", 0},
    {{0xD3,0xF8,0x5D,0xEC,0x9A}, "Guru",    0},
    {{0x33,0x84,0xD0,0xEC,0x8B}, "Akash",   0}
};

#define USER_CNT (sizeof(users) / sizeof(users[0]))

/* ================= UTILS ================= */
uint8_t cmpUID(uint8_t *a, uint8_t *b){
    for(int i = 0; i < 5; i++)
        if(a[i] != b[i]) return 0;
    return 1;
}

const char* getName(uint8_t *uid){
    for(int i = 0; i < USER_CNT; i++)
        if(cmpUID(uid, users[i].uid))
            return users[i].name;
    return "Unknown";
}

const char* getStatus(uint8_t *uid){
    for(int i = 0; i < USER_CNT; i++){
        if(cmpUID(uid, users[i].uid)){
            users[i].inside ^= 1;   // toggle
            return users[i].inside ? "ENTRY" : "EXIT";
        }
    }
    return "UNKNOWN";
}

uint8_t getInsideCount(void){
    uint8_t count = 0;
    for(int i = 0; i < USER_CNT; i++)
        if(users[i].inside) count++;
    return count;
}

/* ================= ADC (MQ135) ================= */
void ADC_Init(void){
    LPC_SC->PCONP |= (1 << 12);
    LPC_PINCON->PINSEL1 &= ~(3 << 14);
    LPC_PINCON->PINSEL1 |=  (1 << 14);   // P0.23 AD0.0
    LPC_ADC->ADCR = (4 << 8) | (1 << 21);
}

uint16_t ADC_Read(void){
    LPC_ADC->ADCR &= ~0xFF;
    LPC_ADC->ADCR |= (1 << MQ135_CH);
    LPC_ADC->ADCR |= (1 << 24);
    while(!(LPC_ADC->ADGDR & (1 << 31)));
    return (LPC_ADC->ADGDR >> 4) & 0xFFF;
}

/* ================= BUZZER ================= */
void buzzer_short(void){
    LPC_GPIO1->FIOSET = BUZZER_PIN;
    delay_ms(200);
    LPC_GPIO1->FIOCLR = BUZZER_PIN;
}

void buzzer_long(void){
    LPC_GPIO1->FIOSET = BUZZER_PIN;
    delay_ms(600);
    LPC_GPIO1->FIOCLR = BUZZER_PIN;
}

/* ================= MAIN ================= */
int main(void)
{
    uint8_t uid[5], type[2];
    char buf[220];

    UART0_Init(9600);
    UART3_Init(9600);
    ADC_Init();

    LPC_GPIO1->FIODIR |= BUZZER_PIN;
    LPC_GPIO1->FIOCLR  = BUZZER_PIN;

    SSP1_Init();
    MFRC522_Init();

    UART0_SendString("MINE SAFETY SYSTEM STARTED\r\n");
    UART3_SendString("LPC1768_READY\r\n");

    while(1)
    {
        /* -------- DHT11 + ENV (every ~3 sec) -------- */
        if(dhtCounter++ >= 60){
            dhtCounter = 0;
            uint8_t t, h;
            if(dht11Read(&h, &t)){
                if(t > 0 && h > 0){
                    temperature = t;
                    humidity = h;

                    uint8_t inside = getInsideCount();
                    uint16_t air = ADC_Read();

                    sprintf(buf,
                      "ENV,{\"temp\":%d,\"hum\":%d,\"air\":%d,\"inside\":%d}\r\n",
                      temperature, humidity, air, inside);

                    UART3_SendString(buf);
                }
            }
        }

        /* -------- RFID -------- */
        if(MFRC522_Request(PICC_REQIDL, type) == MI_OK){
            if(MFRC522_Anticoll(uid) == MI_OK){
                const char *name = getName(uid);
                const char *st   = getStatus(uid);
                uint8_t inside   = getInsideCount();

                sprintf(buf,
                  "RFID,{\"name\":\"%s\",\"status\":\"%s\","
                  "\"uid\":\"%02X%02X%02X%02X%02X\",\"inside\":%d}\r\n",
                  name, st,
                  uid[0], uid[1], uid[2], uid[3], uid[4],
                  inside);

                UART0_SendString(buf);
                UART3_SendString(buf);

                if(strcmp(st, "ENTRY") == 0) buzzer_short();
                else buzzer_long();

                delay_ms(1500);
            }
        }

        /* -------- SAFETY & EMERGENCY -------- */
        uint16_t air = ADC_Read();
        uint8_t workers = getInsideCount();

        if(workers > 0 &&
           (air > AIR_DANGER ||
            temperature > TEMP_DANGER ||
            humidity > HUM_DANGER))
        {
            if(!alarm_active){
                alarm_active = 1;
                admin_notified = 0;
                LPC_GPIO1->FIOSET = BUZZER_PIN;   // siren ON
            }

            if(!admin_notified){
                admin_notified = 1;

                sprintf(buf,
                  "ALERT,{\"type\":\"EMERGENCY\","
                  "\"air\":%d,\"temp\":%d,\"hum\":%d,\"inside\":%d}\r\n",
                  air, temperature, humidity, workers);

                UART3_SendString(buf);
            }
        }
        else
        {
            if(alarm_active && air < AIR_SAFE){
                alarm_active = 0;
                admin_notified = 0;
                LPC_GPIO1->FIOCLR = BUZZER_PIN;

                UART3_SendString(
                  "ALERT,{\"type\":\"CLEARED\"}\r\n"
                );
            }
        }

        delay_ms(50);
    }
}

/* ================= UART ================= */
void UART0_Init(unsigned int b){
    LPC_SC->PCONP |= (1 << 3);
    LPC_PINCON->PINSEL0 |= (0x5 << 4);
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLL = 0xA2;
    LPC_UART0->LCR = 0x03;
}

void UART3_Init(unsigned int b){
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

void delay_ms(unsigned int ms){
    for(unsigned int i = 0; i < ms; i++)
        for(unsigned int j = 0; j < 10000; j++);
}
