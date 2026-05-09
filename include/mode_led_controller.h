#pragma once

#include "snapclient_config.h"

class ModeLedController {
 public:
  void begin();
  void setMode(app_config::OperatingMode mode);
  void setBluetoothClientConnected(bool connected);
  void setWifiConnected(bool connected);
  void setLowBatteryWarningActive(bool active);
  void update();

 private:
  void writeWifiLed(bool on);
  void writeBtLed(bool on);
  void writeLowBatteryLed(bool on);
  void writeModeLed(bool on);
  void writeAllModeLeds(bool on);

  app_config::OperatingMode activeMode_ = app_config::OperatingMode::Snapclient;
  uint32_t lastToggleMs_ = 0;
  uint32_t lastLowBatteryToggleMs_ = 0;
  bool bluetoothClientConnected_ = false;
  bool wifiConnected_ = false;
  bool lowBatteryWarningActive_ = false;
  bool lowBatteryLedOn_ = false;
  bool lowBatteryRedPhase_ = false;
  bool wifiLedOn_ = false;
  bool btLedOn_ = false;
};
