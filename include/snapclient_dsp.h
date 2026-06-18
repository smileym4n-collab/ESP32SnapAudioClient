#pragma once

#include <math.h>
#include <stdint.h>

#include <Arduino.h>

namespace app_config {

struct SnapclientDspConfig {
  bool enabled;
  float lowShelfHz;
  float lowShelfDb;
  float midHz;
  float midQ;
  float midDb;
  float highShelfHz;
  float highShelfDb;
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

class SnapclientDsp {
 public:
  void configure(const SnapclientDspConfig &config) {
    config_ = config;
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
           channels_ == 2;
  }

  void setVolume(float volume) {
    lastVolume_ = clamp(volume, 0.0f, 1.0f);
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

      for (uint8_t band = 0; band < kBandCount; ++band) {
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

 private:
  enum Band : uint8_t { kLowShelf = 0, kMidPeak = 1, kHighShelf = 2 };
  static constexpr uint8_t kBandCount = 3;
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
      if (rawA0 == 0.0f) {
        setBypass();
        return;
      }

      b0 = rawB0 / rawA0;
      b1 = rawB1 / rawA0;
      b2 = rawB2 / rawA0;
      a1 = rawA1 / rawA0;
      a2 = rawA2 / rawA0;
      active = true;
    }

    void reset() {
      z1 = 0.0f;
      z2 = 0.0f;
    }
  };

  SnapclientDspConfig config_ = {
      false, 120.0f, 0.0f, 1000.0f, 0.8f, 0.0f, 8000.0f, 0.0f,
      0.0f,  0.0f, 0.0f, true,    3.0f, 0.30f, 0.80f, 0.0f,
      true,  0.98f};
  Biquad filters_[2][kBandCount];
  uint32_t sampleRate_ = 0;
  uint8_t bitsPerSample_ = 0;
  uint8_t channels_ = 0;
  float loudnessBassDb_ = 0.0f;
  float lastVolume_ = 1.0f;

  static float clamp(float value, float minimum, float maximum) {
    if (value < minimum) {
      return minimum;
    }
    if (value > maximum) {
      return maximum;
    }
    return value;
  }

  static float dbToLinear(float db) {
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
    return dbToLinear(config_.headroomDb + config_.leftGainDb) * balanceGain;
  }

  float rightGainLinear() const {
    const float balance = clamp(config_.balance, -1.0f, 1.0f);
    const float balanceGain = balance < 0.0f ? 1.0f + balance : 1.0f;
    return dbToLinear(config_.headroomDb + config_.rightGainDb) * balanceGain;
  }

  void resetState() {
    for (uint8_t channel = 0; channel < 2; ++channel) {
      for (uint8_t band = 0; band < kBandCount; ++band) {
        filters_[channel][band].reset();
      }
    }
  }

  void configureFilters() {
    for (uint8_t channel = 0; channel < 2; ++channel) {
      configureLowShelf(filters_[channel][kLowShelf],
                        config_.lowShelfHz,
                        config_.lowShelfDb + loudnessBassDb_);
      configurePeaking(filters_[channel][kMidPeak],
                       config_.midHz,
                       config_.midQ,
                       config_.midDb);
      configureHighShelf(filters_[channel][kHighShelf],
                         config_.highShelfHz,
                         config_.highShelfDb);
    }
  }

  bool validFilter(float frequencyHz, float gainDb) const {
    return sampleRate_ > 0 && frequencyHz > 0.0f &&
           frequencyHz < (static_cast<float>(sampleRate_) * 0.45f) &&
           fabsf(gainDb) >= 0.01f;
  }

  void configureLowShelf(Biquad &filter, float frequencyHz, float gainDb) const {
    if (!validFilter(frequencyHz, gainDb)) {
      filter.setBypass();
      return;
    }

    const float a = powf(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * kPi * frequencyHz / static_cast<float>(sampleRate_);
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    const float sqrtA = sqrtf(a);
    const float alpha = sine / 2.0f * sqrtf(2.0f);

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

  void configureHighShelf(Biquad &filter, float frequencyHz, float gainDb) const {
    if (!validFilter(frequencyHz, gainDb)) {
      filter.setBypass();
      return;
    }

    const float a = powf(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * kPi * frequencyHz / static_cast<float>(sampleRate_);
    const float sine = sinf(omega);
    const float cosine = cosf(omega);
    const float sqrtA = sqrtf(a);
    const float alpha = sine / 2.0f * sqrtf(2.0f);

    filter.setCoefficients(
        a * ((a + 1.0f) + ((a - 1.0f) * cosine) + (2.0f * sqrtA * alpha)),
        -2.0f * a * ((a - 1.0f) + ((a + 1.0f) * cosine)),
        a * ((a + 1.0f) + ((a - 1.0f) * cosine) - (2.0f * sqrtA * alpha)),
        (a + 1.0f) - ((a - 1.0f) * cosine) + (2.0f * sqrtA * alpha),
        2.0f * ((a - 1.0f) - ((a + 1.0f) * cosine)),
        (a + 1.0f) - ((a - 1.0f) * cosine) - (2.0f * sqrtA * alpha));
  }

  static float limitSample(float sample, float ceiling) {
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
