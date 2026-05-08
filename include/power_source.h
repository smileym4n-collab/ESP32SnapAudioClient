#pragma once

#include <Arduino.h>

namespace app_config {

enum class PowerSource : uint8_t { Battery, Mains };

inline const char *powerSourceName(PowerSource source) {
  switch (source) {
    case PowerSource::Battery:
      return "battery";
    case PowerSource::Mains:
      return "mains";
    default:
      return "battery";
  }
}

inline bool parsePowerSource(const String &value, PowerSource &source) {
  String normalized = value;
  normalized.trim();

  if (normalized == "battery") {
    source = PowerSource::Battery;
    return true;
  }
  if (normalized == "mains") {
    source = PowerSource::Mains;
    return true;
  }
  return false;
}

}  // namespace app_config
