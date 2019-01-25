// ----------------------------------------------------------------------------
// Reflow Oven Controller
// (c) 2017 Debugged and restructured by David Sanz Kirbis
// (c) 2014 Karl Pitrich <karl@pitrich.com>
// (c) 2012-2013 Ed Simmons
// ----------------------------------------------------------------------------

#include <avr/eeprom.h>
#include <EEPROM.h>
#include <PID_v1.h>
#include <SPI.h>
#include <PDQ_GFX.h>             // PDQ: Core graphics library
#include "PDQ_ST7735_config.h"   // PDQ: ST7735 pins and other setup for this sketch

#include <PDQ_ST7735.h>          // PDQ: Hardware-specific driver library
#include <Menu.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include "max6675.h"

#include "portMacros.h"
#include "temperature.h"
#include "helpers.h"
#include "UI.h"
#include "globalDefs.h"

#ifdef PIDTUNE
#include <PID_AutoTune_v0.h>
#endif

// ----------------------------------------------------------------------------
volatile uint32_t    timerTicks       = 0;
volatile uint8_t     phaseCounter     = 0;
static const uint8_t TIMER1_PERIOD_US = 100;
// ----------------------------------------------------------------------------
uint32_t lastUpdate        = 0;
uint32_t lastDisplayUpdate = 0;
State    previousState     = Idle;
bool     stateChanged      = false;
uint32_t stateChangedTicks = 0;
// ----------------------------------------------------------------------------
// PID

PID PID(&Input, &Output, &Setpoint, heaterPID.Kp, heaterPID.Ki, heaterPID.Kd, DIRECT);

#ifdef PIDTUNE
PID_ATune PIDTune(&Input, &Output);

double aTuneStep       =  50,
       aTuneNoise      =   1,
       aTuneStartValue =  50; // is set to Output, i.e. 0-100% of Heater

unsigned int aTuneLookBack = 30;
#endif

/*************************************/

typedef struct {
  double temp;
  uint16_t ticks;
} Temp_t;

Temp_t airTemp[NUM_TEMP_READINGS];

double readingsT1[NUM_TEMP_READINGS]; // the readings used to make a stable temp rolling average
double runningTotalRampRate;
double rateOfRise = 0;          // the result that is displayed
double totalT1 = 0;             // the running total
double averageT1 = 0;           // the average
uint8_t index = 0;              // the index of the current reading
uint8_t thermocoupleErrorCount;

// ----------------------------------------------------------------------------
// Ensure that Solid State Relais are off when starting
//
void setupPins(void) {

pinAsOutput(PIN_HEATER);
digitalLow(PIN_HEATER); // off
#ifdef WITH_FAN
pinAsOutput(PIN_FAN);
digitalHigh(PIN_FAN);
#endif
pinAsInputPullUp(PIN_ZX);
pinAsOutput(PIN_TC_CS);
pinAsOutput(PIN_LCD_CS);
pinAsOutput(PIN_TC_CS);
#ifdef WITH_BEEPER
    pinAsOutput(PIN_BEEPER);
#endif

}
// ----------------------------------------------------------------------------
void killRelayPins(void) {
  Timer1.stop();
  detachInterrupt(INT_ZX);
#ifdef WITH_FAN
  digitalHigh(PIN_FAN);
#endif
  digitalHigh(PIN_HEATER);
  //PORTD |= (1 << PIN_HEATER) | (1 << PIN_FAN); // off - TODO: *what* is off?
}

// ----------------------------------------------------------------------------
// wave packet control: only turn the solid state relais on for a percentage 
// of complete sinusoids (i.e. 1x 360°)

#define CHANNELS       2
#define CHANNEL_HEATER 0
#ifdef WITH_FAN
#define CHANNEL_FAN    1
#endif

typedef struct Channel_s {
  volatile uint8_t target; // percentage of on-time
  uint8_t state;           // current state counter
  int32_t next;            // when the next change in output shall occur  
  bool action;             // hi/lo active
  uint8_t pin;             // io pin of solid state relais
} Channel_t;

