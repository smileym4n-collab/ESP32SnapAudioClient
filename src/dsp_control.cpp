#include "dsp_control.h"

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#ifdef ESP32
#include <esp_heap_caps.h>
#endif
#include "board_config.h"

namespace dsp_control {
namespace {
using namespace audio_dsp;
Engine engine;
Workspace *workspace = nullptr;
bool workspaceOkay = false;
Config config;
std::atomic<unsigned> requestedRate{48000}, route{0};
std::atomic<bool> validFormat{true}, started{false};
std::atomic<uint32_t> maxBlockUs{0};
unsigned compiledRate = 0;
bool bluetooth = false, dirty = true, dacConfigured = false, dacOkay = true, dacMuted = true;
bool preparedValid = false;
bool stopped = false, potEnabled = false, unsaved = false;
float toneHz = 0, toneDb = -30, potFiltered = 0, masterDb = 0;
int adcRaw = -1;
uint32_t lastPotMs = 0;
uint32_t lastPotReportMs = 0;
int lastPotReportRaw = -1;
const char *configState = "defaults";
const char *types[] = {"peak", "lowshelf", "highshelf", "notch"};

bool dacWrite(uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(board_config::PCM5122_I2C_ADDRESS);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission() == 0;
}
bool dacRead(uint8_t reg, uint8_t &value) {
  Wire1.beginTransmission(board_config::PCM5122_I2C_ADDRESS);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0 ||
      Wire1.requestFrom(board_config::PCM5122_I2C_ADDRESS,
                        static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = static_cast<uint8_t>(Wire1.read());
  return true;
}
const char *dacPowerStateName(uint8_t state) {
  switch (state & 0x0F) {
    case 0x0: return "powerdown";
    case 0x1: return "wait-charge-pump";
    case 0x2:
    case 0x3: return "calibration";
    case 0x4: return "volume-ramp-up";
    case 0x5: return "run";
    case 0x6: return "output-short";
    case 0x7: return "volume-ramp-down";
    case 0x8: return "standby";
    default: return "unknown";
  }
}
void dacStatus() {
  if (!dacConfigured) {
    Serial.println("[dsp-dac] I2C disabled");
    return;
  }

  uint8_t powerRequest = 0, pll = 0, pllReference = 0, dacClock = 0;
  uint8_t pllP = 0, pllJ = 0, pllDMsb = 0, pllDLsb = 0, pllR = 0;
  uint8_t dspDivider = 0, dacDivider = 0, cpDivider = 0, osrDivider = 0;
  uint8_t idacMsb = 0, idacLsb = 0, clockFlex = 0;
  uint8_t mute = 0, format = 0, volumeLeft = 0, volumeRight = 0;
  uint8_t rate = 0, bckRatioHigh = 0, bckRatioLow = 0;
  uint8_t clock = 0, clockFault = 0, analogMute = 0, shortStatus = 0;
  uint8_t xsmute = 0, power = 0;
  const bool readOkay = dacWrite(0, 0) &&
      dacRead(2, powerRequest) && dacRead(4, pll) &&
      dacRead(13, pllReference) && dacRead(14, dacClock) &&
      dacRead(20, pllP) && dacRead(21, pllJ) &&
      dacRead(22, pllDMsb) && dacRead(23, pllDLsb) && dacRead(24, pllR) &&
      dacRead(27, dspDivider) && dacRead(28, dacDivider) &&
      dacRead(29, cpDivider) && dacRead(30, osrDivider) &&
      dacRead(35, idacMsb) && dacRead(36, idacLsb) &&
      dacRead(37, clockFlex) && dacRead(3, mute) && dacRead(40, format) &&
      dacRead(61, volumeLeft) && dacRead(62, volumeRight) &&
      dacRead(91, rate) && dacRead(92, bckRatioHigh) &&
      dacRead(93, bckRatioLow) && dacRead(94, clock) &&
      dacRead(95, clockFault) && dacRead(108, analogMute) &&
      dacRead(109, shortStatus) && dacRead(114, xsmute) &&
      dacRead(118, power);
  if (!readOkay) {
    Serial.printf("[dsp-dac] readback=FAILED address=0x%02X SDA=%d SCL=%d\n",
                  board_config::PCM5122_I2C_ADDRESS,
                  board_config::PCM5122_I2C_SDA_PIN,
                  board_config::PCM5122_I2C_SCL_PIN);
    return;
  }

  const unsigned bckRatio =
      (static_cast<unsigned>(bckRatioHigh & 0x01) << 8) | bckRatioLow;
  Serial.printf(
      "[dsp-dac] readback=OK addr=0x%02X mute=0x%02X format=0x%02X volumeL=0x%02X volumeR=0x%02X\n",
      board_config::PCM5122_I2C_ADDRESS, mute, format, volumeLeft, volumeRight);
  Serial.printf(
      "[dsp-dac] clockConfig powerReq=0x%02X pll=0x%02X ref=0x%02X dacSrc=0x%02X P=%u J=%u D=%u R=%u DDSP=%u DDAC=%u DNCP=%u DOSR=%u IDAC=%u flex=0x%02X\n",
      powerRequest, pll, pllReference, dacClock,
      static_cast<unsigned>((pllP & 0x0F) + 1),
      static_cast<unsigned>(pllJ & 0x3F),
      static_cast<unsigned>((static_cast<unsigned>(pllDMsb & 0x3F) << 8) | pllDLsb),
      static_cast<unsigned>((pllR & 0x0F) + 1),
      static_cast<unsigned>((dspDivider & 0x7F) + 1),
      static_cast<unsigned>((dacDivider & 0x7F) + 1),
      static_cast<unsigned>((cpDivider & 0x7F) + 1),
      static_cast<unsigned>((osrDivider & 0x7F) + 1),
      static_cast<unsigned>((static_cast<unsigned>(idacMsb) << 8) | idacLsb),
      clockFlex);
  Serial.printf(
      "[dsp-dac] fsCode=%u sckRatioCode=%u bckRatio=%u clock=0x%02X clockFault=0x%02X analogMute=0x%02X short=0x%02X xsmute=0x%02X boot=%s power=0x%X(%s)\n",
      static_cast<unsigned>((rate >> 4) & 0x07),
      static_cast<unsigned>(rate & 0x0F), bckRatio, clock, clockFault,
      analogMute, shortStatus, xsmute,
      (power & 0x80) != 0 ? "done" : "busy",
      static_cast<unsigned>(power & 0x0F), dacPowerStateName(power));
}
void masterUpdate() {
  if (!potEnabled) masterDb = config.masterDb;
  engine.setMaster(masterDb <= -80 ? 0 : dbGain(masterDb));
}
void hardwareBegin() {
  const int sda = board_config::PCM5122_I2C_SDA_PIN;
  const int scl = board_config::PCM5122_I2C_SCL_PIN;
  dacConfigured = sda >= 0 || scl >= 0;
  if (dacConfigured) {
    // Reserve battery pins only when that hardware is enabled. This board has
    // no INA236 and reuses GPIO21 for the DAC on Wire1.
    const bool batteryConflict = board_config::BATTERY_MONITOR_ENABLED &&
        (sda == board_config::BATTERY_I2C_SDA_PIN || sda == board_config::BATTERY_I2C_SCL_PIN ||
         scl == board_config::BATTERY_I2C_SDA_PIN || scl == board_config::BATTERY_I2C_SCL_PIN);
    dacOkay = sda >= 0 && scl >= 0 && sda != scl && !batteryConflict;
    if (dacOkay) dacOkay = Wire1.begin(sda, scl, 100000);
    Wire1.setTimeOut(10);
    // TI PCM5122 SLAS763C section 8.3.6.3 requires software-mode 3-wire I2S
    // to program the PLL and all clock dividers. BCK is 32*fS for our 16-bit
    // stereo frames. Table 131's 44.1/48-kHz, ratio-32 settings share these
    // values: BCK * 32 * 2 = 2048*fS, DSP=1024*fS, DAC=128*fS,
    // OSR=16*fS and charge-pump clock=32*fS.
    if (dacOkay) dacOkay =
        dacWrite(0, 0) &&
        dacWrite(3, 0x11) &&       // soft mute L/R while clock tree changes
        dacWrite(2, 0x10) &&       // request standby
        dacWrite(4, 0x01) &&       // PLL enabled
        dacWrite(13, 0x10) &&      // PLL reference = BCK
        dacWrite(14, 0x10) &&      // DAC clock source = PLL
        dacWrite(20, 0x00) &&      // PLL P = 1
        dacWrite(21, 0x20) &&      // PLL J = 32
        dacWrite(22, 0x00) && dacWrite(23, 0x00) && // PLL D = 0000
        dacWrite(24, 0x01) &&      // PLL R = 2
        dacWrite(27, 0x01) &&      // DSP divider = 2
        dacWrite(28, 0x0F) &&      // DAC divider = 16
        dacWrite(29, 0x03) &&      // NCP divider = 4
        dacWrite(30, 0x07) &&      // OSR divider = 8
        dacWrite(34, 0x00) &&      // single-speed fS, 8x interpolation
        dacWrite(35, 0x04) && dacWrite(36, 0x00) && // IDAC = 1024
        dacWrite(37, 0x1A) &&      // ignore absent SCK, manual dividers
        dacWrite(40, 0x00) &&      // 16-bit I2S
        dacWrite(61, 48) && dacWrite(62, 48) && // fixed 0-dB volume
        dacWrite(19, 0x01) && dacWrite(19, 0x00) && // resync clock tree
        dacWrite(2, 0x00);         // normal operation when BCK/LRCK arrive
    if (!dacOkay) Serial.println("[dsp] PCM5122 setup failed; DSP output held at zero");
    else Serial.printf("[dsp] PCM5122 ACK address=0x%02X SDA=%d SCL=%d; 3-wire BCK-PLL, 16-bit I2S, 0 dB, soft mute requested\n",
                       board_config::PCM5122_I2C_ADDRESS, sda, scl);
  }
  const int pin = board_config::VOLUME_ADC_PIN;
  // ADC2 cannot be used reliably with Wi-Fi; ADC1 on classic ESP32 is 32..39.
  potEnabled = pin >= 32 && pin <= 39 &&
      pin != board_config::WIFI_STATUS_LED_PIN && pin != board_config::BT_STATUS_LED_PIN &&
      board_config::VOLUME_ADC_MAX > board_config::VOLUME_ADC_MIN;
  if (pin >= 0 && !potEnabled) {
    dacOkay = false;
    Serial.println("[dsp] invalid/conflicting volume ADC configuration; output held at zero");
  }
  if (potEnabled) {
    analogReadResolution(12);
    analogSetPinAttenuation(pin, ADC_11db);
    masterDb = -80;  // never start at full volume while awaiting first ADC read
  }
}
void status() {
  const unsigned rate = requestedRate.load();
  Serial.printf("[dsp] schema=%u source=%s routing=%s Fs=%u appliedFs=%u format=%s settings=%s unsaved=%s pending=%s\n",
      kSchema, bluetooth ? "Bluetooth" : "Snapcast", bluetooth ? "0.5L+0.5R" :
      (route.load() == 1 ? "LEFT -> mono" : route.load() == 2 ? "RIGHT -> mono" : "stereo -> 0.5L+0.5R"),
      rate, engine.appliedRate(), validFormat.load() ? "stereo16" : "unsupported: ZERO", configState,
      unsaved ? "yes" : "no", (dirty || engine.pending()) ? "yes" : "no");
  Serial.printf("[dsp] LR4 Fc=%.1f Hz HIGH=%s LOW=%s masterTarget=%.2f dB control=%s ADC=%d filtered=%.1f\n",
      config.crossoverHz, config.highRight ? "RIGHT" : "LEFT", config.highRight ? "LEFT" : "RIGHT",
      masterDb, potEnabled ? "pot" : "shell", adcRaw, potFiltered);
  Serial.printf("[dsp] I2C=%s DAC=%s coefficients=%s outputStarted=%s stop=%s tone=%.1f Hz %.1f dBFS (never saved) clips=%lu max128FrameBlock=%lu us engine=%u bytes\n",
      dacConfigured ? "configured" : "disabled", !dacOkay ? "FAULT/ZERO" : dacMuted ? "muted" : "released",
      preparedValid ? "valid" : "INVALID/ZERO", started.load() ? "yes" : "no", stopped ? "yes" : "no", toneHz, toneDb,
      (unsigned long)engine.clipped(), (unsigned long)maxBlockUs.load(), unsigned(sizeof(engine)));
  for (unsigned ch = 0; ch < 2; ++ch) {
    const auto &o = config.output[ch];
    Serial.printf("[dsp] %s gain=%.2f dB mute=%s polarity=%s delay=%.3f ms (%lu samples)\n",
        ch == 0 ? "HIGH" : "LOW", o.gainDb, o.muted ? "on" : "off", o.inverted ? "invert" : "normal",
        o.delayMs, (unsigned long)lroundf(o.delayMs*rate/1000));
    for (unsigned i = 0; i < kPeqSlots; ++i) {
      const auto &p = o.peq[i];
      Serial.printf("  peq %u %s %s %.1f Hz Q=%.3f gain=%.2f dB\n", i+1,
          p.enabled ? "on" : "bypass", types[unsigned(p.type)], p.frequency, p.q, p.gainDb);
    }
  }
}
void help() {
  Serial.println("dsp status | dac | help | crossover <Hz> | map high-left|high-right");
  Serial.println("dsp master <-80..0 dB> (only with ADC disabled; -80=mute)");
  Serial.println("dsp high|low gain <dB> | mute on|off | polarity normal|invert | delay <ms>");
  Serial.println("dsp high|low peq <1..6> peak|lowshelf|highshelf <Hz> <Q> <dB>");
  Serial.println("dsp high|low peq <1..6> notch <Hz> <Q>");
  Serial.println("dsp high|low peq <1..6> on|off|clear | peq clear");
  Serial.println("dsp tone <Hz> [dBFS=-30] | tone off | defaults | save");
}
bool number(const char *s, float &value) {
  if (!s || !*s) return false;
  char *end;
  value = strtof(s, &end);
  return *end == '\0' && std::isfinite(value);
}
}
void begin(bool isBluetooth) {
  bluetooth = isBluetooth;
  requestedRate.store(bluetooth ? 44100 : 48000);
  if (!workspace) {
#ifdef ESP32
    // Explicit external allocation: never consume the internal heap needed by
    // FreeRTOS stacks and I2S DMA, even if PSRAM is absent/exhausted.
    void *memory = heap_caps_malloc(sizeof(Workspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    void *memory = malloc(sizeof(Workspace));  // host tests
#endif
    if (memory) workspace = new (memory) Workspace{};
  }
  workspaceOkay = engine.begin(workspace);
  Serial.printf("[dsp] workspace=%s bytes=%u internal-engine=%u bytes\n",
      workspaceOkay ? "ready (PSRAM on ESP32)" : "FAILED/output ZERO",
      unsigned(sizeof(Workspace)), unsigned(sizeof(engine)));
  config = Config{};
  configState = "defaults (missing/invalid NVS)";
  Preferences prefs;
  Config stored;
  Prepared restored;
  const char *error = nullptr;
  if (prefs.begin("speaker-dsp", true)) {
    if (prefs.getBytesLength("config") == sizeof(Config) &&
        prefs.getBytes("config", &stored, sizeof(stored)) == sizeof(stored) &&
        prepare(stored, requestedRate.load(), 0, -30, restored, error)) {
      config = stored;
      configState = "NVS";
    } else configState = "defaults (missing/invalid NVS)";
    prefs.end();
  }
  // Tone lives outside Config and is always off at boot.
  toneHz = 0;
  hardwareBegin();
  masterUpdate();
  service();
}
void format(unsigned rate, unsigned channels, unsigned bits) {
  validFormat.store(channels == 2 && bits == 16 && rate >= 32000 && rate <= kMaxRate);
  requestedRate.store(rate);
}
void outputStarted(bool value) { started.store(value); }
void routing(unsigned mode) { route.store(mode); }
void process(const int16_t *input, int16_t *output, size_t frames) {
  const uint32_t start = micros();
  engine.process(input, output, frames, validFormat.load() ? requestedRate.load() : 0);
  const uint32_t duration = micros()-start;
  if (duration > maxBlockUs.load()) maxBlockUs.store(duration);
}
void stop() {
  stopped = true;
  engine.setAllowed(false);
  if (dacConfigured && dacOkay) dacOkay = dacWrite(0, 0) && dacWrite(3, 0x11);
  dacMuted = true;
}
void service() {
  if (!workspaceOkay) { engine.setAllowed(false); return; }
  const unsigned rate = requestedRate.load();
  if ((dirty || rate != compiledRate) && !engine.pending() && validFormat.load()) {
    Prepared prepared;
    const char *error = nullptr;
    if (prepare(config, rate, toneHz, toneDb, prepared, error)) {
      engine.publish(prepared);
      preparedValid = true;
      compiledRate = rate;
      dirty = false;
    } else {
      // A lower negotiated rate can invalidate a saved high-frequency filter.
      // Stay silent until corrected; never silently replace a tuned crossover.
      engine.setAllowed(false);
      preparedValid = false;
      if (compiledRate != rate || dirty) Serial.printf("[dsp] output ZERO: %s\n", error);
      compiledRate = rate;
      dirty = false;
      return;
    }
  }
  if (potEnabled && millis()-lastPotMs >= 20) {
    lastPotMs = millis();
    adcRaw = analogRead(board_config::VOLUME_ADC_PIN);
    potFiltered += 0.15f*(adcRaw-potFiltered);
    const float position = fmaxf(0, fminf(1, (potFiltered-board_config::VOLUME_ADC_MIN)/
        (board_config::VOLUME_ADC_MAX-board_config::VOLUME_ADC_MIN)));
    const float db = position <= 0.02f ? -80 : board_config::VOLUME_MIN_DB*(1-position)/0.98f;
    if (fabsf(db-masterDb) >= 0.5f || (db == -80 && masterDb != -80) || (position > 0.995f && masterDb != 0)) {
      masterDb = position > 0.995f ? 0 : db;
      masterUpdate();
    }
    // Bench-friendly live indication without flooding the UART/audio tasks.
    // Report the first sample, then at most four times per second while the
    // physical control is moving by a meaningful amount.
    const uint32_t now = millis();
    if (lastPotReportRaw < 0 ||
        (now-lastPotReportMs >= 250 && abs(adcRaw-lastPotReportRaw) >= 16)) {
      Serial.printf("[dsp-pot] GPIO%d raw=%d filtered=%.1f position=%.1f%% master=%.2f dB\n",
          board_config::VOLUME_ADC_PIN, adcRaw, potFiltered, position*100, masterDb);
      lastPotReportMs = now;
      lastPotReportRaw = adcRaw;
    }
  }
  const bool ready = preparedValid && !stopped && started.load() && validFormat.load() &&
      engine.appliedRate() == rate && compiledRate == rate && dacOkay;
  if (ready && dacMuted) {
    if (dacConfigured) dacOkay = dacWrite(0, 0) && dacWrite(3, 0);
    if (dacOkay) dacMuted = false;
    else Serial.println("[dsp] DAC unmute failed; output held at zero");
  }
  engine.setAllowed(ready && dacOkay);
}
void command(char *line) {
  char *tokens[12] = {};
  unsigned n = 0;
  char *context = nullptr;
  for (char *s = strtok_r(line, " \t", &context); s; s = strtok_r(nullptr, " \t", &context)) {
    if (n == 12) { Serial.println("[dsp] too many arguments"); return; }
    tokens[n++] = s;
  }
  auto eq = [&](unsigned i, const char *s) { return i < n && strcmp(tokens[i], s) == 0; };
  if (!eq(0, "dsp")) { Serial.println("[dsp] expected 'dsp'; use dsp help"); return; }
  if (n == 1 || (n == 2 && eq(1, "help"))) { help(); return; }
  if (n == 2 && eq(1, "status")) { status(); return; }
  if (n == 2 && eq(1, "dac")) { dacStatus(); return; }
  // Always allow the safety stop, even if a rate change invalidated the tuning.
  if (n == 3 && eq(1, "tone") && eq(2, "off")) {
    toneHz = 0;
    dirty = true;
    Serial.println("[dsp] tone off (runtime only)");
    return;
  }
  if (n == 2 && eq(1, "save")) {
    const char *error = nullptr;
    if (!validate(config, requestedRate.load(), error)) { Serial.printf("[dsp] %s\n", error); return; }
    Preferences prefs;
    bool saved = prefs.begin("speaker-dsp", false);
    if (saved) { saved = prefs.putBytes("config", &config, sizeof(config)) == sizeof(config); prefs.end(); }
    if (saved) { unsaved = false; configState = "NVS"; }
    Serial.println(saved ? "[dsp] saved complete config (tone excluded)" : "[dsp] NVS save FAILED");
    return;
  }
  Config next = config;
  float nextTone = toneHz, nextLevel = toneDb;
  bool okay = false, toneOnly = false, masterOnly = false;
  if (n == 2 && eq(1, "defaults")) { next = Config{}; nextTone = 0; okay = true; }
  else if (n == 3 && eq(1, "crossover")) okay = number(tokens[2], next.crossoverHz);
  else if (n == 3 && eq(1, "master")) {
    if (potEnabled) { Serial.println("[dsp] master is owned by volume pot"); return; }
    okay = number(tokens[2], next.masterDb); masterOnly = true;
  } else if (n == 3 && eq(1, "map")) {
    okay = eq(2, "high-left") || eq(2, "high-right"); next.highRight = eq(2, "high-right");
  } else if (eq(1, "tone")) {
    toneOnly = true;
    if (n == 3 || n == 4) {
      nextLevel = -30;
      okay = number(tokens[2], nextTone) && nextTone > 0 && (n == 3 || number(tokens[3], nextLevel));
    }
  } else if (eq(1, "high") || eq(1, "low")) {
    auto &o = next.output[eq(1, "high") ? 0 : 1];
    if (n == 4 && eq(2, "gain")) okay = number(tokens[3], o.gainDb);
    else if (n == 4 && eq(2, "delay")) okay = number(tokens[3], o.delayMs);
    else if (n == 4 && eq(2, "mute")) { okay = eq(3, "on") || eq(3, "off"); o.muted = eq(3, "on"); }
    else if (n == 4 && eq(2, "polarity")) { okay = eq(3, "normal") || eq(3, "invert"); o.inverted = eq(3, "invert"); }
    else if (eq(2, "peq")) {
      if (n == 4 && eq(3, "clear")) { for (auto &p : o.peq) p = Peq{}; okay = true; }
      else if (n >= 5) {
        float slot;
        if (!number(tokens[3], slot) || slot < 1 || slot > kPeqSlots || floorf(slot) != slot) {
          Serial.println("[dsp] PEQ slot must be integer 1..6"); return;
        }
        auto &p = o.peq[unsigned(slot)-1];
        if (n == 5 && eq(4, "clear")) { p = Peq{}; okay = true; }
        else if (n == 5 && (eq(4, "on") || eq(4, "off"))) { p.enabled = eq(4, "on"); okay = true; }
        else {
          for (unsigned type = 0; type < 4; ++type) if (eq(4, types[type])) {
            const bool notch = type == 3;
            if (n == (notch ? 7u : 8u)) {
              p.type = FilterType(type); p.enabled = 1; p.gainDb = 0;
              okay = number(tokens[5], p.frequency) && number(tokens[6], p.q) &&
                  (notch || number(tokens[7], p.gainDb));
            }
          }
        }
      }
    }
  }
  if (!okay) { Serial.println("[dsp] invalid command/number; use dsp help"); return; }
  Prepared check;
  const char *error = nullptr;
  if (!prepare(next, requestedRate.load(), nextTone, nextLevel, check, error)) {
    Serial.printf("[dsp] %s\n", error); return;
  }
  config = next; toneHz = nextTone; toneDb = nextLevel;
  if (!toneOnly) { unsaved = true; configState = "RAM"; }
  if (!masterOnly) dirty = true;
  masterUpdate();
  Serial.println("[dsp] applied in RAM; dsp save persists settings, never tone");
}
}  // namespace dsp_control
