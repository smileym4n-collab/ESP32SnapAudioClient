#include "power_source_store.h"

#include <Preferences.h>

#include "snapclient_config.h"

namespace {

constexpr char kPrefsNamespace[] = "power-config";
constexpr char kPrefsSourceKey[] = "source";

}  // namespace

app_config::PowerSource loadPowerSourcePreference() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    Serial.println("[power] failed to open power source preferences");
    return app_config::DEFAULT_POWER_SOURCE;
  }

  const uint8_t storedSource = prefs.getUChar(
      kPrefsSourceKey, static_cast<uint8_t>(app_config::DEFAULT_POWER_SOURCE));
  prefs.end();

  if (storedSource <= static_cast<uint8_t>(app_config::PowerSource::Mains)) {
    return static_cast<app_config::PowerSource>(storedSource);
  }

  return app_config::DEFAULT_POWER_SOURCE;
}

void savePowerSourcePreference(app_config::PowerSource source) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    Serial.println("[power] failed to store power source preference");
    return;
  }

  prefs.putUChar(kPrefsSourceKey, static_cast<uint8_t>(source));
  prefs.end();
}
