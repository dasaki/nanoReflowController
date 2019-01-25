#ifndef GLOBAL_DEFS_H
#define GLOBAL_DEFS_H

#include "config.h"

#ifdef MAINS_50HZ 
  static const uint8_t DEFAULT_LOOP_DELAY = 89;  // should be about 16% less for 60Hz mains
  static const uint8_t TICKS_PER_SEC      = 100; // for 50Hz mains:  2*50Hz = 100 ticks per second
#else
#ifdef MAINS_60HZ
  static const uint8_t DEFAULT_LOOP_DELAY = 74;  // 60Hz mains = 74?
  static const uint8_t TICKS_PER_SEC      = 120; // for 60Hz mains:  2*60Hz = 120 ticks per second
#endif
#endif

static const uint8_t TICKS_PER_UPDATE     = 25; // 
static const uint8_t TICKS_TO_REDRAW      = 50; // 

const char * ver = "3.2";

double temperature;
uint8_t tcStat = 0;

double Setpoint;
double Input;
double Output;

#ifdef WITH_FAN
uint8_t fanValue;
#endif
uint8_t heaterValue;
double rampRate = 0;

// ----------------------------------

typedef struct {
  double Kp;
  double Ki;
  double Kd;
} PID_t;

PID_t heaterPID = { FACTORY_KP, FACTORY_KI,  FACTORY_KD };
//PID_t heaterPID = { 4.00, 0.05,  2.00 };
#ifdef WITH_FAN
PID_t fanPID    = { 1.00, 0.00, 0.00 };
#endif

int idleTemp = 50; // the temperature at which to consider the oven safe to leave to cool naturally
uint32_t startCycleZeroCrossTicks;
volatile uint32_t zeroCrossTicks = 0;
char buf[20]; // generic char buffer

#ifdef WITH_FAN
int fanAssistSpeed = 33; // default fan speed
#endif

// ----------------------------------------------------------------------------
// state machine

typedef enum {
  None     = 0,
  Idle     = 1,
  Settings = 2,
  Edit     = 3,

  UIMenuEnd = 9,

  RampToSoak = 10,
  Soak,
  RampUp,
  Peak,
  RampDown,
  CoolDown,

  Complete = 20,

  Tune = 30
} State;

State currentState  = Idle;


// data type for the values used in the reflow profile
typedef struct profileValues_s {
  int16_t soakTempA;
  int16_t soakTempB;
  int16_t soakDuration;
  int16_t peakTemp;
  int16_t peakDuration;
  double  rampUpRate;
  double  rampDownRate;
  uint8_t checksum;
} Profile_t;

Profile_t activeProfile; // the one and only instance
int activeProfileId = 0;

const uint8_t maxProfiles = 30;

void makeDefaultProfile() {
  activeProfile.soakTempA     = DEFAULT_SOAK_TEPM_A; 
  activeProfile.soakTempB     = DEFAULT_SOAK_TEPM_B; 
  activeProfile.soakDuration = DEFAULT_SOAK_DURATION; 
  activeProfile.peakTemp     = DEFAULT_PEAK_TEPM;
  activeProfile.peakDuration =  DEFAULT_PEAK_DURATION;
  activeProfile.rampUpRate   =   DEFAULT_RAMP_UP_RATE;
  activeProfile.rampDownRate =   DEFAULT_RAMP_DOWN_RATE;
}

#endif // GLOBAL_DEFS_H
