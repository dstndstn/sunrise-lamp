#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

#include <lcd.h>

// AC_DETECT uses an interrupt, so MUST be pin 2 or 3
#define AC_DETECT  2
#define COUNT      4
#define AC_CONTROL 7
//#define PWM_LED    3

//#define BUTTONS    A0
#define BUTTON_A    A0
#define BUTTON_B    A1
#define BUTTON_C    A2
#define BUTTON_D    A3

// neopixel
#define PIN 1

int last_ac = 0;
int n_ac = 0;
int blink = 0;

volatile int ac_on = 0;
volatile uint16_t ac_delay = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, PIN, NEO_GRB + NEO_KHZ800);
uint32_t c;

void setup() {
  pinMode(AC_DETECT, INPUT);
  pinMode(COUNT, OUTPUT);
  pinMode(AC_CONTROL, OUTPUT);
  //pinMode(PWM_LED, OUTPUT);
  digitalWrite(COUNT, 1);
  digitalWrite(AC_CONTROL, 0);

  //pinMode(BUTTONS, INPUT);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
  pinMode(BUTTON_D, INPUT_PULLUP);
  
  last_ac = 0;
  n_ac = 0;
  blink = 0;

  lcd_port_init();
  delay(20);
  lcd_full_reset();

  lcd_write_instr(LCD_CMD_FUNCTION |
          LCD_FUNCTION_4BITS | LCD_FUNCTION_2LINES | LCD_FUNCTION_FONT_5X8);
  lcd_write_instr(LCD_CMD_DISPLAY |
          LCD_DISPLAY_ON | LCD_CURSOR_ON | LCD_CURSOR_BLINK);
  lcd_write_instr(LCD_CMD_ENTRY | LCD_ENTRY_INC);
  lcd_write_instr(LCD_CMD_HOME);
  /*
  lcd_write_data('H');
  lcd_write_data('e');
  lcd_write_data('l');
  lcd_write_data('l');
  lcd_write_data('o');

  delay(500);
  */
  lcd_set_stdout();
  lcd_write_instr(LCD_CMD_DISPLAY | LCD_DISPLAY_ON);
  lcd_write_instr(LCD_CMD_CLEAR);

  lcd_home();
  lcd_clear();
  printf("Hello world!");
  //printf("Hello     World\n%i  %s", 42, "penguins");
  //lcd_test();
  /*
  lcd_write_instr(LCD_CMD_HOME);
  lcd_move_to(0);
  lcd_write_data('H');
  lcd_write_data('e');
  lcd_write_data('l');
  lcd_write_data('l');
  lcd_write_data('o');
  */
    
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  c = strip.Color(255, 0, 0);
  strip.setPixelColor(0, c);
  strip.show();

  delay(100);
  c = strip.Color(255, 255, 0);
  strip.setPixelColor(0, c);
  strip.show();

  delay(100);
  c = strip.Color(255, 50, 0);
  strip.setPixelColor(0, c);
  strip.show();

  // timer1: call interrupt after a delay
  noInterrupts();
  TCCR1A = 0;
  //TCCR1B = 0;
  // TCNT1 = x;   // 65536 - 16MHz/prescale/delay
  //TCCR1B = (1 << CS12); // 256 prescaler
  TCNT1 = 65535;
  TCCR1B = (1 << CS11); // 8 prescaler
  TIMSK1 |= (1 << TOIE1); // enable timer overflow interrupt
  interrupts();
  
  attachInterrupt(digitalPinToInterrupt(AC_DETECT), ac_isr, FALLING);
}

static void ac_isr() {
  n_ac++;
  //
  if (ac_on) {
    TCNT1 = ac_delay;
  }
}

// Interrupt routine for Timer1 overflow
ISR(TIMER1_OVF_vect) {
  if (ac_on) {
    digitalWrite(AC_CONTROL, 1);
    delayMicroseconds(5);
    digitalWrite(AC_CONTROL, 0);
  }
}

// About AVR timers
// timer0, timer2 are 8-bit.  timer1 is 16-bit
// 16 MHz clock
// Arduino: timer0 used for delay(), millis(), micros()
//          timer1 used for Servo library
//          timer2 used for tone()
// - timers set for 1kHz freq




int seconds = 0;

void set_ac(int perthou) {
  // 0 to 1000
  if (perthou < 0)
    perthou = 0;
  if (perthou > 1000)
    perthou = 1000;
  uint16_t t = (((uint32_t)15800 * (uint32_t)(1000-perthou)) / (uint32_t)1000);
  ac_delay = 65535 - t;
}

int ac_frac = 0;

