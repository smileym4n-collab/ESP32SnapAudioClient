#include "snapclient_dsp_store.h"

#include <Preferences.h>

#include "snapclient_config.h"

namespace {

constexpr char kPrefsNamespace[] = "snap-dsp";
constexpr char kPrefsBlobKey[] = "config";
constexpr uint32_t kDspPrefsMagic = 0x44535033;  // "DSP3"
constexpr uint16_t kDspPrefsVersion = 2;

struct StoredDspConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  app_config::SnapclientDspConfig config;
};

bool validStoredHeader(const StoredDspConfig &stored) {
  return stored.magic == kDspPrefsMagic &&
         stored.version == kDspPrefsVersion &&
         stored.size == sizeof(app_config::SnapclientDspConfig);
}

}  // namespace

app_config::SnapclientDspConfig loadSnapclientDspConfig() {
  app_config::SnapclientDspConfig config = app_config::SNAPCLIENT_DSP_CONFIG;

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    Serial.println("[dsp] failed to open DSP preferences");
    app_config::sanitizeSnapclientDspConfig(
        config, app_config::SNAPCLIENT_DSP_CONFIG);
    return config;
  }

  StoredDspConfig stored{};
  const size_t readBytes =
      prefs.getBytes(kPrefsBlobKey, &stored, sizeof(stored));
  prefs.end();

  if (readBytes == sizeof(stored) && validStoredHeader(stored)) {
    config = stored.config;
  } else if (readBytes > 0) {
    Serial.println("[dsp] stored DSP preferences invalid, using defaults");
  }

  app_config::sanitizeSnapclientDspConfig(
      config, app_config::SNAPCLIENT_DSP_CONFIG);
  return config;
}

void saveSnapclientDspConfig(const app_config::SnapclientDspConfig &config) {
  app_config::SnapclientDspConfig sanitized = config;
  app_config::sanitizeSnapclientDspConfig(
      sanitized, app_config::SNAPCLIENT_DSP_CONFIG);

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    Serial.println("[dsp] failed to store DSP preferences");
    return;
  }

  const StoredDspConfig stored = {
      kDspPrefsMagic,
      kDspPrefsVersion,
      sizeof(app_config::SnapclientDspConfig),
      sanitized,
  };
  prefs.putBytes(kPrefsBlobKey, &stored, sizeof(stored));
  prefs.end();
}

void resetSnapclientDspConfig() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    Serial.println("[dsp] failed to reset DSP preferences");
    return;
  }

  prefs.remove(kPrefsBlobKey);
  prefs.end();
}
