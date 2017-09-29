// ---------------------------------------------------------------------------
// Created by Tim Eckel - teckel@leethost.com
// Copyright 2013 License: GNU GPL v3 http://www.gnu.org/licenses/gpl-3.0.html
//
// See "toneAC.h" for purpose, syntax, version history, links, and more.
// ---------------------------------------------------------------------------

#include "toneAC.h"

#ifdef TONEAC_LENGTH
unsigned long _tAC_time; // Used to track end note with timer when playing note in the background.
#endif

#ifdef TONEAC_VOLUME
//uint8_t _tAC_volume[] = { 200, 100, 67, 50, 40, 33, 29, 22, 11, 2 }; // Duty for linear volume control.
uint8_t _tAC_volume[] = { 150, 72, 51, 38, 32, 23, 20, 19, 10, 2 }; //new duty values for three phased Low Power mode
#endif

void toneAC(unsigned long frequency
#ifdef TONEAC_VOLUME
            , uint8_t volume
#endif
#ifdef TONEAC_LENGTH
            , unsigned long length, uint8_t background
#endif
	    ) {

  /* check if no tone */ 
  if (frequency == 0
#ifdef TONEAC_VOLUME     
      || volume == 0
#endif
      ) { noToneAC(); return; } 

  /* check volume */
#ifdef TONEAC_VOLUME
  if (volume > 10) volume = 10;
#endif

  
  /* set PWM */
  PWMT1DREG |= _BV(PWMT1AMASK) | _BV(PWMT1BMASK); // Set timer 1 PWM pins to OUTPUT (because analogWrite does it too).

  uint8_t prescaler = _BV(CS10);                 // Try using prescaler 1 first.
  unsigned long top = F_CPU / frequency / 2 - 1; // Calculate the top.
  if (top > 65535) {                             // If not in the range for prescaler 1, use prescaler 256 (122 Hz and lower @ 16 MHz).
    prescaler = _BV(CS12);                       // Set the 256 prescaler bit.
    top = top / 256 - 1;                         // Calculate the top using prescaler 256.
  }

  /* set duty cycle */
#ifdef TONEAC_VOLUME
  unsigned int duty = top / _tAC_volume[volume - 1]; // Calculate the duty cycle (volume).
#else
  unsigned int duty = top >> 1;
#endif

  /* compute length time */
#ifdef TONEAC_LENGTH
  if (length > 0 && background) {  // Background tone playing, returns control to your sketch.

    _tAC_time = millis() + length; // Set when the note should end.
    TIMSK1 |= _BV(OCIE1A);         // Activate the timer interrupt.
  }
#endif

  /* set PWM register */
  ICR1   = top;                         // Set the top.
  if (TCNT1 > top) TCNT1 = top;         // Counter over the top, put within range.
  TCCR1B = _BV(WGM13)  | prescaler;     // Set PWM, phase and frequency corrected (top=ICR1) and prescaler.
  OCR1A  = duty;                        // three phased volume control (Low Power mode)
  OCR1B  = top - duty;

  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1B0); // Inverted/non-inverted mode (AC).

#ifdef TONEAC_LENGTH
  if (length > 0 && !background) { delay(length); noToneAC(); } // Just a simple delay, doesn't return control untill finished.
#endif
}

void noToneAC() {
#ifdef TONEAC_LENGTH
  TIMSK1 &= ~_BV(OCIE1A);     // Remove the timer interrupt.
#endif
  TCCR1B  = _BV(CS11);        // Default clock prescaler of 8.
  TCCR1A  = _BV(WGM10);       // Set to defaults so PWM can work like normal (PWM, phase corrected, 8bit).
  PWMT1PORT &= ~_BV(PWMT1AMASK); // Set timer 1 PWM pins to LOW.
  PWMT1PORT &= ~_BV(PWMT1BMASK); // Other timer 1 PWM pin also to LOW.
}

#ifdef TONEAC_LENGTH
ISR(TIMER1_COMPA_vect) { // Timer interrupt vector.
  if (millis() >= _tAC_time) noToneAC(); // Check to see if it's time for the note to end.
}
#endif
