/**
* ============================================
* RFID ENTRY-EXIT SYSTEM - BATCH-3
* Version: 3.9 - JSON Cloud Integration
* MAX CAPACITY: 9 PEOPLE
* Date: December 30, 2025
* ============================================
*/

#include "LPC17xx.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "uart.h"
#include "SSP0.h"
#include "RC522_RFID.h"
#include "DELAY.h"
#include "LCD.h"
#include "DHT11.h"
#include "MQ135.h"
#include "globals.h"
#include "UART3.h"


// ============================================
// PIN DEFINITIONS
// ============================================
#define RC522_RST_PIN (1<<1)
#define BUZZER_PIN (1<<27)
#define SERVO_PIN (1<<5)
#define EMERGENCY_BUTTON (1<<11)
#define DHT11_PIN (1<<7)

// LED Pins P1.19 to P1.25 (7 LEDs)
#define LED1_PIN (1<<19)
#define LED2_PIN (1<<20)
#define LED3_PIN (1<<21)
#define LED4_PIN (1<<22)
#define LED5_PIN (1<<23)
#define LED6_PIN (1<<24)
#define LED7_PIN (1<<25)
#define LED_ALL_PINS (0x7F<<19)

// ============================================
// SYSTEM CONFIGURATION
// ============================================
#define MAX_ROOM_CAPACITY 9
#define GATE_OPEN_TIME 3
#define MAX_CARDS 10
#define SENSOR_READ_INTERVAL 600  // Every 60 seconds (600 * 100ms)
#define LCD_UPDATE_INTERVAL 30

// ============================================
// CARD STRUCTURE
// ============================================
typedef struct {
    uint8_t uid[4];
    char card_name[16];
    char group_name[16];
    uint16_t scan_count;
    uint8_t is_active;
    uint8_t is_inside;
    uint32_t last_scan_time;
} Card_t;

typedef struct {
    int16_t total_people_inside;
    uint8_t gate_open;
    uint8_t gate_busy;
    float temperature;
    float humidity;
    uint16_t air_quality;
    uint32_t system_uptime;
    uint16_t total_entries;
    uint16_t total_exits;
} SystemState_t;

// ============================================
// ALL 10 CARDS
// ============================================
Card_t cards[MAX_CARDS] = {
    {{0xF3, 0x52, 0x22, 0x2A}, "A0", "FOUR MEM GRP", 0, 1, 0, 0},
    {{0x83, 0x00, 0x05, 0xED}, "A1", "FOUR MEM GRP", 0, 1, 0, 0},
    {{0x33, 0x84, 0xD0, 0xEC}, "A2", "FOUR MEM GRP", 0, 1, 0, 0},
    {{0xD3, 0xF8, 0x5D, 0xEC}, "A3", "FOUR MEM GRP", 0, 1, 0, 0},

    {{0x35, 0x64, 0x94, 0x5F}, "B0", "THREE MEM GRP", 0, 1, 0, 0},
    {{0x03, 0x22, 0x3C, 0xED}, "B1", "THREE MEM GRP", 0, 1, 0, 0},
    {{0x1A, 0x88, 0x36, 0x02}, "B2", "THREE MEM GRP", 0, 1, 0, 0},

    {{0xD4, 0xC8, 0x7D, 0x05}, "C0", "TWO MEM GRP", 0, 1, 0, 0},
    {{0x3B, 0x3D, 0x7D, 0x05}, "C1", "TWO MEM GRP", 0, 1, 0, 0},

    {{0xA5, 0xD7, 0x91, 0x5F}, "D0", "ONE MEM GRP", 0, 1, 0, 0}
};

// ============================================
// SYSTEM STATE
// ============================================
SystemState_t system_state = {0, 0, 0, 0.0f, 0.0f, 0, 0, 0, 0};
char uart_buf[512];
char temp_str[8] = "---";
char hum_str[8] = "---";
char air_str[8] = "---";
volatile uint32_t system_tick = 0;

