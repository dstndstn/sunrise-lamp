#include <avr/power.h>

#include <Adafruit_NeoPixel.h>
#include <lcd.h>
//#include <pinknoise.h>

//// AC_DETECT uses an interrupt, so MUST be pin 2 or 3
#define AC_DETECT  3
#define AC_CONTROL 8
#define NEO_PIN    9

#define PWM_PIN    11

#define BUTTON_A    A0
#define BUTTON_B    A1
#define BUTTON_C    A2
#define BUTTON_D    A3
#define BUTTON_E    A4
#define BUTTON_F    A5


static void ac_isr();
static void reset_wakeup();
static void set_ac(int perthou);
void setup();

#if STANDALONE
// From https://github.com/arduino/Arduino/blob/2bfe164b9a5835e8cb6e194b928538a9093be333/hardware/arduino/avr/cores/arduino/main.cpp
// Weak empty variant initialization function.
// May be redefined by variant files.
void initVariant() __attribute__((weak));
void initVariant() { }

int main() {
    init();
    initVariant();
    setup();
    for (;;)
        loop();
}
#endif


int n_ac = 0;

volatile int ac_on = 0;
volatile uint16_t ac_delay = 0;

Adafruit_NeoPixel neopix = Adafruit_NeoPixel(1, NEO_PIN, NEO_GRB + NEO_KHZ800);
uint32_t c;

int demo = 0;
int32_t demo_saved_wakeup;

int ac_frac = 0;

void setup() {

    pinMode(AC_DETECT, INPUT_PULLUP);
    pinMode(AC_CONTROL, OUTPUT);
    digitalWrite(AC_CONTROL, 0);

    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    pinMode(BUTTON_D, INPUT_PULLUP);
    pinMode(BUTTON_E, INPUT_PULLUP);
    pinMode(BUTTON_F, INPUT_PULLUP);
  
    n_ac = 0;

    lcd_port_init();
    delay(20);
    lcd_full_reset();

    lcd_write_instr(LCD_CMD_FUNCTION |
                    LCD_FUNCTION_4BITS | LCD_FUNCTION_2LINES | LCD_FUNCTION_FONT_5X8);
    lcd_write_instr(LCD_CMD_DISPLAY |
                    LCD_DISPLAY_ON | LCD_CURSOR_ON | LCD_CURSOR_BLINK);
    lcd_write_instr(LCD_CMD_ENTRY | LCD_ENTRY_INC);
  
    lcd_set_stdout();
    lcd_write_instr(LCD_CMD_DISPLAY | LCD_DISPLAY_ON);
    lcd_write_instr(LCD_CMD_CLEAR);

    lcd_home();
    lcd_clear();
    printf("Wakey wakey!");

    neopix.begin();
    neopix.show();
    int i;
    for (i=0; i<256; i++) {
        c = neopix.Color(i, 0, 0);
        neopix.setPixelColor(0, c);
        neopix.show();
        delay(2);
    }
    for (i=0; i<256; i++) {
        c = neopix.Color(255, i, 0);
        neopix.setPixelColor(0, c);
        neopix.show();
        delay(2);
    }
    for (i=0; i<256; i++) {
        c = neopix.Color(255, 255, i);
        neopix.setPixelColor(0, c);
        neopix.show();
        delay(2);
    }
    for (i=255; i>=0; i--) {
        c = neopix.Color(i, i, i);
        neopix.setPixelColor(0, c);
        neopix.show();
        delay(1);
    }

    // timer1: call interrupt after a delay
    noInterrupts();
    TCCR1A = 0;
    // TCNT1 = x;   // 65536 - 16MHz/prescale/delay
    TCNT1 = 65535;
    TCCR1B = (1 << CS11);   // factor 8 prescaler
    TIMSK1 |= (1 << TOIE1); // enable timer overflow interrupt
    interrupts();
  
    // The AC_DETECT pin goes HIGH as the 120 Hz AC signal crosses zero,
    // so rises at the tail end of one cycle, stays high during the zero
    // crossing, then falls at the beginning of the next cycle.
    // By listening for the FALLING edge, we're sure that if we trigger
    // the TRIAC, it will stay on for the whole cycle, at the expense of
    // not being able to hit 100% duty cycle.
    //  attachInterrupt(digitalPinToInterrupt(AC_DETECT), ac_isr, FALLING);
    attachInterrupt(digitalPinToInterrupt(AC_DETECT), ac_isr, CHANGE);

    reset_wakeup();

    set_ac(0);
    ac_on = 0;

    /*
     lcd_home();
     printf("Fire up bulb...");

     // Fire up the LED bulb... can't just ramp up from zero.
     set_ac(200);
     delay(100);
     set_ac(50);
     delay(100);

     delay(1000);
     lcd_home();
     printf("Dim up...");
     for (i=0; i<1000; i++) {
     set_ac(i);
     delay(2);
     }
     for (i=1000; i>=0; i--) {
     set_ac(i);
     delay(2);
     }
     delay(1000);
     */
  
    /* Good enough for start-up?
     set_ac(150);
     delay(250);
     set_ac(60);
     */
  
    /*
     delay(1000);
  
     for (i=60; i<300; i++) {
     set_ac(i);
     delay(20);
     }
     for (i=300; i>=60; i--) {
     set_ac(i);
     delay(20);
     }
     */
    /*
     set_ac(200);
     delay(50);
     set_ac(60);
     */
    ac_frac = 60;
  
    //ac_on = 1;
  
    // OC2A-> Arduino pin 11

    // OCR2A, OCR2B
    // WGM2
    // COM2

    // Fast PWM mode: WGM2 bits 0-2 = 0x3 (count up to 255) or 0x7 (count up to OCR2A)
    //     COM2x bits 0-1 = 0x2 (non-inverted PWM), 0x3 (inverted PWM)

    /* Pink noise
     OCR2A = 0x80;
     pinMode(PWM_PIN, OUTPUT);
     analogWrite(PWM_PIN, 0x80);
     setup_pinknoise();
     */

}

