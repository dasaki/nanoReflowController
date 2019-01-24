#ifndef HELPERS_H
#define HELPERS_H

// ----------------------------------------------------------------------------

#include <Arduino.h>

// ----------------------------------------------------------------------------
// 

class ScopedTimer {
public:
  ScopedTimer(const char * Label)
    : label(Label), ts(millis())
  {
  }
  ~ScopedTimer() {
    Serial.print(label); Serial.print(": ");
    Serial.println(millis() - ts);
  }
private:
  const char *label;
  const unsigned long ts;
};

// ----------------------------------------------------------------------------

template<typename T> inline const T labs(T const& x) {
    return (x < 0) ? -x : x;
}

// ----------------------------------------------------------------------------

long lpow(int base, int exponent) {
  long result = 1;
  while (exponent) {
    if (exponent & 1) {
      result *= base;
    }
    exponent >>= 1;
    base *= base;
  }
  return result;
}

//double round(double x) {
//    return (x >= 0.0) ? floor(x + 0.5) : ceil(x - 0.5);
//}

// ----------------------------------------------------------------------------

void itoa10(int32_t n, char *result, bool preventMinus = false) {
  uint32_t u;
  uint16_t i = 0;

  if (n < 0) { // for negative number, prepend '-' and invert
    if (!preventMinus) {
      result[0] = '-';
      result++;    
    }
    u = ((uint32_t) -(n + 1)) + 1;
  }
  else { 
    u = (uint32_t)n;
  }
  
  do {
    result[i++] = '0' + u % 10;
    u /= 10;
  } 
  while (u > 0);
  
  // rotate string bytewise
  for (uint16_t j = 0; j < i / 2; ++j) {
    char tmp = result[j];
    result[j] = result[i - j - 1];
    result[i - j - 1] = tmp;
  }
  result[i] = '\0';
}

// ----------------------------------------------------------------------------

uint8_t countDigits(uint32_t n) {
  uint8_t d = 1;
  switch (n) {
    case  100000000 ... 999999999:  d++;
    case   10000000 ... 99999999:   d++;
    case    1000000 ... 9999999:    d++;
    case     100000 ... 999999:     d++;
    case      10000 ... 99999:      d++;
    case       1000 ... 9999:       d++;
    case        100 ... 999:        d++;
    case         10 ... 99:         d++;
  }
  return d;
}

// ----------------------------------------------------------------------------

void ftoa(char *buf, float val, int places) {
  if (signbit(val)) *buf++ = '-';

  int32_t digit = (int32_t)(val);
  const int32_t precision = lpow(10, places);
  int32_t decimal = lround((val - digit) * precision);

  if (labs(decimal) == precision) {
    signbit(val) ? digit-- : digit++;
    decimal = 0;
  }

  itoa10(labs(digit), buf);
  while (*buf != '\0') buf++;
  *buf++ = '.';

  if (decimal) {
    int32_t tmp = labs(decimal) * 10;
    while (tmp < precision) {
      *buf++ = '0';
      tmp *= 10;
    }
  }

  itoa10(labs(decimal), buf);
  if (decimal == 0) {
    while (places--) *buf++ = '0';
    *buf = '\0';
  }
}

// ----------------------------------------------------------------------------

void itostr(char *r, int16_t val, char *unit = NULL) {
  char *p = r, *u = unit;
  itoa10(val, p);
  while(*p != 0x00) p++;
  while(*u != 0x00) *p++ = *u++;
  *p = '\0';
}

// ---------------------------------------------------------------------------- 
// crc8
//
// Copyright (c) 2002 Colin O'Flynn
// Minor changes by M.Thomas 9/2004 
// ----------------------------------------------------------------------------

#define CRC8INIT    0x00
#define CRC8POLY    0x18              //0X18 = X^8+X^5+X^4+X^0

// ----------------------------------------------------------------------------

uint8_t crc8(uint8_t *data, uint16_t data_length) {
  uint8_t  b;
  uint8_t  bit_counter;
  uint8_t  crc = CRC8INIT;
  uint8_t  feedback_bit;
  uint16_t loop_count;
  
  for (loop_count = 0; loop_count != data_length; loop_count++) {
    b = data[loop_count];
    bit_counter = 8;

    do {
      feedback_bit = (crc ^ b) & 0x01;

      if (feedback_bit == 0x01) {
        crc = crc ^ CRC8POLY;
      }

      crc = (crc >> 1) & 0x7F;

      if (feedback_bit == 0x01) {
        crc = crc | 0x80;
      }

      b = b >> 1;
      bit_counter--;

    } while (bit_counter > 0);
  }
  
  return crc;
}

#endif // HELPERS_H