// ============================================
// JSON HELPER FUNCTIONS
// ============================================
void send_json_system_status(void) {
    sprintf(uart_buf,
        "STATUS,{\"type\":\"SYSTEM_STATUS\","
        "\"inside\":%d,\"capacity\":%d,"
        "\"entries\":%d,\"exits\":%d,"
        "\"temp\":%.1f,\"hum\":%.1f,"
        "\"air\":%d,\"air_status\":\"%s\","
        "\"uptime\":%lu}\r\n",
        system_state.total_people_inside, MAX_ROOM_CAPACITY,
        system_state.total_entries, system_state.total_exits,
        system_state.temperature, system_state.humidity,
        system_state.air_quality, MQ135_GetStatusString(),
        system_state.system_uptime);
    uart_dual_send_string(uart_buf);
}

void send_json_rfid_scan(Card_t *card, const char *action, uint8_t success) {
    sprintf(uart_buf,
        "RFID,{\"type\":\"CARD_SCAN\","
        "\"card\":\"%s\",\"group\":\"%s\","
        "\"uid\":\"%02X:%02X:%02X:%02X\","
        "\"action\":\"%s\",\"success\":%d,"
        "\"inside\":%d,\"capacity\":%d,"
        "\"scan_count\":%d}\r\n",
        card->card_name, card->group_name,
        card->uid[0], card->uid[1], card->uid[2], card->uid[3],
        action, success,
        system_state.total_people_inside, MAX_ROOM_CAPACITY,
        card->scan_count);
    uart_dual_send_string(uart_buf);
}

void send_json_unknown_card(uint8_t *uid) {
    sprintf(uart_buf,
        "RFID,{\"type\":\"UNKNOWN_CARD\","
        "\"uid\":\"%02X:%02X:%02X:%02X\","
        "\"status\":\"DENIED\"}\r\n",
        uid[0], uid[1], uid[2], uid[3]);
    uart_dual_send_string(uart_buf);
}

void send_json_gate_event(const char *event) {
    sprintf(uart_buf,
        "GATE,{\"type\":\"GATE_EVENT\","
        "\"event\":\"%s\","
        "\"inside\":%d}\r\n",
        event, system_state.total_people_inside);
    uart_dual_send_string(uart_buf);
}

void send_json_emergency(void) {
    sprintf(uart_buf,
        "ALERT,{\"type\":\"EMERGENCY\","
        "\"inside\":%d,"
        "\"temp\":%.1f,\"hum\":%.1f,"
        "\"air\":%d}\r\n",
        system_state.total_people_inside,
        system_state.temperature, system_state.humidity,
        system_state.air_quality);
    uart_dual_send_string(uart_buf);
}

void send_json_sensor_data(void) {
    sprintf(uart_buf,
        "ENV,{\"type\":\"SENSOR_DATA\","
        "\"temp\":%.1f,\"hum\":%.1f,"
        "\"air\":%d,\"air_status\":\"%s\","
        "\"inside\":%d,"
        "\"temp_str\":\"%s\",\"hum_str\":\"%s\",\"air_str\":\"%s\"}\r\n",
        system_state.temperature, system_state.humidity,
        system_state.air_quality, MQ135_GetStatusString(),
        system_state.total_people_inside,
        temp_str, hum_str, air_str);
    uart_dual_send_string(uart_buf);
}

void send_json_system_init(const char *stage) {
    sprintf(uart_buf,
        "INIT,{\"type\":\"SYSTEM_INIT\","
        "\"stage\":\"%s\","
        "\"status\":\"OK\"}\r\n",
        stage);
    uart_dual_send_string(uart_buf);
}

// ============================================
// LCD HELPER FUNCTIONS
// ============================================
void lcd_display_centered(uint8_t row, const char *text) {
    char line[17] = {0};
    uint8_t len = strlen(text);
    uint8_t padding = 0;

    if(len < 16) {
        padding = (16 - len) / 2;
    }

    for(uint8_t i = 0; i < 16; i++) {
        if(i < padding || i >= (padding + len)) {
            line[i] = ' ';
        } else {
            line[i] = text[i - padding];
        }
    }
    line[16] = '\0';

    lcd_goto(row, 0);
    lcd_string(line);
}