static void ac_isr() {
    //
    uint8_t ac = digitalRead(AC_DETECT);
    if (!ac) {
        // just went low -- just after the start of a half-cycle
        // increment AC rectified-wave count (120 Hz)
        n_ac++;
        // if AC is enabled, start timer1 for triggering TRIAC
        if (ac_on)
            TCNT1 = ac_delay;
    } else {
        // just went high -- end of a half-cycle.
        // Turn off TRIAC.
        if (ac_on)
            digitalWrite(AC_CONTROL, 0);
    }
}
/*
 ISR (TIMER2_OVF_vect) {
 timer_overflow_isr();
 }
 */

// Interrupt routine for Timer1 overflow
ISR(TIMER1_OVF_vect) {
    if (ac_on) {
        // Trigger TRIAC
        digitalWrite(AC_CONTROL, 1);
        //delayMicroseconds(5);
        // long enough to allow the 'sillyscope to catch it!
        //delayMicroseconds(100);
        //digitalWrite(AC_CONTROL, 0);
    }
}

// About AVR timers
// timer0, timer2 are 8-bit.  timer1 is 16-bit
// 16 MHz clock
// Arduino: timer0 used for delay(), millis(), micros()
//          timer1 used for Servo library --- I'm using it here for AC dimming
//          timer2 used for tone()
// - timers are set by the arduino startup code for ~1kHz freq (/64 prescaler)

// Apparently, delay() on timer0 still work if you use PWM, just don't mess with the
// prescaler or the frequency (TOP count)

// Output compare registers:
// Register   Arduino pin#     Pin  Chip pin
// OC0A            6           PD6     12
// OC0B            5           PD5     11
// OC1A            9           PB1     15
// OC1B           10           PB2     16
// OC2A           11           PB3     17
// OC2B            3           PD3      5




// current time
int32_t seconds = 8 * 3600L;

// wake time
//int32_t wakeup = (6+12) * 3600L + 30 * 60;
int32_t wakeup = 8 * 3600L + 610;

void set_ac(int perthou) {
    // perthou: brightness, range 0 to 1000
    if (perthou < 0)
        perthou = 0;
    if (perthou > 1000)
        perthou = 1000;

    // 8M / 120 = 66666
    // 8M / 120 / 8 = 8333
  
    // 16M / 8 / 120 = 16667 timer1 counts per 120-Hz cycle
    // 16000 goes on full blast (has wrapped around to next cycle)
    // 15900 full on
    // 15800 is very low
    // 15500 is very low
    // 15000 is very low

    uint16_t t;
#if (F_CPU == 16000000UL)
    t = ((15800UL * (uint32_t)(1000-perthou)) / 1000UL);
#elif (F_CPU == 8000000UL)
    //t = ((7900UL * (uint32_t)(1000-perthou)) / 1000UL);
    t = ((7500UL * (uint32_t)(1000-perthou)) / 1000UL);
#else
#warning Unknown clock freq
#endif

    ac_delay = 65535 - t;
}


//int started_lamp;

static void reset_wakeup() {
    //started_lamp = 0;
    ac_on = 0;
}

