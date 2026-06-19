#pragma once

#include <math.h>
#include <stdint.h>

#include <Arduino.h>

namespace app_config {

static constexpr uint8_t SNAPCLIENT_EQ_BAND_COUNT = 5;
static constexpr uint8_t SNAPCLIENT_EQ_PRESET_COUNT = 14;

enum class SnapclientEqFilterType : uint8_t {
  LowShelf,
  Peaking,
  HighShelf,
};

struct SnapclientEqBand {
  SnapclientEqFilterType type;
  float frequencyHz;
  float gainDb;
  float q;
  bool enabled;
};

struct SnapclientEqPreset {
  const char *name;
  const char *displayName;
  float preampDb;
  SnapclientEqBand bands[SNAPCLIENT_EQ_BAND_COUNT];
};

static const SnapclientEqPreset SNAPCLIENT_EQ_PRESETS[SNAPCLIENT_EQ_PRESET_COUNT] = {
    {"Flat",
     "Flat",
     0.0f,
     {{SnapclientEqFilterType::LowShelf, 80.0f, 0.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 3500.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 0.0f, 0.707f, true}}},
    {"Pop",
     "Pop",
     -4.0f,
     {{SnapclientEqFilterType::LowShelf, 90.0f, 2.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, -1.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 1500.0f, 1.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 4000.0f, 2.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 2.0f, 0.707f, true}}},
    {"Rock",
     "Rock",
     -5.0f,
     {{SnapclientEqFilterType::LowShelf, 80.0f, 3.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, -2.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, -1.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 3500.0f, 2.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 3.0f, 0.707f, true}}},
    {"Deep_Bass",
     "Deep Bass",
     -6.0f,
     {{SnapclientEqFilterType::LowShelf, 55.0f, 5.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 120.0f, 2.0f, 0.85f, true},
      {SnapclientEqFilterType::Peaking, 300.0f, -2.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 2500.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 1.0f, 0.707f, true}}},
    {"Electronic",
     "Electronic",
     -6.0f,
     {{SnapclientEqFilterType::LowShelf, 60.0f, 4.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 200.0f, -2.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, -1.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 4000.0f, 2.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 12000.0f, 3.0f, 0.707f, true}}},
    {"Dance",
     "Dance",
     -6.0f,
     {{SnapclientEqFilterType::LowShelf, 65.0f, 4.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 180.0f, -1.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 900.0f, -2.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 3000.0f, 1.5f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 11000.0f, 3.0f, 0.707f, true}}},
    {"Vocal",
     "Vocal",
     -4.0f,
     {{SnapclientEqFilterType::LowShelf, 100.0f, -2.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, -1.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 1200.0f, 2.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 3000.0f, 3.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 1.0f, 0.707f, true}}},
    {"Podcast",
     "Podcast / Speech",
     -4.0f,
     {{SnapclientEqFilterType::LowShelf, 120.0f, -4.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, -2.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 1200.0f, 2.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 3000.0f, 3.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 9000.0f, -1.0f, 0.707f, true}}},
    {"Jazz",
     "Jazz",
     -3.0f,
     {{SnapclientEqFilterType::LowShelf, 80.0f, 1.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, 1.0f, 0.85f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 3500.0f, 1.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 2.0f, 0.707f, true}}},
    {"Classical",
     "Classical",
     -3.0f,
     {{SnapclientEqFilterType::LowShelf, 70.0f, 1.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, 0.0f, 0.85f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 4000.0f, 1.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 12000.0f, 2.0f, 0.707f, true}}},
    {"Loudness",
     "Loudness",
     -6.0f,
     {{SnapclientEqFilterType::LowShelf, 75.0f, 4.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, 0.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, -2.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 4000.0f, 1.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 3.0f, 0.707f, true}}},
    {"Treble_Boost",
     "Treble Boost",
     -5.0f,
     {{SnapclientEqFilterType::LowShelf, 80.0f, 0.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, -1.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 4000.0f, 2.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 12000.0f, 4.0f, 0.707f, true}}},
    {"Small_Speaker_Safe",
     "Small Speaker Safe",
     -3.0f,
     {{SnapclientEqFilterType::LowShelf, 90.0f, -3.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 180.0f, 1.0f, 0.85f, true},
      {SnapclientEqFilterType::Peaking, 800.0f, 0.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 3500.0f, 1.0f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, 2.0f, 0.707f, true}}},
    {"Late_Night",
     "Late Night",
     -4.0f,
     {{SnapclientEqFilterType::LowShelf, 80.0f, -2.0f, 0.707f, true},
      {SnapclientEqFilterType::Peaking, 250.0f, -1.0f, 0.9f, true},
      {SnapclientEqFilterType::Peaking, 1000.0f, 1.0f, 1.0f, true},
      {SnapclientEqFilterType::Peaking, 3000.0f, 1.5f, 0.9f, true},
      {SnapclientEqFilterType::HighShelf, 10000.0f, -1.0f, 0.707f, true}}},
};

inline const SnapclientEqPreset &snapclientEqPreset(uint8_t presetIndex) {
  if (presetIndex >= SNAPCLIENT_EQ_PRESET_COUNT) {
    return SNAPCLIENT_EQ_PRESETS[0];
  }
  return SNAPCLIENT_EQ_PRESETS[presetIndex];
}

struct SnapclientDspConfig {
  bool enabled;
  uint8_t eqPresetIndex;
  float bassBoostDb;
  float leftGainDb;
  float rightGainDb;
  float balance;
  bool loudnessEnabled;
  float loudnessBassMaxDb;
  float loudnessFullBoostVolume;
  float loudnessFlatVolume;
  float headroomDb;
  bool softLimiterEnabled;
  float softLimiterCeiling;
};

inline float clampDspValue(float value, float minimum, float maximum) {
  if (!isfinite(value)) {
    return minimum;
  }
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

inline void sanitizeSnapclientDspConfig(SnapclientDspConfig &config,
                                        const SnapclientDspConfig &defaults) {
  config.loudnessFullBoostVolume = defaults.loudnessFullBoostVolume;
  config.loudnessFlatVolume = defaults.loudnessFlatVolume;

  if (config.eqPresetIndex >= SNAPCLIENT_EQ_PRESET_COUNT) {
    config.eqPresetIndex = defaults.eqPresetIndex;
  }
  config.bassBoostDb =
      isfinite(config.bassBoostDb)
          ? clampDspValue(config.bassBoostDb, 0.0f, 6.0f)
          : defaults.bassBoostDb;
  config.leftGainDb =
      isfinite(config.leftGainDb)
          ? clampDspValue(config.leftGainDb, -12.0f, 12.0f)
          : defaults.leftGainDb;
  config.rightGainDb =
      isfinite(config.rightGainDb)
          ? clampDspValue(config.rightGainDb, -12.0f, 12.0f)
          : defaults.rightGainDb;
  config.balance =
      isfinite(config.balance) ? clampDspValue(config.balance, -1.0f, 1.0f)
                               : defaults.balance;
  config.loudnessBassMaxDb =
      isfinite(config.loudnessBassMaxDb)
          ? clampDspValue(config.loudnessBassMaxDb, 0.0f, 9.0f)
          : defaults.loudnessBassMaxDb;
  config.headroomDb =
      isfinite(config.headroomDb)
          ? clampDspValue(config.headroomDb, -12.0f, 0.0f)
          : defaults.headroomDb;
  config.softLimiterCeiling =
      isfinite(config.softLimiterCeiling)
          ? clampDspValue(config.softLimiterCeiling, 0.50f, 1.0f)
          : defaults.softLimiterCeiling;
}

class SnapclientDsp {
 public:
  void configure(const SnapclientDspConfig &config) {
    config_ = config;
    sanitizeSnapclientDspConfig(config_, safeDefaults());
    loudnessBassDb_ = calculateLoudnessBassDb(lastVolume_);
    configureFilters();
  }

  void setAudioInfo(uint32_t sampleRate, uint8_t bitsPerSample, uint8_t channels) {
    sampleRate_ = sampleRate;
    bitsPerSample_ = bitsPerSample;
    channels_ = channels;
    resetState();
    configureFilters();
  }

  bool canProcess() const {
    return config_.enabled && sampleRate_ > 0 && bitsPerSample_ == 16 &&
           channels_ == 2 && hasActiveProcessing();
  }

  void setVolume(float volume) {
    lastVolume_ = isfinite(volume) ? clamp(volume, 0.0f, 1.0f) : 1.0f;
    const float nextLoudnessBassDb = calculateLoudnessBassDb(lastVolume_);
    if (fabsf(nextLoudnessBassDb - loudnessBassDb_) >= 0.05f) {
      loudnessBassDb_ = nextLoudnessBassDb;
      configureFilters();
    }
  }

  void processStereo16(uint8_t *buffer, size_t size) {
    if (!canProcess()) {
      return;
    }

    int16_t *samples = reinterpret_cast<int16_t *>(buffer);
    const size_t frameCount = size / (sizeof(int16_t) * 2);

    const float leftGain = leftGainLinear();
    const float rightGain = rightGainLinear();
    const float limiterCeiling =
        clamp(config_.softLimiterCeiling, 0.50f, 1.0f) * 32767.0f;

    for (size_t frame = 0; frame < frameCount; ++frame) {
      float left = static_cast<float>(samples[frame * 2]) * leftGain;
      float right = static_cast<float>(samples[(frame * 2) + 1]) * rightGain;

      for (uint8_t band = 0; band < SNAPCLIENT_EQ_BAND_COUNT; ++band) {
        left = filters_[0][band].process(left);
        right = filters_[1][band].process(right);
      }

      if (config_.softLimiterEnabled) {
        left = limitSample(left, limiterCeiling);
        right = limitSample(right, limiterCeiling);
      }

      samples[frame * 2] = toInt16(left);
      samples[(frame * 2) + 1] = toInt16(right);
    }
  }

  float loudnessBassDb() const { return loudnessBassDb_; }
  bool isBypassed() const { return !canProcess(); }

 private:
  static constexpr float kPi = 3.14159265358979323846f;

  struct Biquad {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;
    bool active = false;

    float process(float input) {
      if (!active) {
        return input;
      }

      const float output = (b0 * input) + z1;
      z1 = (b1 * input) - (a1 * output) + z2;
      z2 = (b2 * input) - (a2 * output);
      return output;
    }

    void setBypass() {
      b0 = 1.0f;
      b1 = 0.0f;
      b2 = 0.0f;
      a1 = 0.0f;
      a2 = 0.0f;
      active = false;
    }

    void setCoefficients(float rawB0,
                         float rawB1,
                         float rawB2,
                         float rawA0,
                         float rawA1,
                         float rawA2) {
      if (rawA0 == 0.0f || !isfinite(rawA0) || !isfinite(rawB0) ||
          !isfinite(rawB1) || !isfinite(rawB2) || !isfinite(rawA1) ||
          !isfinite(rawA2)) {
        setBypass();
        return;
      }

      const float nextB0 = rawB0 / rawA0;
      const float nextB1 = rawB1 / rawA0;
      const float nextB2 = rawB2 / rawA0;
      const float nextA1 = rawA1 / rawA0;
      const float nextA2 = rawA2 / rawA0;
      if (!isfinite(nextB0) || !isfinite(nextB1) || !isfinite(nextB2) ||
          !isfinite(nextA1) || !isfinite(nextA2)) {
        setBypass();
        return;
      }

      b0 = nextB0;
      b1 = nextB1;
      b2 = nextB2;
      a1 = nextA1;
      a2 = nextA2;
      active = true;
    }

    void reset() {
      z1 = 0.0f;
      z2 = 0.0f;
    }
  };

  SnapclientDspConfig config_ = {
      false, 0, 0.0f, 0.0f, 0.0f, 0.0f, true, 3.0f, 0.30f, 0.80f,
      0.0f,  true, 0.98f};
  Biquad filters_[2][SNAPCLIENT_EQ_BAND_COUNT];
  uint32_t sampleRate_ = 0;
  uint8_t bitsPerSample_ = 0;
  uint8_t channels_ = 0;
  float loudnessBassDb_ = 0.0f;
  float lastVolume_ = 1.0f;

  static const SnapclientDspConfig &safeDefaults() {
    static const SnapclientDspConfig defaults = {
        false, 0, 0.0f, 0.0f, 0.0f, 0.0f, true, 3.0f, 0.30f, 0.80f,
        0.0f,  true, 0.98f};
    return defaults;
  }

  static float clamp(float value, float minimum, float maximum) {
    if (!isfinite(value)) {
      return minimum;
    }
    if (value < minimum) {
      return minimum;
    }
    if (value > maximum) {
      return maximum;
    }
    return value;
  }

  static float dbToLinear(float db) {
    if (!isfinite(db)) {
      return 1.0f;
    }
    return powf(10.0f, db / 20.0f);
  }

  float calculateLoudnessBassDb(float volume) const {
    if (!config_.loudnessEnabled || config_.loudnessBassMaxDb <= 0.0f) {
      return 0.0f;
    }

    const float fullBoost = clamp(config_.loudnessFullBoostVolume, 0.0f, 1.0f);
    const float flat = clamp(config_.loudnessFlatVolume, fullBoost + 0.01f, 1.0f);
    if (volume <= fullBoost) {
      return config_.loudnessBassMaxDb;
    }
    if (volume >= flat) {
      return 0.0f;
    }

    const float fade = 1.0f - ((volume - fullBoost) / (flat - fullBoost));
    return config_.loudnessBassMaxDb * fade;
  }

  float leftGainLinear() const {
    const float balance = clamp(config_.balance, -1.0f, 1.0f);
    const float balanceGain = balance > 0.0f ? 1.0f - balance : 1.0f;
    const SnapclientEqPreset &preset = snapclientEqPreset(config_.eqPresetIndex);
    return dbToLinear(config_.headroomDb + preset.preampDb + config_.leftGainDb) *
           balanceGain;
  }

  float rightGainLinear() const {
    const float balance = clamp(config_.balance, -1.0f, 1.0f);
    const float balanceGain = balance < 0.0f ? 1.0f + balance : 1.0f;
    const SnapclientEqPreset &preset = snapclientEqPreset(config_.eqPresetIndex);
    return dbToLinear(config_.headroomDb + preset.preampDb + config_.rightGainDb) *
           balanceGain;
  }

  bool hasActiveProcessing() const {
    if (!config_.enabled) {
      return false;
    }

    const SnapclientEqPreset &preset = snapclientEqPreset(config_.eqPresetIndex);
    if (fabsf(config_.headroomDb) >= 0.01f ||
        fabsf(config_.leftGainDb) >= 0.01f ||
        fabsf(config_.rightGainDb) >= 0.01f ||
        fabsf(config_.balance) >= 0.001f ||
        fabsf(preset.preampDb) >= 0.01f ||
        fabsf(config_.bassBoostDb) >= 0.01f ||
        fabsf(loudnessBassDb_) >= 0.01f ||
        config_.softLimiterEnabled) {
      return true;
    }

    for (uint8_t bandIndex = 0; bandIndex < SNAPCLIENT_EQ_BAND_COUNT;
         ++bandIndex) {
      const SnapclientEqBand &band = preset.bands[bandIndex];
      if (band.enabled && fabsf(band.gainDb) >= 0.01f) {
        return true;
      }
    }

    return false;
  }

  void resetState() {
    for (uint8_t channel = 0; channel < 2; ++channel) {
      for (uint8_t band = 0; band < SNAPCLIENT_EQ_BAND_COUNT; ++band) {
        filters_[channel][band].reset();
      }
    }
  }

  void configureFilters() {
    const SnapclientEqPreset &preset = snapclientEqPreset(config_.eqPresetIndex);
    for (uint8_t channel = 0; channel < 2; ++channel) {
      for (uint8_t bandIndex = 0; bandIndex < SNAPCLIENT_EQ_BAND_COUNT;
           ++bandIndex) {
        const SnapclientEqBand &band = preset.bands[bandIndex];
        Biquad &filter = filters_[channel][bandIndex];
        if (!band.enabled) {
          filter.setBypass();
          continue;
        }

        float gainDb = band.gainDb;
        if (band.type == SnapclientEqFilterType::LowShelf) {
          gainDb += config_.bassBoostDb + loudnessBassDb_;
        }

        switch (band.type) {
          case SnapclientEqFilterType::LowShelf:
            configureLowShelf(filter, band.frequencyHz, band.q, gainDb);
            break;
          case SnapclientEqFilterType::Peaking:
            configurePeaking(filter, band.frequencyHz, band.q, gainDb);
            break;
          case SnapclientEqFilterType::HighShelf:
            configureHighShelf(filter, band.frequencyHz, band.q, gainDb);
            break;
        }
      }
    }
  }

  bool validFilter(float frequencyHz, float gainDb) const {
    return sampleRate_ > 0 && isfinite(frequencyHz) && isfinite(gainDb) &&
           frequencyHz > 0.0f &&
           frequencyHz < (static_cast<float>(sampleRate_) * 0.45f) &&
           fabsf(gainDb) >= 0.01f;
  }

  void configureLowShelf(Biquad &filter,
                         float frequencyHz,
                         float q,
                         float gainDb) const {
    if (!validFilter(frequencyHz, gainDb) || q <= 0.0f) {
      filter.setBypass();
      return;
    }

    const float a = powf(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * kPi * frequencyHz / static_cast<float>(sampleRate_);
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    const float sqrtA = sqrtf(a);
    const float alpha = sine / (2.0f * q);

    filter.setCoefficients(
        a * ((a + 1.0f) - ((a - 1.0f) * cosine) + (2.0f * sqrtA * alpha)),
        2.0f * a * ((a - 1.0f) - ((a + 1.0f) * cosine)),
        a * ((a + 1.0f) - ((a - 1.0f) * cosine) - (2.0f * sqrtA * alpha)),
        (a + 1.0f) + ((a - 1.0f) * cosine) + (2.0f * sqrtA * alpha),
        -2.0f * ((a - 1.0f) + ((a + 1.0f) * cosine)),
        (a + 1.0f) + ((a - 1.0f) * cosine) - (2.0f * sqrtA * alpha));
  }

  void configurePeaking(Biquad &filter,
                        float frequencyHz,
                        float q,
                        float gainDb) const {
    if (!validFilter(frequencyHz, gainDb) || q <= 0.0f) {
      filter.setBypass();
      return;
    }

    const float a = powf(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * kPi * frequencyHz / static_cast<float>(sampleRate_);
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    const float alpha = sine / (2.0f * q);

    filter.setCoefficients(1.0f + (alpha * a),
                           -2.0f * cosine,
                           1.0f - (alpha * a),
                           1.0f + (alpha / a),
                           -2.0f * cosine,
                           1.0f - (alpha / a));
  }

  void configureHighShelf(Biquad &filter,
                          float frequencyHz,
                          float q,
                          float gainDb) const {
    if (!validFilter(frequencyHz, gainDb) || q <= 0.0f) {
      filter.setBypass();
      return;
    }

    const float a = powf(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * kPi * frequencyHz / static_cast<float>(sampleRate_);
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    const float sqrtA = sqrtf(a);
    const float alpha = sine / (2.0f * q);

    filter.setCoefficients(
        a * ((a + 1.0f) + ((a - 1.0f) * cosine) + (2.0f * sqrtA * alpha)),
        -2.0f * a * ((a - 1.0f) + ((a + 1.0f) * cosine)),
        a * ((a + 1.0f) + ((a - 1.0f) * cosine) - (2.0f * sqrtA * alpha)),
        (a + 1.0f) - ((a - 1.0f) * cosine) + (2.0f * sqrtA * alpha),
        2.0f * ((a - 1.0f) - ((a + 1.0f) * cosine)),
        (a + 1.0f) - ((a - 1.0f) * cosine) - (2.0f * sqrtA * alpha));
  }

  static float limitSample(float sample, float ceiling) {
    if (!isfinite(sample)) {
      return 0.0f;
    }
    if (!isfinite(ceiling) || ceiling <= 0.0f) {
      ceiling = 32767.0f;
    }
    const float magnitude = fabsf(sample);
    const float kneeStart = ceiling * 0.85f;
    if (magnitude <= kneeStart) {
      return sample;
    }

    const float sign = sample < 0.0f ? -1.0f : 1.0f;
    if (magnitude >= ceiling) {
      return sign * ceiling;
    }

    const float range = ceiling - kneeStart;
    const float t = (magnitude - kneeStart) / range;
    const float limited = kneeStart + (range * (1.0f - ((1.0f - t) * (1.0f - t))));
    return sign * limited;
  }

  static int16_t toInt16(float sample) {
    if (sample > 32767.0f) {
      return 32767;
    }
    if (sample < -32768.0f) {
      return -32768;
    }
    return static_cast<int16_t>(sample);
  }
};

}  // namespace app_config
