#include <lpc17xx.h>
#include "MFRC.h"
#include "LCD_header.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================= DEFINES ================= */
#define BUZZER_PIN      (1 << 27)
#define STUDENT_DANGER  3
#define ENTRY_COOLDOWN  5000   // 5 seconds cooldown
#define DOOR_OPEN_TIME  5000   // 
#define EXAM_DURATION   7200000 // 2 hours in milliseconds (120 minutes)

/* ================= KEYPAD MATRIX ================= */
// Rows: P2.0, P2.1, P2.2, P2.3 (Output)
// Cols: P2.4, P2.5, P2.6, P2.7 (Input with pull-up)
#define ROW_PINS  ((1<<0)|(1<<1)|(1<<2)|(1<<3))
#define COL_PINS  ((1<<4)|(1<<5)|(1<<6)|(1<<7))

const char keypad_map[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

/* ================= SERVO CONTROL ================= */
#define SERVO_CLOSED_POSITION  500
#define SERVO_OPEN_POSITION    2500

volatile uint8_t door_is_open = 0;
volatile uint32_t door_open_start_time = 0;

/* ================= EXAM TIMER ================= */
volatile uint8_t exam_started = 0;
volatile uint32_t exam_start_time = 0;
volatile uint8_t exam_ended = 0;

/* ================= UART BUFFER ================= */
volatile char uart_rx_buf[64];
volatile uint8_t uart_rx_idx = 0;

/* ================= UART ================= */
void UART0_Init(void);
void UART3_Init(void);
void UART0_SendChar(char c);
void UART3_SendChar(char c);
void UART0_SendString(const char *s);
void UART3_SendString(const char *s);
void delay_ms(unsigned int);

/* ================= STUDENT DATABASE ================= */
typedef struct {
    uint8_t uid[4];  // Changed to 4 bytes
    const char *name;
    char seat[10];
    uint8_t inside;
    uint32_t last_scan_time;
} Student;

Student students[] = {
    {{0xF3,0x52,0x22,0x2A}, "Suvarna", "", 0, 0},
    {{0xA5,0xD7,0x91,0x5F}, "Sudheer", "", 0, 0},
    {{0xD3,0xF8,0x5D,0xEC}, "Guru",    "", 0, 0},
    {{0x33,0x84,0xD0,0xEC}, "Akash",   "", 0, 0},
    {{0x1A,0x88,0x36,0x02}, "Student5", "", 0, 0},
    {{0x83,0x00,0x05,0xED}, "Student6", "", 0, 0}
};

#define STUDENT_CNT (sizeof(students)/sizeof(students[0]))

const char* available_seats[] = {
    "A-01", "A-02", "A-03", "A-04",
    "B-01", "B-02", "B-03", "B-04",
    "C-01", "C-02", "C-03", "C-04"
};
#define SEAT_CNT (sizeof(available_seats)/sizeof(available_seats[0]))

uint8_t seat_taken[SEAT_CNT] = {0};
volatile uint32_t system_time_ms = 0;

/* ================= KEYPAD FUNCTIONS ================= */
void Keypad_Init(void){
    // Rows as output (P2.0 to P2.3)
    LPC_GPIO2->FIODIR |= ROW_PINS;
    LPC_GPIO2->FIOSET = ROW_PINS;  // Set all rows HIGH initially
    
    // Columns as input (P2.4 to P2.7 with pull-up via PINMODE)
    LPC_GPIO2->FIODIR &= ~COL_PINS;
    LPC_PINCON->PINMODE4 &= ~(0xFF << 8);  // Clear bits for P2.4-P2.7
    // Pull-up is enabled by default (00)
}

char Keypad_Scan(void){
    uint8_t row, col;
    
    for(row = 0; row < 4; row++){
        // Set all rows HIGH
        LPC_GPIO2->FIOSET = ROW_PINS;
        
        // Set current row LOW
        LPC_GPIO2->FIOCLR = (1 << row);
        
        delay_ms(5);  // Debounce delay
        
        // Check columns
        for(col = 0; col < 4; col++){
            if(!(LPC_GPIO2->FIOPIN & (1 << (4 + col)))){
                // Key pressed
                while(!(LPC_GPIO2->FIOPIN & (1 << (4 + col))));  // Wait for release
                delay_ms(20);  // Debounce
                return keypad_map[row][col];
            }
        }
    }
    
    return 0;  // No key pressed
}

/* ================= SERVO PWM SETUP ================= */
void Servo_Init(void){
    LPC_SC->PCONP |= (1 << 6);
    LPC_PINCON->PINSEL4 &= ~(3 << 0);
    LPC_PINCON->PINSEL4 |= (1 << 0);
    
    LPC_PWM1->PR = 24;
    LPC_PWM1->MR0 = 20000;
    LPC_PWM1->MR1 = SERVO_CLOSED_POSITION;
    LPC_PWM1->MCR = 0x02;
    LPC_PWM1->PCR = (1 << 9);
    LPC_PWM1->LER = 0x03;
    LPC_PWM1->TCR = 0x09;
}

void Servo_SetPosition(uint16_t pulse_width){
    LPC_PWM1->MR1 = pulse_width;
    LPC_PWM1->LER = 0x02;
}

void Door_Open(void){
    UART0_SendString("DOOR: Opening...\r\n");
    Servo_SetPosition(SERVO_OPEN_POSITION);
    door_is_open = 1;
    door_open_start_time = system_time_ms;
}

void Door_Close(void){
    UART0_SendString("DOOR: Closing...\r\n");
    Servo_SetPosition(SERVO_CLOSED_POSITION);
    door_is_open = 0;
}

void Door_CheckAutoClose(void){
    if(door_is_open && (system_time_ms - door_open_start_time) >= DOOR_OPEN_TIME){
        Door_Close();
    }
}

/* ================= EXAM TIMER FUNCTIONS ================= */
void Exam_Start(void){
    if(!exam_started){
        exam_started = 1;
        exam_start_time = system_time_ms;
        exam_ended = 0;
        UART0_SendString("\r\n===== EXAM STARTED =====\r\n");
    }
}

void Exam_CheckTimeUp(void){
    if(exam_started && !exam_ended){
        if((system_time_ms - exam_start_time) >= EXAM_DURATION){
            exam_ended = 1;
            UART0_SendString("\r\n===== EXAM TIME UP! All students can exit =====\r\n");
            
            // Display on LCD
            command(0x01);
            delay_ms(2);
            command(0x80);
            sendString("EXAM COMPLETE!");
            command(0xC0);
            sendString("You may exit");
            
            // Long beep to signal exam end
            LPC_GPIO1->FIOSET = BUZZER_PIN;
            delay_ms(1000);
            LPC_GPIO1->FIOCLR = BUZZER_PIN;
            
            // Open door automatically for all students
            Door_Open();
            delay_ms(10000);  // Keep door open for 10 seconds
            Door_Close();
            
            // Reset all students to outside
            for(int i = 0; i < STUDENT_CNT; i++){
                students[i].inside = 0;
                strcpy(students[i].seat, "");
            }
            
            // Reset seats
            for(int i = 0; i < SEAT_CNT; i++){
                seat_taken[i] = 0;
            }
            
            exam_started = 0;
        }
    }
}

uint32_t Exam_GetRemainingTime(void){
    if(!exam_started || exam_ended) return 0;
    
    uint32_t elapsed = system_time_ms - exam_start_time;
    if(elapsed >= EXAM_DURATION) return 0;
    
    return EXAM_DURATION - elapsed;
}

/* ================= TIMER FOR SYSTEM TIME ================= */
void Timer1_Init(void){
    LPC_SC->PCONP |= (1 << 2);
    LPC_TIM1->TCR = 0x02;
    LPC_TIM1->PR = 25000 - 1;
    LPC_TIM1->MR0 = 1;
    LPC_TIM1->MCR = 0x03;
    NVIC_EnableIRQ(TIMER1_IRQn);
    LPC_TIM1->TCR = 0x01;
}

void TIMER1_IRQHandler(void){
    if(LPC_TIM1->IR & 0x01){
        LPC_TIM1->IR = 0x01;
        system_time_ms++;
    }
}

/* ================= SEAT ALLOCATION ================= */
void assignRandomSeat(Student *student) {
    uint8_t attempts = 0, seat_idx;
    
    if(strlen(student->seat) > 0) return;
    
    while(attempts < 100) {
        seat_idx = rand() % SEAT_CNT;
        if(!seat_taken[seat_idx]) {
            strcpy(student->seat, available_seats[seat_idx]);
            seat_taken[seat_idx] = 1;
            return;
        }
        attempts++;
    }
    strcpy(student->seat, "FULL");
}

/* ================= UTILS ================= */
uint8_t cmpUID(uint8_t *a, uint8_t *b){
    for(int i=0;i<4;i++)  // Changed to 4 bytes
        if(a[i]!=b[i]) return 0;
    return 1;
}

Student* getStudent(uint8_t *uid){
    for(int i=0;i<STUDENT_CNT;i++)
        if(cmpUID(uid,students[i].uid))
            return &students[i];
    return NULL;
}

uint8_t getInsideCount(void){
    uint8_t c=0;
    for(int i=0;i<STUDENT_CNT;i++)
        if(students[i].inside) c++;
    return c;
}

/* ================= LCD DISPLAY ================= */
void displayStudentInfo(const char *name, const char *seat, const char *status){
    char buffer[20];
    command(0x01);
    delay_ms(2);
    command(0x80);
    sendString((char*)name);
    command(0xC0);
    sprintf(buffer, "Seat:%s %s", seat, status);
    sendString(buffer);
}

void displayError(const char *line1, const char *line2){
    command(0x01);
    delay_ms(2);
    command(0x80);
    sendString((char*)line1);
    command(0xC0);
    sendString((char*)line2);
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

void buzzer_error(void){
    for(int i=0; i<3; i++){
        LPC_GPIO1->FIOSET = BUZZER_PIN;
        delay_ms(150);
        LPC_GPIO1->FIOCLR = BUZZER_PIN;
        delay_ms(150);
    }
}

/* ================= UART0 INTERRUPT ================= */
void UART0_IRQHandler(void){
    if(LPC_UART0->IIR & 0x04) {
        char c = LPC_UART0->RBR;
        if(uart_rx_idx < sizeof(uart_rx_buf) - 1){
            uart_rx_buf[uart_rx_idx++] = c;
        }
    }
}

/* ================= MAIN ================= */
int main(void)
{
    uint8_t uid[5], type[2];
    char buf[220], key;
    Student *student;
    uint32_t current_time;

    LPC_SC->PCONP |= (1 << 1);
    Timer1_Init();
    Servo_Init();
    Keypad_Init();

    UART0_Init();
    UART3_Init();

    LPC_PINCON->PINSEL0 &= ~(3 << 20);
    LPC_PINCON->PINSEL0 &= ~(3 << 22);
    LPC_PINCON->PINSEL1 &= ~(0xFF << 6);
    LPC_GPIO0->FIODIR |= RS | ENABLE | DATA;
    LPC_GPIO0->FIOCLR  = RS | ENABLE | DATA;
    
    initLCD();
    
    command(0x80);
    sendString("RFID Exam System");
    command(0xC0);
    sendString("Ready...");
    delay_ms(2000);
    command(0x01);

    LPC_GPIO1->FIODIR |= BUZZER_PIN;
    LPC_GPIO1->FIOCLR  = BUZZER_PIN;

    SSP1_Init();
    MFRC522_Init();

    srand(system_time_ms);

    UART0_SendString("\r\n===== RFID EXAM DOOR SYSTEM =====\r\n");
    UART0_SendString("ENTRY: Scan RFID outside\r\n");
    UART0_SendString("EXIT: Auto after 2 hours OR press '#' on keypad\r\n");
    UART3_SendString("LPC1768_READY\r\n");
    
    Door_Close();

    while(1)
    {
        Door_CheckAutoClose();
        Exam_CheckTimeUp();

        /* ===== EMERGENCY EXIT KEYPAD ===== */
        key = Keypad_Scan();
        if(key == '#'){  // '#' key for emergency exit
            UART0_SendString("\r\nEMERGENCY EXIT pressed!\r\n");
            
            displayError("EMERGENCY EXIT", "Door Opening");
            buzzer_long();
            Door_Open();
            
            delay_ms(3000);
            
            command(0x01);
            delay_ms(2);
            command(0x80);
            sendString("RFID Exam System");
            command(0xC0);
            sendString("Ready...");
        }

        if(uart_rx_idx > 0){
            uart_rx_buf[uart_rx_idx] = '\0';
            UART0_SendString("\r\nRX: ");
            UART0_SendString((char*)uart_rx_buf);
            UART0_SendString("\r\n");
            uart_rx_idx = 0;
        }

        /* ===== RFID ENTRY ===== */
        if(MFRC522_Request(PICC_REQIDL,type)==MI_OK){
            if(MFRC522_Anticoll(uid)==MI_OK){
                student = getStudent(uid);
                current_time = system_time_ms;
                
                if(student == NULL){
                    displayError("Unknown Card!", "Access Denied");
                    UART0_SendString("Unknown RFID - Access Denied\r\n");
                    buzzer_error();
                    delay_ms(2000);
                    
                    command(0x01);
                    delay_ms(2);
                    command(0x80);
                    sendString("RFID Exam System");
                    command(0xC0);
                    sendString("Ready...");
                    continue;
                }
                
                if((current_time - student->last_scan_time) < ENTRY_COOLDOWN){
                    displayError("Please Wait", "Scan Again");
                    delay_ms(1500);
                    
                    command(0x01);
                    delay_ms(2);
                    command(0x80);
                    sendString("RFID Exam System");
                    command(0xC0);
                    sendString("Ready...");
                    continue;
                }
                
                student->last_scan_time = current_time;
                
                if(student->inside == 0){
                    // ===== ENTRY MODE =====
                    if(strlen(student->seat) == 0){
                        assignRandomSeat(student);
                    }
                    
                    student->inside = 1;
                    
                    // Start exam timer on first entry
                    if(getInsideCount() == 1){
                        Exam_Start();
                    }
                    
                    displayStudentInfo(student->name, student->seat, "ENTRY");
                    
                    UART0_SendString("ENTRY: ");
                    UART0_SendString(student->name);
                    UART0_SendString(" -> Seat ");
                    UART0_SendString(student->seat);
                    UART0_SendString("\r\n");
                    
                    buzzer_short();
                    Door_Open();
                    
                }
                else{
                    // ===== CARD ALREADY USED =====
                    displayError("CARD ALREADY", "USED! BLOCKED");
                    
                    UART0_SendString("SECURITY: ");
                    UART0_SendString(student->name);
                    UART0_SendString("'s card already in use!\r\n");
                    
                    buzzer_error();
                    
                    delay_ms(3000);
                    
                    command(0x01);
                    delay_ms(2);
                    command(0x80);
                    sendString("RFID Exam System");
                    command(0xC0);
                    sendString("Ready...");
                    continue;
                }
                
                uint8_t inside = getInsideCount();

                sprintf(buf,
                  "RFID,{\"name\":\"%s\",\"status\":\"ENTRY\","
                  "\"uid\":\"%02X%02X%02X%02X\",\"seat\":\"%s\",\"inside\":%d}\r\n",
                  student->name,
                  uid[0], uid[1], uid[2], uid[3],
                  student->seat, inside);

                UART3_SendString(buf);
                
                delay_ms(3000);
                
                command(0x01);
                delay_ms(2);
                command(0x80);
                sendString("RFID Exam System");
                command(0xC0);
                sendString("Ready...");
            }
        }

        if(getInsideCount() > STUDENT_DANGER){
            LPC_GPIO1->FIOSET = BUZZER_PIN;
        }else{
            LPC_GPIO1->FIOCLR = BUZZER_PIN;
        }
    }
}

/* ================= UART FUNCTIONS ================= */
void UART0_Init(void){
    LPC_SC->PCONP |= (1<<3);
    LPC_PINCON->PINSEL0 |= (0x5<<4);
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLL = 0xA2;
    LPC_UART0->LCR = 0x03;
    LPC_UART0->IER = 0x01;
    NVIC_EnableIRQ(UART0_IRQn);
}

void UART3_Init(void){
    LPC_SC->PCONP |= (1<<25);
    LPC_PINCON->PINSEL0 |= (0xA<<0);
    LPC_UART3->LCR = 0x83;
    LPC_UART3->DLL = 0xA2;
    LPC_UART3->LCR = 0x03;
}

void UART0_SendChar(char c){
    while(!(LPC_UART0->LSR & (1<<5)));
    LPC_UART0->THR = c;
}

void UART3_SendChar(char c){
    while(!(LPC_UART3->LSR & (1<<5)));
    LPC_UART3->THR = c;
}

void UART0_SendString(const char *s){
    while(*s) UART0_SendChar(*s++);
}

void UART3_SendString(const char *s){
    while(*s) UART3_SendChar(*s++);
}

void delay_ms(unsigned int ms){
    for(unsigned int i=0;i<ms;i++)
        for(unsigned int j=0;j<10000;j++);
}