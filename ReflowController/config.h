#ifndef CONFIG_H
#define CONFIG_H

//#define ALWAYS_FIRST_RUN
//#define FAKE_HW 1
//#define PIDTUNE 1 // autotune wouldn't fit in the 28k available on my arduino pro micro. // TODO: Figure out if this can be made to work? (breaks menu if enabled)
#define WITH_BEEPER // Enables Beeper
// #define WITH_FAN // Enables Fan
//#define SERIAL_VERBOSE

// run a calibration loop that measures how many timer ticks occur between 2 zero corssings
// FIXME: does not work reliably at the moment, so a oscilloscope-determined value is used.
//#define WITH_CALIBRATION 1 // loop timing calibration

//#define MAINS_60HZ true // define this if your power is 60hz for proper timing otherwise comment out this line for 50hz
#define MAINS_50HZ true // define this if your power is 60hz for proper timing otherwise comment out this line for 50hz

#define WITH_SPLASH 1

// display
#define ST7735_CHIPSET ST7735_INITR_REDTAB // <= Set ST7735 LCD chipset/variation here (from list in PDQ_ST7735_config.h)
static const uint8_t PIN_LCD_CS   = 10;
static const uint8_t PIN_LCD_DC   = 9;
static const uint8_t PIN_LCD_RST  = 8;
static const uint8_t LCD_ROTATION = 3; // 0/2-> portrait, 1/3-> landscape

// thermostat
static const uint8_t PIN_TC_CS   = A1;
static const uint8_t PIN_TC_DO   = 12;
static const uint8_t PIN_TC_CLK  = 13;

// buzzer
static const uint8_t PIN_BEEPER = A5; // Beeper Out
static const uint16_t BEEP_FREQ = 1976; // B6 note

// encoder
static const uint8_t PIN_ENC_A           = A2;
static const uint8_t PIN_ENC_B           = A3;
static const uint8_t PIN_ENC_BTN         = A4;
static const uint8_t ENC_STEPS_PER_NOTCH = 4;
static const boolean IS_ENC_ACTIVE       = false; // encoder module actively fed with VCC ( seems to works badly if set to true )

// zerox detector
static const uint8_t PIN_ZX = 2;
static const uint8_t INT_ZX = digitalPinToInterrupt(PIN_ZX);

// other peripherals
static const uint8_t PIN_HEATER = 3;
static const uint8_t PIN_FAN    = A0;

// default values
// see: https://www.compuphase.com/electronics/reflowsolderprofiles.htm  
static const uint8_t DEFAULT_SOAK_TEPM_A      = 110;
static const uint8_t DEFAULT_SOAK_TEPM_B      = 160; 
static const uint8_t DEFAULT_SOAK_DURATION    = 180; 
static const uint8_t DEFAULT_PEAK_TEPM        = 200;
static const uint8_t DEFAULT_PEAK_DURATION    = 15;
static const float   DEFAULT_RAMP_UP_RATE     = 1.2; // degrees / second (keep it about 1/2 of maximum to prevent PID overshooting)
static const float   DEFAULT_RAMP_DOWN_RATE   = 2.0; // degrees / second
static const uint8_t FACTORY_FAN_ASSIST_SPEED = 33;

// various settings
static const uint8_t NUM_TEMP_READINGS  = 5;
static const uint8_t TC_ERROR_TOLERANCE = 5; // allow for n consecutive errors due to noisy power supply before bailing out

/*
Kp: Determines how aggressively the PID reacts to the current amount of error (Proportional) (double >=0)
Ki: Determines how aggressively the PID reacts to error over time (Integral) (double>=0)
Kd: Determines how aggressively the PID reacts to the change in error (Derivative) (double>=0)

Experimental method to tune PID:

> Set all gains to 0.
> Increase Kd until the system oscillates.
> Reduce Kd by a factor of 2-4.
> Set Kp to about 1% of Kd.
> Increase Kp until oscillations start.
> Decrease Kp by a factor of 2-4.
> Set Ki to about 1% of Kp.
> Increase Ki until oscillations start.
> Decrease Ki by a factor of 2-4.
*/

#define PID_SAMPLE_TIME 200
#define FACTORY_KP  1.75 // 1.75 //4.0 
#define FACTORY_KI 0.03 // 0.03 // 0.05 
#define FACTORY_KD 3.0 //3.0//2.0 

// allow the user to set their own configuration, which will not be committed to git
#if __has_include("userdefs.h")
#include "userdefs.h"
#endif

#endif // CONFIG_H
