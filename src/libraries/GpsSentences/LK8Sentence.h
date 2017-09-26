#ifndef LK8SENTENCE_H
#define LK8SENTENCE_H

#include <Arduino.h>
#include <digit.h>

#define USE_MYWAY

#ifdef USE_MYWAY
#define LK8_SENTENCE_TAG "$LK8EX1,,A,V,T,B,*P\r\n"
#define LK8_SENTENCE_TAG_SIZE 21
#define LK8_SENTENCE_PRESSURE_POS 8
#define LK8_SENTENCE_ALTI_POS 9
#define LK8_SENTENCE_VARIO_POS 11
#define LK8_SENTENCE_TEMP_POS 13
#define LK8_SENTENCE_BAT_POS 15
#define LK8_SENTENCE_PARITY_POS 18
#define LK8_SENTENCE_PRESSURE_PRECISION 0
#define LK8_SENTENCE_ALTI_PRECISION 0
#define LK8_SENTENCE_VARIO_PRECISION 0
#else // USE_MYWAY
/* no special character for pressure, just for alti "A", vario "V" and parity "P" */
#define LK8_SENTENCE_TAG "$LK8EX1,,A,V,99,999,*P\r\n"
#define LK8_SENTENCE_TAG_SIZE 24
#define LK8_SENTENCE_PRESSURE_POS 8
#define LK8_SENTENCE_ALTI_POS 9
#define LK8_SENTENCE_VARIO_POS 11
#define LK8_SENTENCE_PARITY_POS 21
#define LK8_SENTENCE_PRESSURE_PRECISION 0
#define LK8_SENTENCE_ALTI_PRECISION 0
#define LK8_SENTENCE_VARIO_PRECISION 0
#endif // USE_MYWAY

#define LK8_BASE_SEA_PRESSURE 1013.25

class LK8Sentence {

 public:
 LK8Sentence() : valueDigit() { } 
  void begin(double alti, double vario);
  bool available(void);
  uint8_t get(void);
  
#ifdef USE_MYWAY
  void setExtra(double t, double v);  
#endif // USE_MYWAY

 private:
  double alti;
  double vario;
#ifdef USE_MYWAY
  double temp;
  double volt;
#endif // USE_MYWAY
  Digit valueDigit;
  uint8_t parity;
  HexDigit parityDigit;
  uint8_t tagPos;
};

#endif
