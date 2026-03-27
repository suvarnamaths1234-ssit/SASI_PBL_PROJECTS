#include <LPC17xx.h>
#include <stdio.h>

/* ================= PIN DEFINITIONS ================= */

/* ---- IR Sensors ---- */
#define ENTRY_IR   (1<<1)   // P2.1
#define EXIT_IR    (1<<2)   // P2.2

/* ---- DHT11 ---- */
#define DHT11_PIN  (1<<0)   // P2.0

/* ---- BUZZER ---- */
#define BUZZER     (1<<27)  // P1.27

/* ---- LCD (PORT0) ---- */
#define LCD_RS  (1<<10)     // P0.10
#define LCD_EN  (1<<11)     // P0.11
#define LCD_D4  (1<<19)     // P0.19
#define LCD_D5  (1<<20)     // P0.20
#define LCD_D6  (1<<21)     // P0.21
#define LCD_D7  (1<<22)     // P0.22
#define LCD_DATA (LCD_D4|LCD_D5|LCD_D6|LCD_D7)

/* ---- SEAT CONFIG ---- */
#define TOTAL_SEATS 10

/* ================= UART FUNCTIONS ================= */
void UART0_Init(void){
    LPC_SC->PCONP |= (1 << 3);
    LPC_PINCON->PINSEL0 &= ~(0xF << 4);
    LPC_PINCON->PINSEL0 |= (0x5 << 4);
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLL = 0xA2;
    LPC_UART0->DLM = 0x00;
    LPC_UART0->LCR = 0x03;
    LPC_UART0->FCR = 0x07;
}