void update_wakeup() {

    int32_t neo_dur;
    int32_t ac_dur;

    if (demo) {
        neo_dur = 30;
        ac_dur = 60;
    } else {
        neo_dur = 120;
        ac_dur =  600;
    }

    int32_t duration = neo_dur + ac_dur;
    int32_t wakeup_start = wakeup - duration;

    if ((seconds < wakeup_start) && ac_on) {
        reset_wakeup();
    }

    if (seconds < wakeup_start || seconds > wakeup)
        return;

    int32_t elapsed = seconds - wakeup_start;

    //printf("\n%li el ", elapsed);

    int32_t neo_dur1 = neo_dur / 2;

    if (elapsed < neo_dur1) {
        int r = (255 * elapsed / neo_dur1);
        int g = ( 50 * elapsed / neo_dur1);
        int b = ( 10 * elapsed / neo_dur1);
        //printf("neo %i %i %i", r, g, b);
        c = neopix.Color(r, g, b);
        neopix.setPixelColor(0, c);
        neopix.show();
        return;
    }
    if (elapsed < neo_dur) {
        int r = 255;
        int g =  50 + ( 50 * (elapsed-neo_dur1) / (neo_dur-neo_dur1));
        int b =  10 + (  5 * (elapsed-neo_dur1) / (neo_dur-neo_dur1));
        c = neopix.Color(r, g, b);
        neopix.setPixelColor(0, c);
        neopix.show();
        return;
    }


    elapsed -= neo_dur;
    /*
     if (!started_lamp) {
     // Fire up the LED bulb... can't just ramp up from zero.
     ac_on = 1;
     set_ac(150);
     delay(250);
     set_ac(60);
     delay(100);
     started_lamp = 1;
     }
     */

    // slower ramp at start
    int32_t dur1 = ac_dur/2;
    int32_t frac;
    if (elapsed < dur1) {
        frac = 60 + 240 * elapsed / dur1;
    } else {
        frac = 60 + 240 + 700 * (elapsed-dur1) / (ac_dur - dur1);
    }

    ac_frac = frac;

    //printf("ac %li %li", elapsed, frac);
    set_ac(frac);
}


// button press start time / most recent
unsigned long ba_time = 0;
unsigned long bb_time = 0;
unsigned long bc_time = 0;
unsigned long bd_time = 0;
unsigned long be_time = 0;
unsigned long bf_time = 0;

void loop() {
  
    int ba = (analogRead(BUTTON_A) < 512);
    int bb = (analogRead(BUTTON_B) < 512);
    int bc = (analogRead(BUTTON_C) < 512);
    int bd = (analogRead(BUTTON_D) < 512);
    int be = (analogRead(BUTTON_E) < 512);
    int bf = (analogRead(BUTTON_F) < 512);

    unsigned long now = millis();

    int updated = 0;

    if (bf && now - bf_time > 50) {
        ac_frac--;
        if (ac_frac < 0)
            ac_frac = 0;
        set_ac(ac_frac);
        if (ac_frac == 0) {
            ac_on = 0;
        }
        bf_time = now;
        updated = 1;
        /*
         // DEMO mode!
         if (demo == 0) {
         demo = 1;
         demo_saved_wakeup = wakeup;
         wakeup = seconds + 10 + 90;
         updated = 1;
         }
         */
    }
    if (be && now - be_time > 50) {
        //if (be) {
        ac_frac++;
        if (ac_frac > 1000)
            ac_frac = 1000;

        if (!ac_on && ac_frac >= 100) {
            ac_on = 1;

            // Fire up the LED bulb... can't just ramp up from zero.
            //set_ac(150);
            set_ac(200);
            delay(250);
            //set_ac(60);
            set_ac(100);
            delay(100);
       
        }
        set_ac(ac_frac);
        be_time = now;
        updated = 1;
        /*
         // cancel DEMO mode!
         if (demo == 1) {
         demo = 0;
         wakeup = demo_saved_wakeup;
         updated = 1;
         }
         */
    }

    if (bc && now - bc_time > 50) {
        // C press/hold
        /*
         */

        wakeup -= 60;
        if (wakeup < 0)
            wakeup = 0;

        bc_time = now;
        updated = 1;
    }
    if (ba && now - ba_time > 50) {
        // A press/hold
        wakeup += 60;

        ba_time = now;
        updated = 1;
    }

    if (bb && now - bb_time > 50) {
        // B press/hold
        seconds += 60;
        bb_time = now;
        updated = 1;
    }
    if (bd && now - bd_time > 50) {
        // D press/hold
        seconds -= 60;
        if (seconds < 0)
            seconds = 0;
        bd_time = now;
        updated = 1;
    }

    // clock
    if (n_ac >= 120) {
        n_ac -= 120;
        seconds++;
        updated = 1;
    }

    if (!updated)
        return;

    int ss = seconds % 60;
    int mm = seconds / 60;
    int hh = (mm / 60);
    mm = mm % 60;
    hh = hh % 24;
    /// Not a typo... noon=0, midnight=12h, morning ~= 18h
    int ispm = (hh < 12);
    hh = hh % 12;
    if (hh == 0)
        hh = 12;

    lcd_home();
    printf("     %02i:%02i:%02i %s            ", hh, mm, ss, (ispm ? "PM":"AM"));
  
    ss = wakeup % 60;
    mm = wakeup / 60;
    hh = (mm / 60);
    mm = mm % 60;
    hh = hh % 24;
    ispm = (hh < 12);
    hh = hh % 12;
    if (hh == 0)
        hh = 12;
    printf("\nWake %02i:%02i:%02i %s   %i       ", hh, mm, ss, (ispm ? "PM":"AM"), ac_frac);
    //printf("\nFrac %i    ", ac_frac);

    // Roll over to the next day (at noon)!
    int32_t day = 24 * (int32_t)3600;
    if (seconds >= day) {
        seconds -= day;
        reset_wakeup();
    }

    update_wakeup();
}
