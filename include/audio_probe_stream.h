#pragma once

#include <string.h>

#include <vector>

#include <Arduino.h>

#include "AudioTools.h"
#include "audio_output_controller.h"
#include "channel_mode.h"

class AudioProbeStream : public audio_tools::AudioStream {
 public:
  explicit AudioProbeStream(audio_tools::AudioStream &target) : target_(&target) {}

  void setPcmGain(float gain) { pcmGain_ = gain; }
  void setPeriodicStatsEnabled(bool enabled) { periodicStatsEnabled_ = enabled; }

  // Source of truth for the active Snapclient channel routing. The probe reads
  // the live mode on every write, so POST /api/channel-mode takes effect
  // immediately without a reboot. Left null = stereo passthrough.
  void setChannelController(AudioOutputController *controller) {
    controller_ = controller;
  }

  // Starts a linear fade-out of the Snapclient PCM over durationMs, applied by
  // the decode task as it keeps writing. Used to quiet the output before an OTA
  // flash so the update is not fighting the audio pipeline for CPU and Wi-Fi.
  void beginFadeOut(uint32_t durationMs) {
    fadeStartMs_ = millis();
    fadeDurationMs_ = durationMs > 0 ? durationMs : 1;
    fadeActive_ = true;  // set last so the decode task sees consistent fields
  }

  bool begin() override {
    processBuffer_.reserve(kProcessBufferReserveBytes);
    // The shared I2S output is started by AudioOutputController before
    // Snapclient begins. Re-opening it here can force a second DMA allocation
    // during codec-header handling and crash the ESP32 driver.
    return target_ != nullptr;
  }

  void end() override {
    // AudioOutputController owns the shared I2S peripheral. Snapclient may end
    // and rebuild its decoder chain between streams; closing the target here
    // would leave GPIO DATA low because begin() intentionally does not reopen
    // that shared peripheral.
  }

  void setAudioInfo(audio_tools::AudioInfo newInfo) override {
    info = newInfo;
    firstWriteLogsRemaining_ = 3;
    Serial.printf("[snapclient-pcm] format=%ld Hz, %d-bit, %d ch\n",
                  static_cast<long>(newInfo.sample_rate),
                  newInfo.bits_per_sample,
                  newInfo.channels);
    if (target_ != nullptr) {
      target_->setAudioInfo(newInfo);
      const audio_tools::AudioInfo applied = target_->audioInfo();
      Serial.printf("[i2s] format update=%ld Hz, %d-bit, %d ch\n",
                    static_cast<long>(applied.sample_rate),
                    applied.bits_per_sample,
                    applied.channels);
      if (applied.sample_rate != newInfo.sample_rate ||
          applied.bits_per_sample != newInfo.bits_per_sample ||
          applied.channels != newInfo.channels) {
        Serial.printf(
            "[i2s] format mismatch requested=%ld/%d/%d applied=%ld/%d/%d\n",
            static_cast<long>(newInfo.sample_rate),
            newInfo.bits_per_sample,
            newInfo.channels,
            static_cast<long>(applied.sample_rate),
            applied.bits_per_sample,
            applied.channels);
      }
    }
  }

  audio_tools::AudioInfo audioInfo() override {
    return target_ != nullptr ? target_->audioInfo() : info;
  }

  int available() override { return target_ != nullptr ? target_->available() : 0; }

  int availableForWrite() override {
    return target_ != nullptr ? target_->availableForWrite() : 0;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    return target_ != nullptr ? target_->readBytes(data, len) : 0;
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (target_ == nullptr) {
      return 0;
    }

    const app_config::ChannelMode channelMode =
        controller_ != nullptr ? controller_->channelMode()
                               : app_config::ChannelMode::Stereo;
    const bool routeChannels =
        channelMode != app_config::ChannelMode::Stereo &&
        info.bits_per_sample == 16 && info.channels == 2 &&
        len >= sizeof(int16_t) * 2;
    const float gain = effectiveGain();
    const bool scaleSamples =
        gain < 0.999f && info.bits_per_sample == 16 && len >= sizeof(int16_t);

    const uint8_t *writeData = data;
    size_t writeLen = len;
    bool processBufferActive = routeChannels || scaleSamples;
    if (processBufferActive) {
      processBuffer_.resize(len);
      if (routeChannels) {
        app_config::routeStereo16(channelMode, data, len, processBuffer_.data());
      } else {
        memcpy(processBuffer_.data(), data, len);
      }
    }

    if (processBufferActive) {
      if (scaleSamples) {
        applyScalar(processBuffer_.data(), processBuffer_.size(), gain);
      }
      writeData = processBuffer_.data();
      writeLen = processBuffer_.size();
    }

    const size_t written = target_->write(writeData, writeLen);
    if (periodicStatsEnabled_) {
      accumulate(writeData, written);
    }
    maybeLogFirstWrites(writeData, written);
    maybeLog();
    return written;
  }