// ============================================
// LED FUNCTIONS
// ============================================
void led_init(void) {
    LPC_GPIO1->FIODIR |= LED_ALL_PINS;
    LPC_GPIO1->FIOCLR = LED_ALL_PINS;
}

void led_all_on(void) {
    LPC_GPIO1->FIOSET = LED_ALL_PINS;
}

void led_all_off(void) {
    LPC_GPIO1->FIOCLR = LED_ALL_PINS;
}

void led_set(uint8_t led_num, uint8_t state) {
    if(led_num < 1 || led_num > 7) return;
    uint32_t pin = (1 << (18 + led_num));
    if(state) {
        LPC_GPIO1->FIOSET = pin;
    } else {
        LPC_GPIO1->FIOCLR = pin;
    }
}

void led_bargraph(uint8_t count) {
    led_all_off();
    for(uint8_t i = 1; i <= count && i <= 7; i++) {
        led_set(i, 1);
    }
}

void led_running_pattern(void) {
    for(uint8_t i = 1; i <= 7; i++) {
        led_all_off();
        led_set(i, 1);
        delay_ms(80);
    }
    led_all_off();
}

void led_blink_all(uint8_t times) {
    for(uint8_t i = 0; i < times; i++) {
        led_all_on();
        delay_ms(150);
        led_all_off();
        delay_ms(150);
    }
}

void led_show_occupancy(void) {
    uint8_t led_count = (system_state.total_people_inside * 7) / MAX_ROOM_CAPACITY;
    if(led_count > 7) led_count = 7;
    led_bargraph(led_count);
}

// ============================================
// BUZZER
// ============================================
void buzzer_init(void) {
    LPC_GPIO1->FIODIR |= BUZZER_PIN;
    LPC_GPIO1->FIOCLR = BUZZER_PIN;
}

void buzzer_beep(uint16_t duration_ms) {
    LPC_GPIO1->FIOSET = BUZZER_PIN;
    delay_ms(duration_ms);
    LPC_GPIO1->FIOCLR = BUZZER_PIN;
}

void buzzer_card_detected(void) { buzzer_beep(100); }
void buzzer_gate_opening(void) { buzzer_beep(100); delay_ms(100); buzzer_beep(100); }
void buzzer_gate_closing(void) { buzzer_beep(100); }
void buzzer_error(void) { for(uint8_t i=0; i<3; i++) { buzzer_beep(500); delay_ms(200); } }
void buzzer_emergency(void) { buzzer_beep(800); }
void buzzer_success(void) { buzzer_beep(150); delay_ms(100); buzzer_beep(150); }

// ============================================
// SERVO
// ============================================
void servo_init(void) {
    LPC_GPIO0->FIODIR |= SERVO_PIN;
    LPC_GPIO0->FIOCLR = SERVO_PIN;
}

void servo_pulse(uint16_t pulse_us) {
    LPC_GPIO0->FIOSET = SERVO_PIN;
    delay_us(pulse_us);
    LPC_GPIO0->FIOCLR = SERVO_PIN;
    delay_ms(20 - (pulse_us / 1000));
}

void servo_open(void) {
    // uart_dual_send_string("[SERVO] Opening gate...\r\n");
    send_json_gate_event("OPENING");
    for(uint8_t i = 0; i < 50; i++) servo_pulse(1500);
    system_state.gate_open = 1;
}

void servo_close(void) {
    // uart_dual_send_string("[SERVO] Closing gate...\r\n");
    send_json_gate_event("CLOSING");
    for(uint8_t i = 0; i < 50; i++) servo_pulse(1000);
    system_state.gate_open = 0;
}

