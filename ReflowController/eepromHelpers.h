#ifndef EEPROM_HELPERS_H
#define EEPROM_HELPERS_H

#include <avr/eeprom.h>
#include <EEPROM.h>

// EEPROM offsets
#ifdef WITH_FAN
const uint16_t offsetFanSpeed   = maxProfiles * sizeof(Profile_t) + 1; // one byte
#endif
const uint16_t offsetProfileNum = maxProfiles * sizeof(Profile_t) + 2; // one byte
const uint16_t offsetPidConfig  = maxProfiles * sizeof(Profile_t) + 3; // sizeof(PID_t)


bool savePID() {
  do {} while (!(eeprom_is_ready()));
  eeprom_write_block(&heaterPID, (void *)offsetPidConfig, sizeof(PID_t));
  return true;
}

bool loadPID() {
  do {} while (!(eeprom_is_ready()));
  eeprom_read_block(&heaterPID, (void *)offsetPidConfig, sizeof(PID_t));
  return true;  
}

#ifdef WITH_FAN
void saveFanSpeed() {
  EEPROM.write(offsetFanSpeed, (uint8_t)fanAssistSpeed & 0xff);
  delay(250);
}

void loadFanSpeed() {
  fanAssistSpeed = EEPROM.read(offsetFanSpeed) & 0xff;
}
#endif

void saveLastUsedProfile() {
  EEPROM.write(offsetProfileNum, (uint8_t)activeProfileId & 0xff);
}

bool loadParameters(uint8_t profile) {
  uint16_t offset = profile * sizeof(Profile_t);

  do {} while (!(eeprom_is_ready()));
  eeprom_read_block(&activeProfile, (void *)offset, sizeof(Profile_t));

// TODO: WITH_CHECKSUM is defined in ReflowController.ino - is there any chance it could ever be set in here?
#ifdef WITH_CHECKSUM
  return activeProfile.checksum == crc8((uint8_t *)&activeProfile, sizeof(Profile_t) - sizeof(uint8_t));
#else
  return true;  
#endif
}

void loadLastUsedProfile() {
  activeProfileId = EEPROM.read(offsetProfileNum) & 0xff;
  loadParameters(activeProfileId);
}

bool saveParameters(uint8_t profile) {
#ifndef PIDTUNE
  uint16_t offset = profile * sizeof(Profile_t);

// TODO: WITH_CHECKSUM is defined in ReflowController.ino - is there any chance it could ever be set in here?
#ifdef WITH_CHECKSUM
  activeProfile.checksum = crc8((uint8_t *)&activeProfile, sizeof(Profile_t) - sizeof(uint8_t));
#endif

  do {} while (!(eeprom_is_ready()));
  eeprom_write_block(&activeProfile, (void *)offset, sizeof(Profile_t));
#endif
  return true;
}

#endif // EEPROM_HELPERS_H