Channel_t Channels[CHANNELS] = {
#ifdef WITH_FAN
  { 0, 0, 0, false, PIN_HEATER }, // heater
  { 0, 0, 0, false, PIN_FAN }     // fan
#else
  { 0, 0, 0, false, PIN_HEATER }
#endif
};

// delay to align relay activation with the actual zero crossing
uint16_t zxLoopDelay = 0;

#ifdef WITH_CALIBRATION
// calibrate zero crossing: how many timerIsr happen within one zero crossing
#define zxCalibrationLoops 128
struct {
  volatile int8_t iterations;
  volatile uint8_t measure[zxCalibrationLoops];
} zxLoopCalibration = {
  0, {}
};
#endif

// ----------------------------------------------------------------------------
//                             ZERO CROSSING ISR
// ----------------------------------------------------------------------------
// Zero Crossing ISR; per ZX, process one channel per interrupt only
// NB: use native port IO instead of digitalWrite for better performance
void zeroCrossingIsr(void) {
  static uint8_t ch = 0;

  // reset phase control timer
  phaseCounter = 0;
  TCNT1 = 0;

  zeroCrossTicks++;

  // calculate wave packet parameters
  Channels[ch].state += Channels[ch].target;
  if (Channels[ch].state >= 100) {
    Channels[ch].state -= 100;
    Channels[ch].action = false;
  }
  else {
    Channels[ch].action = true;
  }
  Channels[ch].next = timerTicks + zxLoopDelay; // delay added to reach the next zx

  ch = ((ch + 1) % CHANNELS); // next channel

#ifdef WITH_CALIBRATION
  if (zxLoopCalibration.iterations < zxCalibrationLoops) {
    zxLoopCalibration.iterations++;
  }
#endif
}

// ----------------------------------------------------------------------------
//                                    TIMER ISR
// ----------------------------------------------------------------------------
// timer interrupt handling

void timerIsr(void) { // ticks with 100µS
  static uint32_t lastTicks = 0;

#ifdef WITH_FAN
  // phase control for the fan 
  if (++phaseCounter > 90) {
    phaseCounter = 0;
  }

  if (phaseCounter > Channels[CHANNEL_FAN].target) {
    digitalLow(Channels[CHANNEL_FAN].pin); 
  }
  else {
    digitalHigh(Channels[CHANNEL_FAN].pin);
  }
#endif

  // wave packet control for heater
  if (Channels[CHANNEL_HEATER].next > lastTicks // FIXME: this looses ticks when overflowing
      && timerTicks > Channels[CHANNEL_HEATER].next) 
  {
    if (Channels[CHANNEL_HEATER].action) digitalLow(Channels[CHANNEL_HEATER].pin); //digitalWriteFast(Channels[CHANNEL_HEATER].pin, HIGH);
    else digitalHigh(Channels[CHANNEL_HEATER].pin);//digitalWriteFast(Channels[CHANNEL_HEATER].pin, LOW);
    lastTicks = timerTicks;
  }

  // handle encoder + button
  if (!(timerTicks % 10)) {
    Encoder.service();
  }

  timerTicks++;

#ifdef WITH_CALIBRATION
  if (zxLoopCalibration.iterations < zxCalibrationLoops) {
    zxLoopCalibration.measure[zxLoopCalibration.iterations]++;
  }
#endif
}

// ----------------------------------------------------------------------------

void abortWithError(int error) {
  killRelayPins();
  displayError(error);
}

// ----------------------------------------------------------------------------