int pwm = 0;

unsigned long ba_time = 0;
unsigned long bb_time = 0;
unsigned long bc_time = 0;
unsigned long bd_time = 0;

void loop() {

  // 16M / 8 / 120 = 16667 timer1 counts per 120-Hz cycle

  // 16000 goes on full blast (has wrapped around to next cycle)
  // 15900 full on
  // 15800 is very low
  // 15500 is very low
  // 15000 is very low
  
  //ac_delay = 65535 - 15800;
  if (!ac_on && seconds >= 3) {
    ac_on = 1;

    set_ac(140);
    delay(1000);
    set_ac(100);
    ac_frac = 100;
  }

  //ac_frac = int(1000. * abs(sin(seconds * 0.01)));
  //set_ac(ac_frac);

  //int buttons = analogRead(BUTTONS);
  int ba = (analogRead(BUTTON_A) < 512);
  int bb = (analogRead(BUTTON_B) < 512);
  int bc = (analogRead(BUTTON_C) < 512);
  int bd = (analogRead(BUTTON_D) < 512);

  unsigned long now = millis();

  int updated = 0;
  
  if (bc && now - bc_time > 50) {
    // C press/hold
    /*
    pwm++;
    if (pwm > 255)
      pwm = 255;
    analogWrite(PWM_LED, pwm);
    */
    
    ac_frac++;
    if (ac_frac > 1000)
      ac_frac = 1000;
    set_ac(ac_frac);

    bc_time = now;
    updated = 1;
  }
  if (ba && now - ba_time > 50) {
    // A press/hold
    /*
    pwm--;
    if (pwm < 0)
      pwm = 0;
    analogWrite(PWM_LED, pwm);
    */
    ac_frac--;
    if (ac_frac < 0)
      ac_frac = 0;
    set_ac(ac_frac);

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

  if (n_ac >= 120) {
    n_ac = 0;
    seconds++;
    digitalWrite(COUNT, seconds%2);
    updated = 1;
  }

  if (updated) {
    int ss = seconds % 60;
    int mm = seconds / 60;
    int hh = (mm / 60) % 12;
    mm = mm % 60;
    if (hh == 0) {
      hh = 12;
    }
    lcd_home();
    //lcd_clear();
    printf("%02i:%02i:%02i            ", hh, mm, ss);
    //printf("\n%i    %i", ac_frac, buttons);
    printf("\n%i %i   %i %i %i %i", ac_frac, pwm, ba, bb, bc, bd);
  }

}







// 8400 too much (shoud be 8333)
//const int period = 8300;

const int prewait = 700;
const int period = 8200 - prewait;

void OLD_loop() {
  int ac = digitalRead(AC_DETECT);
  if (ac == last_ac)
    return;
  last_ac = ac;

  n_ac++;
  if (n_ac >= 60) {
    n_ac = 0;
    blink++;
    digitalWrite(COUNT, blink%2);
    //digitalWrite(AC_CONTROL, blink%2);

    if (blink % 2 == 0) {
      seconds++;
      int ss = seconds % 60;
      int mm = seconds / 60;
      int hh = (mm / 60) % 12;
      mm = mm % 60;
      if (hh == 0) {
        hh = 12;
      }
      lcd_home();
      lcd_clear();
      printf("%02i:%02i:%02i", hh, mm, ss);
    }

  }

  //if ((ac) && (blink % 2 == 1)) {
if (ac) {

    int rgbperiod = 50;
    if (0 &&  blink < rgbperiod) {
      int r = ((255 * (blink%rgbperiod)) / rgbperiod);
      int g = ((100 * (blink%rgbperiod)) / rgbperiod);
      int b = (( 10 * (blink%rgbperiod)) / rgbperiod);
      c = strip.Color(r, g, b);
      strip.setPixelColor(0, c);
      strip.show();
    } else {
    
    delayMicroseconds(prewait);
    int ontime = ((blink-rgbperiod) % (period/25)) * 25;
    //int ontime = ((blink % 5) + (period/50 - 5)) * 50;
    //int ontime = ((blink % 5) + (period/50 - 5)) * 50;
    //int ontime = (((blink/5) % 10) + 0) * 50;
    //int ontime = (((blink/5) % (period/50)) + 0) * 50;
    int waittime = period - ontime;
    //delayMicroseconds(period - (blink%(period/50))*50);
    delayMicroseconds(waittime);
    digitalWrite(AC_CONTROL, 1);
    //delayMicroseconds(10);
    delayMicroseconds(ontime);
    digitalWrite(AC_CONTROL, 0);

    }
  }
}


