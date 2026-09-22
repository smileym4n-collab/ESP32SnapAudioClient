#pragma once

#include <atomic>
#include <stddef.h>
#include <stdint.h>

namespace audio_dsp {
constexpr unsigned kSchema = 1;
constexpr unsigned kPeqSlots = 6;
constexpr unsigned kMaxRate = 96000;
constexpr unsigned kDelaySamples = 961;  // 10 ms at maximum supported rate

enum class FilterType : uint8_t { Peak, LowShelf, HighShelf, Notch };
struct Peq {
  float frequency = 1000, q = 0.70710678f, gainDb = 0;
  uint8_t enabled = 0;
  FilterType type = FilterType::Peak;
  uint8_t reserved[2] = {};
};
struct OutputConfig {
  float gainDb = 0, delayMs = 0;
  uint8_t inverted = 0, muted = 0;
  uint8_t reserved[2] = {};
  Peq peq[kPeqSlots];
};
struct Config {
  uint32_t schema = kSchema;
  float crossoverHz = 2400, masterDb = 0;
  uint8_t highRight = 0;
  uint8_t reserved[3] = {};
  OutputConfig output[2];  // HIGH, LOW; never transport L/R
};
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};
struct Prepared {
  Biquad filters[2][kPeqSlots + 2];
  unsigned filterCount[2] = {2, 2};
  float gains[2] = {1, 1};
  unsigned delays[2] = {};
  unsigned rate = 0;
  float toneStep = 0, toneGain = 0;
  bool highRight = false;
};

bool validate(const Config &config, unsigned rate, const char *&error);
bool prepare(const Config &config, unsigned rate, float toneHz, float toneDb,
             Prepared &out, const char *&error);
float dbGain(float db);

// Large sample storage is allocated once in PSRAM before sources start.
// Keep coefficients, filter states and atomics in internal RAM.
struct Workspace {
  float delay[2][kDelaySamples] = {};
  float sine[1025] = {};
};

// Single producer (main loop) / single consumer (existing audio writer).
// Mailbox ownership is transferred with acquire/release; the writer never waits.
class Engine {
 public:
  bool begin(Workspace *workspace);  // before starting either source task
  bool publish(const Prepared &prepared);
  bool pending() const { return pending_.load(std::memory_order_acquire); }
  void setMaster(float gain) { masterTarget_.store(gain, std::memory_order_relaxed); }
  void setAllowed(bool value) { allowed_.store(value, std::memory_order_release); }
  void process(const int16_t *stereo, int16_t *output, size_t frames, unsigned rate);
  uint32_t clipped() const { return clips_.load(std::memory_order_relaxed); }
  unsigned appliedRate() const { return appliedRate_.load(std::memory_order_relaxed); }
 private:
  struct State { float z1 = 0, z2 = 0; };
  Prepared mailbox_, active_;
  std::atomic<bool> pending_{false}, allowed_{false};
  std::atomic<float> masterTarget_{1};
  std::atomic<uint32_t> clips_{0}, appliedRate_{0};
  State state_[2][kPeqSlots + 2];
  Workspace *workspace_ = nullptr;
  unsigned position_ = 0;
  float envelope_ = 0, master_ = 0, phase_ = 0;
  float filter(float value, const Biquad &c, State &s);
};
}  // namespace audio_dsp