void setup() {
#ifdef SERIAL_VERBOSE
  Serial.begin(115200);
  Serial.println("Reflow controller started");
#endif
  
  setupPins();
  setupTFT();

  if (firstRun()) {
    factoryReset();
    loadParameters(0);
  } 
  else {
    loadLastUsedProfile();
  }  
 
  do {
    // wait for MAX chip to stabilize
   delay(500);
   readThermocouple();
  }
  while ((tcStat > 0) && (thermocoupleErrorCount++ < TC_ERROR_TOLERANCE));

  if ((tcStat != 0) || (thermocoupleErrorCount  >= TC_ERROR_TOLERANCE)) {
    abortWithError(tcStat);
  }

  // initialize moving average filter
  runningTotalRampRate = (temperature + tempCorrVal) * NUM_TEMP_READINGS;
  for(int i = 0; i < NUM_TEMP_READINGS; i++) {
    airTemp[i].temp = temperature + tempCorrVal;
  }

#ifdef WITH_FAN
  loadFanSpeed();
#endif
  loadPID();
  loadTempCorr();

  PID.SetOutputLimits(0, 100); // max output 100%
  PID.SetSampleTime(PID_SAMPLE_TIME);
  PID.SetMode(AUTOMATIC);

  delay(1000);

  #ifdef WITH_BEEPER
    tone(PIN_BEEPER,BEEP_FREQ,100);
  #endif

#ifdef WITH_SPLASH
  displaySplash();
#endif

#ifdef WITH_CALIBRATION
  tft.setCursor(7, 99);  
  tft.print("Calibrating... ");
  delay(400);

  // FIXME: does not work reliably
  // TODO: find out what exactly it is that has been described as not working reliably by the above comment...?
  while (zxLoopDelay == 0) {
    if (zxLoopCalibration.iterations == zxCalibrationLoops) { // average tick measurements, dump 1st value
      for (int8_t l = 0; l < zxCalibrationLoops; l++) {
        zxLoopDelay += zxLoopCalibration.measure[l];
      }
      zxLoopDelay /= zxCalibrationLoops;
      zxLoopDelay -= 10; // compensating loop runtime
    }
  }
  tft.print(zxLoopDelay);
#else
  zxLoopDelay = DEFAULT_LOOP_DELAY;
#endif

// setupMenu();
  menuExit(Menu::actionDisplay); // reset to initial state
  MenuEngine.navigate(&miCycleStart);
  currentState = Settings;
  menuUpdateRequest = true;

  Timer1.initialize(TIMER1_PERIOD_US);
  Timer1.attachInterrupt(timerIsr);

  attachInterrupt(INT_ZX, zeroCrossingIsr, RISING);
  delay(100);
}

uint32_t lastRampTicks;
uint32_t lastSoakTicks;

void updateRampSetpoint(bool down = false) {
  if (zeroCrossTicks > lastRampTicks + TICKS_PER_UPDATE) {
    double rate = (down) ? activeProfile.rampDownRate : activeProfile.rampUpRate;
    Setpoint += (rate / (float)TICKS_PER_SEC * (zeroCrossTicks - lastRampTicks)) * ((down) ? -1 : 1);
    lastRampTicks = zeroCrossTicks;
  }
}

void updateSoakSetpoint(bool down = false) {
  if (zeroCrossTicks > lastSoakTicks + TICKS_PER_UPDATE) {
    double rate = (activeProfile.soakTempB-activeProfile.soakTempA)/(float)activeProfile.soakDuration;
    Setpoint += (rate / (float)TICKS_PER_SEC * (zeroCrossTicks - lastSoakTicks)) * ((down) ? -1 : 1);
    lastSoakTicks = zeroCrossTicks;
  }
}

// ----------------------------------------------------------------------------

#ifdef PIDTUNE
void toggleAutoTune() {
 if(currentState != Tune) { //Set the output to the desired starting frequency.
    currentState = Tune;

    Output = aTuneStartValue;
    PIDTune.SetNoiseBand(aTuneNoise);
    PIDTune.SetOutputStep(aTuneStep);
    PIDTune.SetLookbackSec((int)aTuneLookBack);
  }
  else { // cancel autotune
    PIDTune.Cancel();
    currentState = CoolDown;
  }
}
#endif // PIDTUNE