// ============================================
// EMERGENCY BUTTON
// ============================================
void emergency_button_init(void) {
    LPC_GPIO2->FIODIR &= ~EMERGENCY_BUTTON;
    LPC_PINCON->PINMODE4 |= (3 << 22);
}

uint8_t emergency_button_pressed(void) {
    return (LPC_GPIO2->FIOPIN & EMERGENCY_BUTTON) ? 1 : 0;
}

// ============================================
// SYSTEM DATA
// ============================================
void system_data_init(void) {
    for(uint8_t i = 0; i < MAX_CARDS; i++) {
        cards[i].scan_count = 0;
        cards[i].last_scan_time = 0;
        cards[i].is_inside = 0;
    }

    system_state.total_people_inside = 0;
    system_state.total_entries = 0;
    system_state.total_exits = 0;
    system_state.system_uptime = 0;
    system_state.gate_busy = 0;
    system_tick = 0;
}

int8_t card_find(const uint8_t *uid) {
    for(uint8_t i = 0; i < MAX_CARDS; i++) {
        if(cards[i].is_active) {
            if(memcmp(cards[i].uid, uid, 4) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void update_total_people(void) {
    system_state.total_people_inside = 0;
    for(uint8_t i = 0; i < MAX_CARDS; i++) {
        if(cards[i].is_inside) {
            system_state.total_people_inside++;
        }
    }
}

// ============================================
// SENSOR FUNCTIONS
// ============================================
void sensors_read(void) {
    static uint8_t dht_fail_count = 0;

    // Read DHT11
    if(read_dht11()) {
        system_state.temperature = temperature;
        system_state.humidity = humidity;

        sprintf(temp_str, "%.1f", temperature);
        sprintf(hum_str, "%.1f", humidity);

        dht_fail_count = 0;
    } else {
        dht_fail_count++;
        
        sprintf(uart_buf,
            "ENV,{\"type\":\"SENSOR_ERROR\","
            "\"sensor\":\"DHT11\","
            "\"fail_count\":%d}\r\n",
            dht_fail_count);
        uart_dual_send_string(uart_buf);

        if(dht_fail_count >= 5) {
            strcpy(temp_str, "ERR");
            strcpy(hum_str, "ERR");
        }
    }

    // Read MQ135
    system_state.air_quality = MQ135_Read();
    sprintf(air_str, "%d", system_state.air_quality);
}

// ============================================
// LCD DISPLAY
// ============================================
void lcd_display_card_info(Card_t *card) {
    char line1[17] = {0};
    char line2[17] = {0};

    lcd_clear();
    snprintf(line1, 17, "Card: %-10s", card->card_name);
    lcd_goto(0, 0);
    lcd_string(line1);

    snprintf(line2, 17, "%-16s", card->group_name);
    lcd_goto(1, 0);
    lcd_string(line2);
}

void lcd_display_scrolling(uint8_t state) {
    char line1[17] = {0};
    char line2[17] = {0};

    lcd_clear();

    switch(state % 3) {
        case 0:
            snprintf(line1, 17, "People: %d/%d ",
                system_state.total_people_inside, MAX_ROOM_CAPACITY);
            snprintf(line2, 17, "AirQ: %s    ", air_str);
            break;

        case 1:
            snprintf(line1, 17, "Temp: %sC      ", temp_str);
            snprintf(line2, 17, "Humi: %s%%      ", hum_str);
            break;

        case 2:
            snprintf(line1, 17, "AirQ: %s    ", air_str);
            snprintf(line2, 17, "%-16s", MQ135_GetStatusString());
            break;
    }

    lcd_goto(0, 0);
    lcd_string(line1);
    lcd_goto(1, 0);
    lcd_string(line2);
}

// ============================================
// ENTRY/EXIT LOGIC
// ============================================
uint8_t process_entry(int8_t card_idx) {
    Card_t *card = &cards[card_idx];

    if(system_state.total_people_inside >= MAX_ROOM_CAPACITY) {
        return 0;
    }

    if(card->is_inside) {
        // Commented out: uart_dual_send_string("Already inside!\r\n");
        return 0;
    }

    card->is_inside = 1;
    card->scan_count++;
    card->last_scan_time = system_tick;

    system_state.total_entries++;
    update_total_people();
    led_show_occupancy();

    return 1;
}

void process_exit(int8_t card_idx) {
    Card_t *card = &cards[card_idx];

    if(!card->is_inside) {
        // Commented out: uart_dual_send_string("Not inside!\r\n");
        return;
    }

    card->is_inside = 0;
    card->scan_count++;
    card->last_scan_time = system_tick;

    system_state.total_exits++;
    update_total_people();
    led_show_occupancy();
}

void gate_operate(void) {
    system_state.gate_busy = 1;

    // Commented out: uart_dual_send_string("[GATE] Starting operation...\r\n");
    send_json_gate_event("OPERATION_START");

    buzzer_gate_opening();
    led_running_pattern();
    servo_open();

    // Commented out: uart_dual_send_string("[GATE] Gate open - waiting...\r\n");
    send_json_gate_event("OPEN_WAITING");
    delay_ms(GATE_OPEN_TIME * 1000);

    servo_close();
    buzzer_gate_closing();

    // Commented out: uart_dual_send_string("[GATE] Operation complete\r\n");
    send_json_gate_event("OPERATION_COMPLETE");

    system_state.gate_busy = 0;
}

void print_statistics(void) {
    // Send JSON status instead of formatted text
    send_json_system_status();
    
    /* COMMENTED OUT - OLD FORMAT
    uart_dual_send_string("\r\n========== STATUS ==========\r\n");
    sprintf(uart_buf, "Inside: %d/%d people\r\n",
        system_state.total_people_inside, MAX_ROOM_CAPACITY);
    uart_dual_send_string(uart_buf);
    sprintf(uart_buf, "Number of Entries: %d | Number of Exits: %d\r\n",
        system_state.total_entries, system_state.total_exits);
    uart_dual_send_string(uart_buf);
    sprintf(uart_buf, "Temp: %sC | Hum: %s%%\r\n", temp_str, hum_str);
    uart_dual_send_string(uart_buf);
    sprintf(uart_buf, "Air: %s (%d)\r\n",
        MQ135_GetStatusString(), system_state.air_quality);
    uart_dual_send_string(uart_buf);
    uart_dual_send_string("============================\r\n\r\n");
    */
}

// ============================================
// SYSTEM INITIALIZATION
// ============================================
void system_init(void) {
    UART0_Init();
    delay_ms(100);

    init_uart3();
    delay_ms(100);

    // System ready message in JSON
    uart_dual_send_string("INIT,{\"type\":\"SYSTEM_START\",\"version\":\"3.9\",\"capacity\":9}\r\n");
    
    /* COMMENTED OUT - OLD FORMAT
    uart_dual_send_string("\r\n========================================\r\n");
    uart_dual_send_string(" RFID SYSTEM - BATCH-3\r\n");
    uart_dual_send_string(" 10 Cards (1 Person Each)\r\n");
    uart_dual_send_string(" MAX CAPACITY: 9 PEOPLE\r\n");
    uart_dual_send_string(" DHT11 PIN: P0.7\r\n");
    uart_dual_send_string(" UART0: P0.2/P0.3 | UART3: P0.0/P0.1\r\n");
    uart_dual_send_string("========================================\r\n");
    */
    
    send_json_system_init("UART");

    // uart_dual_send_string("[2/9] LEDs...\r\n");
    led_init();
    led_blink_all(1);
    send_json_system_init("LED");

    // uart_dual_send_string("[3/9] Buzzer...\r\n");
    buzzer_init();
    buzzer_beep(200);
    send_json_system_init("BUZZER");

    // uart_dual_send_string("[4/9] Emergency...\r\n");
    emergency_button_init();
    send_json_system_init("EMERGENCY");

    // uart_dual_send_string("[5/9] LCD...\r\n");
    lcd_init();
    lcd_clear();
    lcd_display_centered(0, "Initializing...");
    send_json_system_init("LCD");

    // uart_dual_send_string("[6/9] RC522...\r\n");
    SSP0_init();
    delay_ms(100);

    LPC_GPIO0->FIODIR |= (1<<1);
    LPC_GPIO0->FIOCLR = (1<<1);
    delay_ms(100);
    LPC_GPIO0->FIOSET = (1<<1);
    delay_ms(100);

    RC522_Init();
    delay_ms(100);

    uint8_t version = SSP0_Read(RC522_REG_VERSION);
    
    sprintf(uart_buf, "INIT,{\"type\":\"RC522_VERSION\",\"version\":\"0x%02X\"}\r\n", version);
    uart_dual_send_string(uart_buf);
    
    // Commented out: sprintf(uart_buf, " Ver=0x%02X\r\n", version);

    if(version == 0x00 || version == 0xFF) {
        uart_dual_send_string("INIT,{\"type\":\"RC522_ERROR\",\"status\":\"FAILED\"}\r\n");
        lcd_clear();
        lcd_display_centered(0, "RC522 ERROR!");
        buzzer_error();
        while(1);
    }
    send_json_system_init("RC522");

    // uart_dual_send_string("[7/9] DHT11 (P0.7)...\r\n");
    DHT11_Init();
    
    lcd_clear();
    lcd_display_centered(0, "Testing DHT11...");
    lcd_display_centered(1, "Pin: P0.7");
    delay_ms(2000);

    uint8_t dht_success = 0;
    for(uint8_t attempt = 1; attempt <= 3; attempt++) {
        lcd_clear();
        sprintf(uart_buf, "DHT11 Test %d/3", attempt);
        lcd_display_centered(0, uart_buf);
        lcd_display_centered(1, "Pin: P0.7");

        if(read_dht11()) {
            sprintf(uart_buf,
                "INIT,{\"type\":\"DHT11_TEST\",\"attempt\":%d,\"temp\":%.1f,\"hum\":%.1f,\"status\":\"OK\"}\r\n",
                attempt, temperature, humidity);
            uart_dual_send_string(uart_buf);
            
            // Commented out: sprintf(uart_buf, "T=%.1fC H=%.1f%% OK\r\n", temperature, humidity);

            lcd_clear();
            sprintf(uart_buf, "T:%.1fC H:%.1f%%", temperature, humidity);
            lcd_display_centered(0, uart_buf);
            lcd_display_centered(1, "SUCCESS!");

            sprintf(temp_str, "%.1f", temperature);
            sprintf(hum_str, "%.1f", humidity);

            dht_success = 1;
            delay_ms(1500);
            break;
        } else {
            sprintf(uart_buf,
                "INIT,{\"type\":\"DHT11_TEST\",\"attempt\":%d,\"status\":\"FAIL\"}\r\n",
                attempt);
            uart_dual_send_string(uart_buf);
            
            lcd_clear();
            lcd_display_centered(0, "DHT11 FAILED!");
            lcd_display_centered(1, "Check P0.7");
            delay_ms(1000);
        }

        if(attempt < 3) {
            delay_ms(2500);
        }
    }

    if(!dht_success) {
        uart_dual_send_string("INIT,{\"type\":\"DHT11_WARNING\",\"status\":\"CHECK_P0.7\"}\r\n");
        strcpy(temp_str, "---");
        strcpy(hum_str, "---");
    }

    delay_ms(1000);

    // uart_dual_send_string("[8/9] MQ135...\r\n");
    MQ135_Init();
    send_json_system_init("MQ135");

    // uart_dual_send_string("[9/9] Servo...\r\n");
    servo_init();

    lcd_clear();
    lcd_display_centered(0, "Testing Servo...");

    servo_open();
    delay_ms(1000);
    servo_close();
    delay_ms(1000);
    send_json_system_init("SERVO");

    system_data_init();

    // Send card database in JSON format
    uart_dual_send_string("INIT,{\"type\":\"CARD_DATABASE\",\"total_cards\":10}\r\n");
    
    for(uint8_t i = 0; i < MAX_CARDS; i++) {
        sprintf(uart_buf,
            "CARD,{\"id\":\"%s\",\"group\":\"%s\","
            "\"uid\":\"%02X:%02X:%02X:%02X\"}\r\n",
            cards[i].card_name, cards[i].group_name,
            cards[i].uid[0], cards[i].uid[1], cards[i].uid[2], cards[i].uid[3]);
        uart_dual_send_string(uart_buf);
    }

    /* COMMENTED OUT - OLD CARD LIST FORMAT
    uart_dual_send_string("===== 10 CARDS (1 PERSON EACH) =====\r\n");
    uart_dual_send_string("\nFOUR MEM GRP (4 cards - 1 person each):\r\n");
    ...
    */

    uart_dual_send_string("INIT,{\"type\":\"SYSTEM_READY\",\"status\":\"ONLINE\"}\r\n");
    
    /* COMMENTED OUT - OLD FORMAT
    uart_dual_send_string("========================================\r\n");
    uart_dual_send_string(" SYSTEM READY - BATCH-3\r\n");
    uart_dual_send_string(" Max: 9 People | DHT11: P0.7\r\n");
    uart_dual_send_string("========================================\r\n\r\n");
    */

    lcd_clear();
    lcd_display_centered(0, "BATCH-3");
    lcd_display_centered(1, "READY!");

    buzzer_success();
    led_blink_all(2);
    delay_ms(2000);
}

// ============================================
// MAIN FUNCTION
// ============================================
int main(void) {
    uint8_t tagType[2];
    uint8_t uid_scanned[5];
    uint16_t sensor_timer = 0;
    uint8_t scroll_timer = 0;
    uint8_t scroll_state = 0;
    uint8_t uptime_timer = 0;
    int8_t card_idx;
    uint8_t emergency_prev = 0;
    char line2[17];

    system_init();

    while(1) {
        system_tick++;

        if(uptime_timer >= 100) {
            uptime_timer = 0;
            system_state.system_uptime += 10;
        }

        // EMERGENCY
        uint8_t emergency_current = emergency_button_pressed();
        if(emergency_current && !emergency_prev) {
            // Commented out: uart_dual_send_string("\r\n!!! EMERGENCY !!!\r\n");
            send_json_emergency();

            lcd_clear();
            lcd_display_centered(0, "EMERGENCY!");
            lcd_display_centered(1, "Opening Gate...");

            buzzer_emergency();
            led_all_on();

            system_state.gate_busy = 1;
            servo_open();
            delay_ms(5000);
            servo_close();
            system_state.gate_busy = 0;

            led_show_occupancy();
            buzzer_card_detected();
            
            uart_dual_send_string("ALERT,{\"type\":\"EMERGENCY_CLEARED\"}\r\n");
            // Commented out: uart_dual_send_string("Emergency closed\r\n\r\n");
            
            scroll_timer = 0;
        }
        emergency_prev = emergency_current;

        // SENSORS - Read every 60 seconds and send data continuously
        if(sensor_timer >= SENSOR_READ_INTERVAL) {
            sensor_timer = 0;
            sensors_read();
            send_json_sensor_data();  // Send immediately after reading
        }

        // LCD - Update every 3 seconds
        if(scroll_timer >= LCD_UPDATE_INTERVAL) {
            scroll_timer = 0;
            lcd_display_scrolling(scroll_state);
            scroll_state = (scroll_state + 1) % 3;  // Now cycles through 3 screens
        }

        // RFID
        if(!system_state.gate_busy) {
            if(RC522_Request(PICC_CMD_REQA, tagType) == MI_OK) {
                if(RC522_Anticoll(uid_scanned) == MI_OK) {
                    buzzer_card_detected();
                    led_blink_all(1);

                    lcd_clear();
                    lcd_display_centered(0, "Card Detected!");
                    lcd_display_centered(1, "Checking...");
                    delay_ms(1000);

                    /* COMMENTED OUT - OLD UID PRINT
                    uart_dual_send_string("\r\n===== CARD SCAN =====\r\n");
                    uart_dual_send_string("UID: ");
                    for(uint8_t i = 0; i < 4; i++) {
                        uart_dual_send_hex(uid_scanned[i]);
                        if(i < 3) uart_dual_send_char(':');
                    }
                    uart_dual_send_string("\r\n");
                    */

                    card_idx = card_find(uid_scanned);

                    if(card_idx == -1) {
                        // Unknown card - send JSON
                        send_json_unknown_card(uid_scanned);
                        
                        /* COMMENTED OUT
                        uart_dual_send_string("UNKNOWN - DENIED\r\n");
                        uart_dual_send_string("=====================\r\n\r\n");
                        */

                        lcd_clear();
                        lcd_display_centered(0, "Access Denied!");
                        lcd_display_centered(1, "Unknown Card");

                        buzzer_error();
                        led_blink_all(5);
                        delay_ms(3000);
                        scroll_timer = 0;
                        led_show_occupancy();
                        continue;
                    }

                    Card_t *card = &cards[card_idx];

                    /* COMMENTED OUT
                    sprintf(uart_buf, "Card: %s\r\n", card->card_name);
                    uart_dual_send_string(uart_buf);
                    sprintf(uart_buf, "Group: %s (1 person)\r\n", card->group_name);
                    uart_dual_send_string(uart_buf);
                    */

                    lcd_display_card_info(card);
                    delay_ms(2000);

                    if(!card->is_inside) {
                        if(process_entry(card_idx)) {
                            // Entry granted - send JSON
                            send_json_rfid_scan(card, "ENTRY", 1);
                            
                            /* COMMENTED OUT
                            uart_dual_send_string("ENTRY GRANTED\r\n");
                            uart_dual_send_string("=====================\r\n");
                            */

                            lcd_clear();
                            lcd_display_centered(0, "WELCOME!");
                            snprintf(line2, 17, "Inside: %d/%d",
                                system_state.total_people_inside, MAX_ROOM_CAPACITY);
                            lcd_goto(1, 0);
                            lcd_string(line2);
                            delay_ms(2000);

                            print_statistics();
                            gate_operate();

                        } else {
                            // Entry denied - room full
                            send_json_rfid_scan(card, "ENTRY_DENIED_FULL", 0);
                            
                            /* COMMENTED OUT
                            uart_dual_send_string("DENIED - FULL\r\n");
                            uart_dual_send_string("=====================\r\n\r\n");
                            */

                            lcd_clear();
                            lcd_display_centered(0, "ROOM FULL!");
                            lcd_display_centered(1, "NO ENTRY");

                            buzzer_error();
                            led_blink_all(5);
                            delay_ms(3000);
                            scroll_timer = 0;
                            led_show_occupancy();
                            continue;
                        }

                    } else {
                        process_exit(card_idx);
                        
                        // Exit recorded - send JSON
                        send_json_rfid_scan(card, "EXIT", 1);
                        
                        /* COMMENTED OUT
                        uart_dual_send_string("EXIT RECORDED\r\n");
                        uart_dual_send_string("=====================\r\n");
                        */

                        lcd_clear();
                        lcd_display_centered(0, "THANK YOU!");
                        snprintf(line2, 17, "Inside: %d/%d",
                            system_state.total_people_inside, MAX_ROOM_CAPACITY);
                        lcd_goto(1, 0);
                        lcd_string(line2);
                        delay_ms(2000);

                        print_statistics();
                        gate_operate();
                    }

                    sensor_timer = 0;
                    scroll_timer = 0;
                    delay_ms(1000);
                }
            }
        }

        sensor_timer++;
        scroll_timer++;
        uptime_timer++;
        delay_ms(100);
    }

    return 0;
}