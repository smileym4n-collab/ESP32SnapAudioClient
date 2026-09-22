#pragma once

#include "runtime_mode.h"
#include "snapclient_config.h"

class ModeSwitchController {
 public:
  void begin(app_config::OperatingMode currentMode);
  void setRuntimeMode(RuntimeMode *runtimeMode);
  void update();

 private:
  bool isButtonActive() const;
  void processSerialInput();
  void printSerialHelp() const;
  void requestModeChange(app_config::OperatingMode nextMode, const char *source);
  void requestModeToggle();

  app_config::OperatingMode currentMode_ = app_config::OperatingMode::Snapclient;
  RuntimeMode *runtimeMode_ = nullptr;
  char dspLine_[192] = {};
  size_t dspLineLength_ = 0;
  bool dspLineActive_ = false;
  bool dspLineOverflow_ = false;
  bool buttonWasReleasedAfterBoot_ = false;
  bool pressHandled_ = false;
  uint32_t pressedSinceMs_ = 0;
};
