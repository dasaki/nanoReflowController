#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "globalDefs.h"

MAX6675 thermocouple(PIN_TC_CLK, PIN_TC_CS, PIN_TC_DO);

void readThermocouple() {
  uint8_t lcdState = digitalState(PIN_LCD_CS);
  digitalHigh(PIN_LCD_CS);
  digitalLow(PIN_TC_CS);
  delay(1);
  double reading = thermocouple.readCelsius();
   
  if (reading == NAN) {
    tcStat = 1;
  } else {
    temperature = reading;
    tcStat = 0;
  }
#ifdef SERIAL_VERBOSE
  Serial.print("temp: ");
  Serial.println(round(temperature));
#endif
  digitalHigh(PIN_TC_CS); 

  if (lcdState == 0) {
    digitalLow(PIN_LCD_CS);
  } else {
    digitalHigh(PIN_LCD_CS);
  }
}

#endif