 private:
  audio_tools::AudioStream *target_ = nullptr;
  AudioOutputController *controller_ = nullptr;
  std::vector<uint8_t> processBuffer_;
  uint32_t windowStartMs_ = millis();
  uint32_t windowBytes_ = 0;
  uint16_t windowPeak_ = 0;
  float pcmGain_ = 1.0f;
  bool periodicStatsEnabled_ = true;
  uint8_t firstWriteLogsRemaining_ = 3;
  bool fadeActive_ = false;
  uint32_t fadeStartMs_ = 0;
  uint32_t fadeDurationMs_ = 1;

  static uint16_t maxAbsPcm16(const uint8_t *buffer, size_t size) {
    const size_t sampleCount = size / sizeof(int16_t);
    const int16_t *samples = reinterpret_cast<const int16_t *>(buffer);
    uint16_t peak = 0;

    for (size_t i = 0; i < sampleCount; ++i) {
      const int32_t value = samples[i];
      const uint16_t magnitude =
          static_cast<uint16_t>(value < 0 ? -value : value);
      if (magnitude > peak) {
        peak = magnitude;
      }
    }

    return peak;
  }

  static constexpr size_t kProcessBufferReserveBytes = 4096;

  void accumulate(const uint8_t *buffer, size_t size) {
    windowBytes_ += static_cast<uint32_t>(size);
    const uint16_t chunkPeak = maxAbsPcm16(buffer, size);
    if (chunkPeak > windowPeak_) {
      windowPeak_ = chunkPeak;
    }
  }

  void maybeLog() {
    if (!periodicStatsEnabled_) {
      return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - windowStartMs_ < 1000) {
      return;
    }

    Serial.printf("[snapclient-pcm] bytes=%lu peak16=%u\n",
                  static_cast<unsigned long>(windowBytes_),
                  windowPeak_);
    windowStartMs_ = nowMs;
    windowBytes_ = 0;
    windowPeak_ = 0;
  }

  void maybeLogFirstWrites(const uint8_t *buffer, size_t size) {
    if (firstWriteLogsRemaining_ == 0 || size == 0) {
      return;
    }

    Serial.printf("[snapclient-pcm] first-write bytes=%lu peak16=%u\n",
                  static_cast<unsigned long>(size),
                  maxAbsPcm16(buffer, size));
    --firstWriteLogsRemaining_;
  }

  // Combined gain for this write: the configured final PCM trim multiplied by
  // the current fade-out envelope (1.0 when not fading, ramping to 0.0).
  float effectiveGain() const {
    float gain = (pcmGain_ > 0.0f && pcmGain_ < 1.0f) ? pcmGain_ : 1.0f;
    if (fadeActive_) {
      const uint32_t elapsed = millis() - fadeStartMs_;
      const float fade = elapsed >= fadeDurationMs_
                             ? 0.0f
                             : 1.0f - static_cast<float>(elapsed) /
                                          static_cast<float>(fadeDurationMs_);
      gain *= fade;
    }
    return gain;
  }

  void applyScalar(uint8_t *buffer, size_t size, float gain) {
    int16_t *samples = reinterpret_cast<int16_t *>(buffer);
    const size_t sampleCount = size / sizeof(int16_t);

    for (size_t i = 0; i < sampleCount; ++i) {
      const float scaled = static_cast<float>(samples[i]) * gain;
      if (scaled > 32767.0f) {
        samples[i] = 32767;
      } else if (scaled < -32768.0f) {
        samples[i] = -32768;
      } else {
        samples[i] = static_cast<int16_t>(scaled);
      }
    }
    // A trailing odd byte (not a full 16-bit sample) is left unchanged.
  }
};
