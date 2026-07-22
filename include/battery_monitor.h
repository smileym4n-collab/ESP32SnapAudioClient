#pragma once

#include <Arduino.h>

struct BatteryStatus {
  bool available = false;
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  int percent = 0;
};

class BatteryMonitor {
 public:
  bool begin();
  void update(bool force = false);
  BatteryStatus status() const;

 private:
  bool detectDevice();
  bool writeRegister(uint8_t reg, uint16_t value);
  bool readRegister(uint8_t reg, uint16_t &value) const;
  int percentFromVoltage(float packVoltage) const;

  bool enabled_ = false;
  bool initialized_ = false;
  uint8_t i2cAddress_ = 0;
  uint32_t lastPollMs_ = 0;
  float packVoltage_ = 0.0f;
  float currentAmps_ = 0.0f;
  float powerWatts_ = 0.0f;
  float filteredPercent_ = 0.0f;
  int percent_ = 0;
};