void UART3_Init(void){
    LPC_SC->PCONP |= (1 << 25);
    LPC_PINCON->PINSEL0 &= ~(0xF << 0);
    LPC_PINCON->PINSEL0 |= (0xA << 0);
    LPC_UART3->LCR = 0x83;
    LPC_UART3->DLL = 0xA2;
    LPC_UART3->DLM = 0x00;
    LPC_UART3->LCR = 0x03;
    LPC_UART3->FCR = 0x07;
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

/* ================= GLOBAL VARIABLES ================= */
uint8_t dht11_data[5];
float temperature = 0, humidity = 0;
int occupied = 0;
char buf[120];

/* ================= DELAY FUNCTIONS ================= */
void delay_us(uint32_t us)
{
    for(volatile uint32_t i=0;i<us*12;i++);
}

void delay_ms(uint32_t ms)
{
    for(uint32_t i=0;i<ms;i++)
        for(volatile uint32_t j=0;j<8000;j++);
}

/* ================= LCD FUNCTIONS ================= */
void lcd_pulse(void)
{
    LPC_GPIO0->FIOSET = LCD_EN;
    delay_us(50);
    LPC_GPIO0->FIOCLR = LCD_EN;
}

void lcd_send(uint8_t data)
{
    LPC_GPIO0->FIOCLR = LCD_DATA;
    LPC_GPIO0->FIOSET = (data << 19);
    lcd_pulse();
}

void lcd_cmd(uint8_t cmd)
{
    LPC_GPIO0->FIOCLR = LCD_RS;
    lcd_send(cmd >> 4);
    lcd_send(cmd & 0x0F);
    delay_ms(2);
}

void lcd_data(uint8_t data)
{
    LPC_GPIO0->FIOSET = LCD_RS;
    lcd_send(data >> 4);
    lcd_send(data & 0x0F);
    delay_ms(2);
}

void lcd_init(void)
{
    LPC_GPIO0->FIODIR |= LCD_RS | LCD_EN | LCD_DATA;
    delay_ms(20);
    lcd_cmd(0x28);    // 4-bit, 2-line
    lcd_cmd(0x0C);    // Display ON
    lcd_cmd(0x06);    // Cursor increment
    lcd_cmd(0x01);    // Clear LCD
}

void lcd_print(char *s)
{
    while(*s) lcd_data(*s++);
}

/* ================= DHT11 FUNCTIONS ================= */
uint8_t read_dht11(void)
{
    uint8_t i,j;

    for(i=0;i<5;i++) dht11_data[i]=0;

    LPC_GPIO2->FIODIR |= DHT11_PIN;
    LPC_GPIO2->FIOCLR = DHT11_PIN;
    delay_ms(20);

    LPC_GPIO2->FIOSET = DHT11_PIN;
    delay_us(30);
    LPC_GPIO2->FIODIR &= ~DHT11_PIN;

    while(LPC_GPIO2->FIOPIN & DHT11_PIN);
    while(!(LPC_GPIO2->FIOPIN & DHT11_PIN));
    while(LPC_GPIO2->FIOPIN & DHT11_PIN);

    for(i=0;i<5;i++)
    {
        for(j=0;j<8;j++)
        {
            while(!(LPC_GPIO2->FIOPIN & DHT11_PIN));
            delay_us(30);
            if(LPC_GPIO2->FIOPIN & DHT11_PIN)
                dht11_data[i] |= (1<<(7-j));
            while(LPC_GPIO2->FIOPIN & DHT11_PIN);
        }
    }

    if((dht11_data[0]+dht11_data[1]+dht11_data[2]+dht11_data[3]) != dht11_data[4])
        return 0;

    humidity = dht11_data[0];
    temperature = dht11_data[2];
    return 1;
}

/* ================= MQ135 (ADC) ================= */
void adc_init(void)
{
    LPC_SC->PCONP |= (1<<12);
    LPC_PINCON->PINSEL1 |= (1<<18);  // P0.25 ? AD0.2
    LPC_ADC->ADCR = (1<<2)|(4<<8)|(1<<21);
}

uint16_t read_mq135(void)
{
    LPC_ADC->ADCR |= (1<<24);
    while(!(LPC_ADC->ADGDR & (1<<31)));
    return (LPC_ADC->ADGDR >> 4) & 0xFFF;
}

/* ================= MAIN FUNCTION ================= */
int main(void)
{
    int available;
    uint16_t mq_adc;

    /* IR as input */
    LPC_GPIO2->FIODIR &= ~(ENTRY_IR | EXIT_IR);
    
    /* Buzzer as output and turn OFF initially */
    LPC_GPIO1->FIODIR |= BUZZER;
    LPC_GPIO1->FIOCLR = BUZZER;

    /* Initialize UART */
    UART0_Init();
    UART3_Init();
    delay_ms(100);

    while(1)
    {
        lcd_init();
        adc_init();

        /* ===== WELCOME MESSAGE ===== */
        lcd_cmd(0x01);
        lcd_print("WELCOME");
        lcd_cmd(0xC0);
        lcd_print("Suvarna Monitoring");
        delay_ms(3000);
        lcd_cmd(0x01);
        
        /* -------- Seat Monitoring -------- */
                /* ENTRY */
        if(!(LPC_GPIO2->FIOPIN & ENTRY_IR) && occupied<TOTAL_SEATS)
        {
            occupied++;
            lcd_cmd(0x01);
            lcd_print("Person Entered");
            UART0_SendString("ENTRY DETECTED\r\n");
            UART3_SendString("ENTRY DETECTED\r\n");
            delay_ms(800);
        }

                /* EXIT */
        if(!(LPC_GPIO2->FIOPIN & EXIT_IR) && occupied>0)
        {
            occupied--;
            lcd_cmd(0x01);
            lcd_print("Person Exited");
            UART0_SendString("EXIT DETECTED\r\n");
            UART3_SendString("EXIT DETECTED\r\n");
            delay_ms(800);
        }


        available = TOTAL_SEATS - occupied;

                /* SEATS */
        lcd_cmd(0x01);
        sprintf(buf,"Total:%d",TOTAL_SEATS);
        lcd_print(buf);
        lcd_cmd(0xC0);
        sprintf(buf,"Avail:%d",available);
        lcd_print(buf);
        delay_ms(2000);

        
        // Send to UART
        sprintf(buf,"Total:%d Avail:%d\r\n",TOTAL_SEATS,available);
        UART0_SendString(buf);
        UART3_SendString(buf);
        
        delay_ms(2000);

        /* -------- Temp & Hum -------- */
        lcd_cmd(0x01);
        if(read_dht11())
        {
            sprintf(buf,"Temp:%.0fC",temperature);
            lcd_print(buf);
            lcd_cmd(0xC0);
            sprintf(buf,"Hum :%.0f%%",humidity);
            lcd_print(buf);
            
            // Send to UART - using raw dht11_data
            sprintf(buf,"Temp:%dC Hum:%d%%\r\n",dht11_data[2],dht11_data[0]);
            UART0_SendString(buf);
            UART3_SendString(buf);
        }
        else
        {
            lcd_print("DHT11 ERROR");
            UART0_SendString("DHT11 ERROR\r\n");
            UART3_SendString("DHT11 ERROR\r\n");
        }
        delay_ms(2000);

        /* -------- Air Quality -------- */
        mq_adc = read_mq135();

        lcd_cmd(0x01);
        sprintf(buf,"Air:%d ppm",mq_adc);
        lcd_print(buf);
				UART0_SendString(buf);
				UART3_SendString(buf);


        lcd_cmd(0xC0);
        if(mq_adc < 1500)
        {
            lcd_print("Good");
            LPC_GPIO1->FIOCLR = BUZZER;// Turn OFF buzzer
        }
        else if(mq_adc < 2500)
        {
            lcd_print("Moderate");
            LPC_GPIO1->FIOCLR = BUZZER;  // Turn OFF buzzer
        
        }
        else
        {
            lcd_print("Bad");
            LPC_GPIO1->FIOSET = BUZZER;  // Turn ON buzzer
        
         }
        
        // Send to UART
        sprintf(buf,"Air:%d ppm ",mq_adc);
        UART0_SendString(buf);
        UART3_SendString(buf);
        
        if(mq_adc < 1500)
        {
            UART0_SendString("Good\r\n");
            UART3_SendString("Good\r\n");
        }
        else if(mq_adc < 2500)
        {
            UART0_SendString("Moderate\r\n");
            UART3_SendString("Moderate\r\n");
        }
        else
        {
            UART0_SendString("Bad\r\n");
            UART3_SendString("Bad\r\n");
        }
        
        delay_ms(2000);
        
        /* -------- HALL FULL CHECK -------- */
        if(occupied >= TOTAL_SEATS)
        {
            lcd_cmd(0x01);
            lcd_print("HALL FULL");
            lcd_cmd(0xC0);
            lcd_print("NO ENTRY");
            LPC_GPIO1->FIOSET = BUZZER;  // Turn ON buzzer
            delay_ms(2000);
        }
    }
}