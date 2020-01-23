/*
  Arduino pink noise generator -- Mark Tillotson, 2018-12-09

  Works for Uno/Pro Mini/Mega, using pin 11 or 10 depending on board (the one driven by timer2 output A)

  Uses the highly efficient algorithm from Stenzel, see:
     http://stenzel.waldorfmusic.de/post/pink/
     https://github.com/Stenzel/newshadeofpink

  Note his request that if used in a commercial product, he is sent one of the units...

 */
#include <stdint.h>
#include <avr/io.h>

typedef uint8_t byte;

#include "firtables.inc"

/// HARD-CODED for PIN 11

void setup_pinknoise() {
  build_revtab();
  TCCR2A = 0xB3 ;  // configure as fast 8-bit PWM (mode 011, clock prescale = 1)
  TCCR2B = 0x01 ;
  TIMSK2 = 0x01;   // timer2 overflow interrupt enabled every 256 cycles (62.5 kHz for a 16MHz ATmega)
}

volatile byte outsampl = 0x80 ;  // cache last sample for ISR
volatile byte phase = 0 ;  // oscillates between 0 <-> 1 for each interrupt
volatile int error = 0 ;   // trick to reduce quantization noise

// output the latest sample, regenerate new sample every two interrupts (since it takes longer than 16us)
// Thus PWM is 62.5 kHz, actual sample rate 31.25 kSPS, so a hardware anti-aliasing filter with cutoff below
// 15kHz is recommended
// Since only 8 bit samples are output, the quantization noise probably pollutes the higher frequencies
//ISR (TIMER2_OVF_vect)
void timer_overflow_isr()
{
  if (phase == 0)
  {
    OCR2A = outsampl + 0x80 ;    // PWM output, offset is 50% duty cycle for 8 bit timer2
    int samp = stenzel_pink_sample () + error ;
    outsampl = samp >> 8 ;
    error = samp - (outsampl<<8) ;  // recalculate error
  }
  phase = 1 - phase ;
}

byte bitrev (byte a)  // reverse a 6 bit value
{
  byte r = 0 ;
  for (byte i = 0 ; i < 6 ; i++)
  {
    r <<= 1 ;
    r |= a & 1 ;
    a >>= 1 ;
  }
  return r ;
}

byte bitrevtab[0x40];  // caches bit reversal at cost of 64 bytes RAM

void build_revtab()
{
  for (byte i = 0 ; i < 0x40 ; i++)
    bitrevtab [i] = bitrev (i) ;
}

// Stenzel "new shade of pink" algorithm state

long lfsr = 0x5EED41F5 ;  // feedback RNG
int inc = 0xCCC ;         // incrementing bit per octave
int dec = 0xCCC ;         // decrementing bit per octave
int accum = 0 ;           // accumulator
int counter = 0xAAA ;     // used to chose bit position

void seed_stenzel (long seed)
{
  lfsr += seed ;
}

int stenzel_pink_sample ()
{
  int bit = lfsr < 0 ? 0xFFF : 0x000 ;  // step the RNG, keeping latest bit as a mask in variable 'bit'
  lfsr += lfsr ;
  if (bit)
    lfsr ^= 0x46000001L ;
    
  counter += 1;
  counter &= 0xFFF ;                    // step counter
  int bitmask = counter & -counter ;    // get lowest bit
  bitmask = bitrevtab [bitmask & 0x3F] ;// reverse using 2 lookups
  bitmask <<= 6 ;
  bitmask |= bitrevtab [bitmask >> 6] ;
  
  dec &= ~bitmask ;         // clear the dec bit at position given by bitmask
  dec |= inc & bitmask ;    // copy the bit at that position from inc to dec (cancelling the effect of inc)
  inc ^= bit & bitmask ;    // depending on latest random bit, perhaps flip the inc bit at that position
  accum += inc - dec ;      // difference calculates the linear interpolations for all 12 generators simultaneously
  int result = accum ;

  int twelve_bits = ((int) lfsr) & 0xFFF ;   // extract last 12 random bits generated and filter to correct spectral
  result += highfirtab [twelve_bits >> 6] ;  // power density at the higher end of spectrum
  result += lowfirtab  [twelve_bits & 0x3F] ;
  
  return result ;
}
