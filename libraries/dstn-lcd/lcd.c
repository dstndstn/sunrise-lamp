#include <avr/io.h>
#include <compat/ina90.h>
#include <util/delay.h>
#include <stdio.h>
#include <Arduino.h>

#include "lcd.h"
#include "lcd-config.h"

#define _delay_ms delay

static void delay_e() {
    _delay_us(1);
    //delayMicroseconds(10);
}

static void set_init_dir();

void lcd_port_init() {
    set_init_dir();
}

void lcd_move_to(uint8_t posn) {
    lcd_write_instr(0x80 | (posn & 0x7f));
}

void lcd_home() {
    lcd_write_instr(LCD_CMD_HOME);
}
void lcd_clear() {
    lcd_write_instr(LCD_CMD_CLEAR);
}

static int lcd_putchar(char c, FILE *stream) {
    if (c == '\n')
        lcd_write_instr(LCD_CMD_SET_DDRAM | 0x40);
    else
        lcd_write_data(c);
    return 0;
}

static FILE lcdstdout = FDEV_SETUP_STREAM(lcd_putchar, NULL, _FDEV_SETUP_WRITE);

void lcd_set_stdout() {
    stdout = &lcdstdout;
}

#define nop _NOP

/*
#define clear_rs_bit()  LCD_PORT_RS &= ~(_BV(LCD_BIT_RS))
#define set_rs_bit()    LCD_PORT_RS |=   _BV(LCD_BIT_RS)
#define clear_e_bit()   LCD_PORT_E  &= ~(_BV(LCD_BIT_E))
#define set_e_bit()     LCD_PORT_E  |=   _BV(LCD_BIT_E)
#define set_read_bit()  LCD_PORT_RW |=   _BV(LCD_BIT_RW)
#define set_write_bit() LCD_PORT_RW &= ~(_BV(LCD_BIT_RW))
//#define get_db7654_bits()  ((LCD_PORT_DATA &  LCD_BITMASK_D7654)     >> LCD_BITSHIFT_D7654)
//#define set_db7654_bits(x)   LCD_PORT_DATA = (LCD_BITMASK_D7654 & (x << LCD_BITSHIFT_D7654))
#define get_db7654_bits()   (LCD_PIN_DATA &  LCD_BITMASK_D7654)
#define set_db7654_bits(x)   LCD_PORT_DATA = (LCD_PORT_DATA & ~LCD_BITMASK_D7654) | (LCD_BITMASK_D7654 & (x))
#define set_data_input()   LCD_DIR_DATA  &= ~(LCD_BITMASK_D7654)
#define set_data_output()  LCD_DIR_DATA  |=   LCD_BITMASK_D7654
 */

static void clear_rs_bit() {
    digitalWrite(LCD_RS, 0);
}
static void set_rs_bit() {
    digitalWrite(LCD_RS, 1);
}
static void clear_e_bit() {
    digitalWrite(LCD_E, 0);
}
static void set_e_bit() {
    digitalWrite(LCD_E, 1);
}
static void set_read_bit() {
    digitalWrite(LCD_RW, 1);
}
static void set_data_input() {
    pinMode(LCD_D4, INPUT);
    pinMode(LCD_D5, INPUT);
    pinMode(LCD_D6, INPUT);
    pinMode(LCD_D7, INPUT);
}
static void set_write_bit() {
    digitalWrite(LCD_RW, 0);
}
static void set_data_output() {
    pinMode(LCD_D4, OUTPUT);
    pinMode(LCD_D5, OUTPUT);
    pinMode(LCD_D6, OUTPUT);
    pinMode(LCD_D7, OUTPUT);
}
static uint8_t get_db7654_bits() {
    return ((digitalRead(LCD_D4) ? 0x1 : 0) |
            (digitalRead(LCD_D5) ? 0x2 : 0) |
            (digitalRead(LCD_D6) ? 0x4 : 0) |
            (digitalRead(LCD_D7) ? 0x8 : 0));
}
static void set_db7654_bits(uint8_t val) {
    digitalWrite(LCD_D4, val & 0x1);
    digitalWrite(LCD_D5, val & 0x2);
    digitalWrite(LCD_D6, val & 0x4);
    digitalWrite(LCD_D7, val & 0x8);
}

// D7
static uint8_t get_busy_bit() {
    return digitalRead(LCD_D7);
}

static void set_init_dir() {
    set_data_input();
    pinMode(LCD_RS, OUTPUT);
    pinMode(LCD_RW, OUTPUT);
    pinMode(LCD_E,  OUTPUT);
}

void lcd_test(void) {
    //set_e_bit();
    //set_rs_bit();
    //set_read_bit();
    set_write_bit();
    set_data_output();
    set_db7654_bits(0x8);
    
    _delay_ms(1000);
    //clear_e_bit();
    //clear_rs_bit();
    //set_write_bit();
    set_db7654_bits(0x0);

    _delay_ms(1000);
    //set_e_bit();
    //set_rs_bit();
    //set_read_bit();
    set_db7654_bits(0x8);
}

static void wait_for_not_busy() {
    uchar busy; //, y;
    uint maxdelay = 2000;
    clear_rs_bit();
    set_read_bit();
    clear_e_bit();
    set_data_input();
    delayMicroseconds(1);

    do {
        set_e_bit();
        delay_e();
        // Read top 4 bits
        busy = get_busy_bit();
        clear_e_bit();

        // Read bottom 4 bits
        delay_e();
        set_e_bit();
        delay_e();
        //y = get_db7654_bits();
        clear_e_bit();
        delay_e();

        maxdelay--;
        if (!maxdelay)
            break;
    } while (busy);

    // read address counter.
    /*
     _delay_us(4);
     set_e_bit();
     delay_e();
     x = get_db7654_bits();
     clear_e_bit();
     delay_e();
     set_e_bit();
     delay_e();
     y = get_db7654_bits();
     clear_e_bit();
     delay_e();
     */
}

static void lcd_write_4bit_instr() {
    clear_rs_bit();
    set_write_bit();
    clear_e_bit();
    set_data_output();
    set_db7654_bits((LCD_CMD_FUNCTION | LCD_FUNCTION_4BITS) >> 4);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();
    delay_e();
}

void lcd_write_instr(uchar cmd) {
    wait_for_not_busy();
    
    clear_rs_bit();
    set_write_bit();
    clear_e_bit();
    set_data_output();

    set_db7654_bits(cmd >> 4);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();
    delay_e();

    set_db7654_bits(cmd & 0xf);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();
    delay_e();
}

void lcd_write_data(uchar data) {
    wait_for_not_busy();

    set_rs_bit();
    set_write_bit();
    clear_e_bit();
    set_data_output();

    set_db7654_bits(data >> 4);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();
    delay_e();

    set_db7654_bits(data & 0xf);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();
    delay_e();
}

void lcd_full_reset() {
    clear_rs_bit();
    set_write_bit();
    clear_e_bit();
    set_data_output();

    _delay_ms(15);
    
    set_db7654_bits(0x3);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();

    _delay_ms(5);

    set_db7654_bits(0x3);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();

    _delay_ms(1);

    set_db7654_bits(0x3);
    delayMicroseconds(1);
    set_e_bit();
    delay_e();
    clear_e_bit();

    _delay_ms(1);

    lcd_write_4bit_instr();
}