// ----------------------------------------------------------------------------

void loop(void) 
{
  // --------------------------------------------------------------------------
  // handle encoder
  //
  encMovement = Encoder.getValue();
  if (encMovement) {
    encAbsolute += encMovement;
    if (currentState == Settings) {
      MenuEngine.navigate((encMovement > 0) ? MenuEngine.getNext() : MenuEngine.getPrev());
      menuUpdateRequest = true;
    }
  }

  // --------------------------------------------------------------------------
  // handle button
  //
  switch (Encoder.getButton()) {
    case ClickEncoder::Clicked:
      if (currentState == Complete) { // at end of cycle; reset at click
        menuExit(Menu::actionDisplay); // reset to initial state
        MenuEngine.navigate(&miCycleStart);
        currentState = Settings;
        menuUpdateRequest = true;
      }
      else if (currentState < UIMenuEnd) {
        menuUpdateRequest = true;
        MenuEngine.invoke();
      }
      else if (currentState > UIMenuEnd) {
        currentState = CoolDown;
      }
      break;

    case ClickEncoder::DoubleClicked:
      if (currentState < UIMenuEnd) {
        if (MenuEngine.getParent() != &miExit) {
          MenuEngine.navigate(MenuEngine.getParent());
          menuUpdateRequest = true;
        }
      }
      break;
  }

  // --------------------------------------------------------------------------
  // update current menu item while in edit mode
  //
  if (currentState == Edit) {
    if (MenuEngine.currentItem != &Menu::NullItem) {
      MenuEngine.executeCallbackAction(Menu::actionDisplay);      
    }
  }

  // --------------------------------------------------------------------------
  // handle menu update
  //
  if (menuUpdateRequest) {
    menuUpdateRequest = false;
    if (currentState < UIMenuEnd && !encMovement && currentState != Edit && previousState != Edit) { // clear menu on child/parent navigation
      tft.fillScreen(ST7735_WHITE);
    }  
    MenuEngine.render(renderMenuItem, menuItemsVisible);
  }

  // --------------------------------------------------------------------------
  // track state changes
  //
  if (currentState != previousState) {
    stateChangedTicks = zeroCrossTicks;
    stateChanged = true;
    previousState = currentState;
  }

  // --------------------------------------------------------------------------

  if (zeroCrossTicks - lastUpdate >= TICKS_PER_UPDATE) {
    uint32_t deltaT = zeroCrossTicks - lastUpdate;
    lastUpdate = zeroCrossTicks;


    readThermocouple(); // should be sufficient to read it every 250ms or 500ms   


    if (tcStat > 0) {
      thermocoupleErrorCount++;
       if ((thermocoupleErrorCount > TC_ERROR_TOLERANCE) && (currentState != Edit)) {
        abortWithError(tcStat);
      } else thermocoupleErrorCount = 0;
    }
    else {
        thermocoupleErrorCount = 0;
#if 0 // verbose thermocouple error bits - TODO: is this a fancy way of out-commenting something?
        tft.setCursor(10, 40);
        for (uint8_t mask = B111; mask; mask >>= 1) {
          tft.print(mask & tSensor.stat ? '1' : '0');
        }
#endif
        // rolling average of the temp T1 and T2
        totalT1 -= readingsT1[index];       // subtract the last reading
        readingsT1[index] = temperature + tempCorrVal;
        totalT1 += readingsT1[index];       // add the reading to the total
        index = (index + 1) % NUM_TEMP_READINGS;  // next position
        averageT1 = totalT1 / (float)NUM_TEMP_READINGS;  // calculate the average temp
    
        // need to keep track of a few past readings in order to work out rate of rise
        for (int i = 1; i < NUM_TEMP_READINGS; i++) { // iterate over all previous entries, moving them backwards one index
          airTemp[i - 1].temp = airTemp[i].temp;
          airTemp[i - 1].ticks = airTemp[i].ticks;     
        }
    
        airTemp[NUM_TEMP_READINGS - 1].temp = averageT1; // update the last index with the newest average
        airTemp[NUM_TEMP_READINGS - 1].ticks = (uint16_t)deltaT;
    
        // calculate rate of temperature change
        uint32_t collectTicks = 0;
        for (int i = 0; i < NUM_TEMP_READINGS; i++) {
          collectTicks += airTemp[i].ticks;
        }
        float tempDiff = (airTemp[NUM_TEMP_READINGS - 1].temp - airTemp[0].temp);
        float timeDiff = collectTicks / (float)(TICKS_PER_SEC);
        
        rampRate = tempDiff / timeDiff;
     
        Input = airTemp[NUM_TEMP_READINGS - 1].temp; // update the variable the PID reads
           
#ifdef SERIAL_VERBOSE
       Serial.write((uint8_t)Input);
#endif
    }
    // display update
    if (zeroCrossTicks - lastDisplayUpdate >= TICKS_TO_REDRAW) {
      lastDisplayUpdate = zeroCrossTicks;
      if (currentState > UIMenuEnd) {
        updateProcessDisplay();
      }
      else displayThermocoupleData(1, tft.height()-16);
    }

    switch (currentState) {
#ifndef PIDTUNE
      case RampToSoak:
        if (stateChanged) {
          lastRampTicks = zeroCrossTicks;
          stateChanged = false;
          Output = 50;
          PID.SetMode(AUTOMATIC);
          PID.SetControllerDirection(DIRECT);
          PID.SetTunings(heaterPID.Kp, heaterPID.Ki, heaterPID.Kd);
          Setpoint = Input;
          #ifdef WITH_BEEPER
              tone(PIN_BEEPER,BEEP_FREQ,100);
          #endif
        }

        updateRampSetpoint();

        if (Setpoint >= activeProfile.soakTempA - 1) {
          currentState = Soak;
        }
        break;

      case Soak:
        if (stateChanged) {
          lastSoakTicks = zeroCrossTicks;
          stateChanged = false;
          Setpoint = activeProfile.soakTempA;
        }

        updateSoakSetpoint();

        if (zeroCrossTicks - stateChangedTicks >= (uint32_t)activeProfile.soakDuration * TICKS_PER_SEC) {
          currentState = RampUp;
        }
        break;

      case RampUp:
        if (stateChanged) {
          stateChanged = false;
          lastRampTicks = zeroCrossTicks;
        }

        updateRampSetpoint();

        if (Setpoint >= activeProfile.peakTemp - 1) {
          Setpoint = activeProfile.peakTemp;
          currentState = Peak;
        }
        break;

      case Peak:
        if (stateChanged) {
          stateChanged = false;
          Setpoint = activeProfile.peakTemp;
        }

        if (zeroCrossTicks - stateChangedTicks >= (uint32_t)activeProfile.peakDuration * TICKS_PER_SEC) {
          currentState = RampDown;
        }
        break;

      case RampDown:
        if (stateChanged) {
          stateChanged = false;
          lastRampTicks = zeroCrossTicks;
          PID.SetControllerDirection(REVERSE);
#ifdef WITH_FAN
          PID.SetTunings(fanPID.Kp, fanPID.Ki, fanPID.Kd);
#endif
          Setpoint = activeProfile.peakTemp - 15; // get it all going with a bit of a kick! very sluggish here otherwise, too hot too long...
#ifdef WITH_BEEPER
          tone(PIN_BEEPER, BEEP_FREQ, 3000); // beep as a reminder that CoolDown starts (and maybe open up the oven door for fast enough cooldown)
#endif
        }

        updateRampSetpoint(true);

        if (Setpoint <= idleTemp) {
          currentState = CoolDown;
        }
        break;
#endif
      case CoolDown:
        if (stateChanged) {
          stateChanged = false;
          PID.SetControllerDirection(REVERSE);
#ifdef WITH_FAN
          PID.SetTunings(fanPID.Kp, fanPID.Ki, fanPID.Kd);
#endif
          Setpoint = idleTemp;

        }

        if (Input < (idleTemp + 5)) {
          currentState = Complete;
          PID.SetMode(MANUAL);
          Output = 0;
          #ifdef WITH_BEEPER
            tone(PIN_BEEPER,BEEP_FREQ,500);  //End Beep
            delay(1500);
            tone(PIN_BEEPER,BEEP_FREQ,500);
            delay(1500);
            tone(PIN_BEEPER,BEEP_FREQ,1500);
          #endif
        }

#ifdef PIDTUNE
      case Tune:
        {
          Setpoint = 210.0;
          int8_t val = PIDTune.Runtime();
         // PIDTune.setpoint = 210.0; // is private inside PIDTune

          if (val != 0) {
            currentState = CoolDown;
          }

          if (currentState != Tune) { // we're done, set the tuning parameters
            heaterPID.Kp = PIDTune.GetKp();
            heaterPID.Ki = PIDTune.GetKi();
            heaterPID.Kd = PIDTune.GetKd();
            
            savePID();

            tft.setCursor(40, 40);
            tft.print("Kp: "); tft.print((uint32_t)(heaterPID.Kp * 100));
            tft.setCursor(40, 52);
            tft.print("Ki: "); tft.print((uint32_t)(heaterPID.Ki * 100));
            tft.setCursor(40, 64);
            tft.print("Kd: "); tft.print((uint32_t)(heaterPID.Kd * 100));
          }
        }
        break;
#endif
    }
  }

  // safety check that we're not doing something stupid. 
  // if the thermocouple is wired backwards, temp goes DOWN when it increases
  // during cooling, the t962a lags a long way behind, hence the hugely lenient cooling allowance.
  // both of these errors are blocking and do not exit!
  //if (Setpoint > Input + 50) abortWithError(10); // if we're 50 degree cooler than setpoint, abort
  //if (Input > Setpoint + 50) abortWithError(20); // or 50 degrees hotter, also abort
  
#ifndef PIDTUNE
  PID.Compute();

  // decides which control signal is fed to the output for this cycle
  if (   currentState != RampDown
      && currentState != CoolDown
      && currentState != Settings
      && currentState != Complete
      && currentState != Idle
      && currentState != Settings
      && currentState != Edit)
  {
    heaterValue = Output;
#ifdef WITH_FAN
    fanValue = fanAssistSpeed;
#endif
  } 
  else {
    heaterValue = 0;
#ifdef WITH_FAN
    fanValue = Output;
#endif
  }
#else
  heaterValue = Output;
#ifdef WITH_FAN
  fanValue = fanAssistSpeed;
#endif
#endif

  Channels[CHANNEL_HEATER].target = heaterValue;

#ifdef WITH_FAN
  double fanTmp = 90.0 / 100.0 * fanValue; // 0-100% -> 0-90° phase control
  Channels[CHANNEL_FAN].target = 90 - (uint8_t)fanTmp;
#endif
}

void saveProfile(unsigned int targetProfile, bool quiet) {
#ifndef PIDTUNE
  activeProfileId = targetProfile;

  if (!quiet) {
    memoryFeedbackScreen(activeProfileId, false);
  }
  saveParameters(activeProfileId); // activeProfileId is modified by the menu code directly, this method is called by a menu action

  if (!quiet) delay(500);
#endif
}

#define WITH_CHECKSUM

bool firstRun() { 
#ifndef PIDTUNE
#ifndef ALWAYS_FIRST_RUN

  // if all bytes of a profile in the middle of the eeprom space are 255, we assume it's a first run
  unsigned int offset = 15 * sizeof(Profile_t);

  for (uint16_t i = offset; i < offset + sizeof(Profile_t); i++) {
    if (EEPROM.read(i) != 255) {
      return false;
    }
  }
#endif
#endif
  return true;
}
