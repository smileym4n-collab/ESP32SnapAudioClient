#include "audio_dsp.h"
#include <cmath>
#include <cstring>

namespace audio_dsp {
namespace {
constexpr double kPi = 3.14159265358979323846;
inline float clamp(float x, float lo, float hi) {
  return x < lo ? lo : x > hi ? hi : x;
}
bool inRange(float x, float lo, float hi) {
  return std::isfinite(x) && x >= lo && x <= hi;
}
Biquad normalized(double b0, double b1, double b2, double a0, double a1, double a2) {
  Biquad result;
  result.b0 = float(b0/a0); result.b1 = float(b1/a0); result.b2 = float(b2/a0);
  result.a1 = float(a1/a0); result.a2 = float(a2/a0);
  return result;
}
bool stable(const Biquad &c) {
  return std::isfinite(c.b0) && std::isfinite(c.b1) && std::isfinite(c.b2) &&
      std::isfinite(c.a1) && std::isfinite(c.a2) && fabsf(c.a2) < 1 &&
      1 + c.a1 + c.a2 > 0 && 1 - c.a1 + c.a2 > 0;
}
Biquad crossover(float hz, unsigned rate, bool high) {
  const double w = 2*kPi*hz/rate, c = cos(w), a = sin(w)/sqrt(2.0);
  const double b = high ? (1+c)/2 : (1-c)/2;
  return normalized(b, high ? -2*b : 2*b, b, 1+a, -2*c, 1-a);
}
Biquad equalizer(const Peq &p, unsigned rate) {
  if (!p.enabled) return {};
  const double w = 2*kPi*p.frequency/rate, c = cos(w);
  const double a = sin(w)/(2*p.q), A = pow(10.0, p.gainDb/40.0);
  switch (p.type) {
    case FilterType::Peak:
      return normalized(1+a*A, -2*c, 1-a*A, 1+a/A, -2*c, 1-a/A);
    case FilterType::Notch:
      return normalized(1, -2*c, 1, 1+a, -2*c, 1-a);
    // RBJ shelves using Q (not slope S); beta = 2*sqrt(A)*alpha.
    case FilterType::LowShelf: {
      const double b = 2*sqrt(A)*a;
      return normalized(A*((A+1)-(A-1)*c+b), 2*A*((A-1)-(A+1)*c),
          A*((A+1)-(A-1)*c-b), (A+1)+(A-1)*c+b,
          -2*((A-1)+(A+1)*c), (A+1)+(A-1)*c-b);
    }
    case FilterType::HighShelf: {
      const double b = 2*sqrt(A)*a;
      return normalized(A*((A+1)+(A-1)*c+b), -2*A*((A-1)+(A+1)*c),
          A*((A+1)+(A-1)*c-b), (A+1)-(A-1)*c+b,
          2*((A-1)-(A+1)*c), (A+1)-(A-1)*c-b);
    }
  }
  return {};
}
}
float dbGain(float db) { return powf(10, db/20); }
bool validate(const Config &c, unsigned rate, const char *&error) {
  error = nullptr;
  if (rate < 32000 || rate > kMaxRate) error = "sample rate must be 32000..96000 Hz";
  else if (c.schema != kSchema) error = "incompatible DSP schema";
  else if (!inRange(c.crossoverHz, 100, rate*0.45f)) error = "crossover must be 100..0.45*Fs Hz";
  else if (!inRange(c.masterDb, -80, 0)) error = "master must be -80..0 dB";
  else if (c.highRight > 1) error = "invalid output map";
  if (error) return false;
  for (const auto &o : c.output) {
    if (!inRange(o.gainDb, -24, 6)) error = "trim must be -24..+6 dB";
    else if (!inRange(o.delayMs, 0, 10)) error = "delay must be 0..10 ms";
    else if (o.inverted > 1 || o.muted > 1) error = "invalid polarity/mute";
    if (error) return false;
    for (const auto &p : o.peq) {
      if (unsigned(p.type) > unsigned(FilterType::Notch) || p.enabled > 1)
        error = "invalid PEQ type/state";
      else if (!inRange(p.frequency, 20, rate*0.45f)) error = "PEQ frequency must be 20..0.45*Fs Hz (including bypassed slots)";
      else if (!inRange(p.q, 0.1f, 20)) error = "Q must be 0.1..20";
      else if (!inRange(p.gainDb, -24, 12)) error = "PEQ gain must be -24..+12 dB";
      if (error) return false;
    }
  }
  return true;
}
bool prepare(const Config &c, unsigned rate, float toneHz, float toneDb,
             Prepared &out, const char *&error) {
  if (!validate(c, rate, error)) return false;
  if (toneHz != 0 && (!inRange(toneHz, 20, rate*0.45f) || !inRange(toneDb, -80, -12))) {
    error = "tone requires 20..0.45*Fs Hz and -80..-12 dBFS";
    return false;
  }
  out = {};
  out.rate = rate;
  out.highRight = c.highRight;
  out.toneStep = toneHz*1024/rate;
  out.toneGain = toneHz == 0 ? 0 : dbGain(toneDb);
  for (unsigned ch = 0; ch < 2; ++ch) {
    const auto &o = c.output[ch];
    out.filters[ch][0] = out.filters[ch][1] = crossover(c.crossoverHz, rate, ch == 0);
    for (unsigned i = 0; i < kPeqSlots; ++i) {
      if (o.peq[i].enabled) out.filters[ch][out.filterCount[ch]++] = equalizer(o.peq[i], rate);
    }
    for (const auto &b : out.filters[ch]) if (!stable(b)) {
      error = "unstable/non-finite coefficients";
      return false;
    }
    out.gains[ch] = o.muted ? 0 : dbGain(o.gainDb)*(o.inverted ? -1 : 1);
    out.delays[ch] = unsigned(lroundf(o.delayMs*rate/1000));
  }
  return true;
}
bool Engine::begin(Workspace *workspace) {
  workspace_ = workspace;
  if (!workspace_) return false;
  memset(workspace_->delay, 0, sizeof(workspace_->delay));
  for (unsigned i = 0; i <= 1024; ++i) workspace_->sine[i] = sinf(float(2*kPi*i/1024));
  return true;
}
bool Engine::publish(const Prepared &p) {
  if (!workspace_ || pending_.load(std::memory_order_acquire)) return false;
  mailbox_ = p;
  pending_.store(true, std::memory_order_release);
  return true;
}
float Engine::filter(float x, const Biquad &c, State &s) {
  const float y = c.b0*x + s.z1;
  s.z1 = c.b1*x - c.a1*y + s.z2;
  s.z2 = c.b2*x - c.a2*y;
  return y;
}
void Engine::process(const int16_t *stereo, int16_t *output, size_t frames, unsigned rate) {
  if (!workspace_) {
    memset(output, 0, frames*4);
    return;
  }
  auto &delay = workspace_->delay;
  auto &sine = workspace_->sine;
  bool changing = pending_.load(std::memory_order_acquire);
  if (changing && envelope_ == 0) {
    active_ = mailbox_;
    pending_.store(false, std::memory_order_release);
    for (auto &channel : state_) for (auto &s : channel) s = {};
    memset(delay, 0, sizeof(delay));
    position_ = 0;
    phase_ = 0;
    appliedRate_.store(active_.rate, std::memory_order_relaxed);
    changing = false;
  }
  // Format transitions and control-bus failures never fall back to broadband.
  if (!allowed_.load(std::memory_order_acquire) || rate != active_.rate || !rate) {
    memset(output, 0, frames*4);
    envelope_ = 0;
    return;
  }
  const float step = 1.0f/(rate*0.010f);
  const float masterStep = 1.0f/(rate*0.020f);
  const float target = masterTarget_.load(std::memory_order_relaxed);
  uint32_t clips = 0;
  for (size_t i = 0; i < frames; ++i) {
    envelope_ = clamp(envelope_ + (changing ? -step : step), 0, 1);
    master_ += clamp(target-master_, -masterStep, masterStep);
    // Duplicated selected Snapcast channel averages to itself. Bluetooth and
    // Snapcast Stereo explicitly average, avoiding the +6 dB of unscaled L+R.
    float mono = (float(stereo[i*2])*0.5f + float(stereo[i*2+1])*0.5f)/32768;
    if (active_.toneStep > 0) {
      const unsigned index = unsigned(phase_);
      mono = (sine[index] + (sine[index+1]-sine[index])*(phase_-index))*active_.toneGain;
      phase_ += active_.toneStep;
      if (phase_ >= 1024) phase_ -= 1024;
    }
    mono *= master_;
    float semantic[2];
    for (unsigned ch = 0; ch < 2; ++ch) {
      float x = mono;
      for (unsigned b = 0; b < active_.filterCount[ch]; ++b) x = filter(x, active_.filters[ch][b], state_[ch][b]);
      x *= active_.gains[ch];
      // Bound pathological internal states rather than converting NaN to PCM.
      if (!std::isfinite(x)) x = 0;
      // Default zero-delay playback never touches external sample storage.
      // Enabling a delay resets its history during the muted bank exchange.
      if (active_.delays[ch] != 0) {
        delay[ch][position_] = x;
        x = delay[ch][(position_+kDelaySamples-active_.delays[ch])%kDelaySamples];
      }
      semantic[ch] = x*envelope_;
    }
    if (++position_ == kDelaySamples) position_ = 0;
    for (unsigned lr = 0; lr < 2; ++lr) {
      float x = semantic[active_.highRight ? 1-lr : lr];
      if (x > 32767.0f/32768 || x < -1) ++clips;
      x = clamp(x, -1, 32767.0f/32768);
      output[i*2+lr] = int16_t(x*32768);
    }
  }
  if (clips) clips_.fetch_add(clips, std::memory_order_relaxed);
}
}  // namespace audio_dsp
