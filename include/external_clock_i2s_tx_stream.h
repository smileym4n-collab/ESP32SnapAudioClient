#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#include "AudioTools.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp32-hal-matrix.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"
#include "soc/i2s_struct.h"

#include "board_config.h"
#include "snapclient_config.h"

class ExternalClockI2STxStream : public audio_tools::AudioStream {
 public:
  bool begin(uint32_t sampleRate,
             uint8_t channels,
             uint8_t bitsPerSample,
             uint8_t dmaBufferCount,
             uint16_t dmaBufferFrames) {
    end();

    audio_tools::AudioInfo sourceInfo(sampleRate, channels, bitsPerSample);
    audio_tools::AudioStream::setAudioInfo(sourceInfo);

    if (!validatePins()) {
      return false;
    }

    pinMode(board_config::I2S_BCK_IN, INPUT);
    pinMode(board_config::I2S_LRCK_IN, INPUT);
    pinMode(board_config::I2S_DATA_OUT, OUTPUT);
    digitalWrite(board_config::I2S_DATA_OUT, LOW);

    i2s_config_t i2sConfig = {};
    i2sConfig.mode = static_cast<i2s_mode_t>(I2S_MODE_SLAVE | I2S_MODE_TX);
    i2sConfig.sample_rate = app_config::I2S_EXTERNAL_SAMPLE_RATE;
    // Classic ESP32 DMA requires the configured sample width to match the
    // channel width. Feed complete 32-bit slots; the source samples occupy the
    // most-significant 16 bits and the remaining bits are zero, which is valid
    // 24-bit Philips I2S data for the Zeppelin DSP.
    i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    i2sConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sConfig.intr_alloc_flags = 0;
    i2sConfig.dma_buf_count = dmaBufferCount;
    i2sConfig.dma_buf_len = dmaBufferFrames;
    // ESP32 slave mode synchronizes external BCK through its internal module
    // clock, which must be at least 8x BCK. 64-bit stereo frames therefore
    // require 512fs internally. This clock is not routed to an MCLK pin.
    i2sConfig.use_apll = app_config::I2S_USE_AUDIO_PLL;
    // Opus PCM arrives in bursts while the Zeppelin clocks continuously.
    // Hardware auto-clear erases the next descriptor between normal decoder
    // writes, so idle silence is handled explicitly below instead.
    i2sConfig.tx_desc_auto_clear = false;
    i2sConfig.fixed_mclk =
        app_config::I2S_EXTERNAL_SAMPLE_RATE * 512;
    i2sConfig.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    i2sConfig.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;

    esp_err_t err = i2s_driver_install(port_,
                                       &i2sConfig,
                                       app_config::I2S_EVENT_QUEUE_LENGTH,
                                       &eventQueue_);
    if (err != ESP_OK) {
      Serial.printf("[i2s] slave-tx driver install failed: %s\n",
                    esp_err_to_name(err));
      eventQueue_ = nullptr;
      holdDataLow();
      return false;
    }

    i2s_pin_config_t pinConfig = {};
    pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;
    pinConfig.bck_io_num = board_config::I2S_BCK_IN;
    pinConfig.ws_io_num = board_config::I2S_LRCK_IN;
    pinConfig.data_out_num = board_config::I2S_DATA_OUT;
    pinConfig.data_in_num = I2S_PIN_NO_CHANGE;

    err = i2s_set_pin(port_, &pinConfig);
    if (err != ESP_OK) {
      Serial.printf("[i2s] slave-tx pin config failed: %s\n",
                    esp_err_to_name(err));
      i2s_driver_uninstall(port_);
      eventQueue_ = nullptr;
      holdDataLow();
      return false;
    }

    i2s_zero_dma_buffer(port_);
    I2S1.conf1.tx_pcm_bypass = 1;
    I2S1.conf1.tx_stop_en = 0;
    I2S1.timing.tx_bck_in_inv = 0;
    GPIO.func_in_sel_cfg[I2S1O_WS_IN_IDX].sig_in_inv = 0;
    pinMatrixOutAttach(board_config::I2S_DATA_OUT,
                       dataSignal_,
                       false,
                       false);
    GPIO.func_out_sel_cfg[board_config::I2S_DATA_OUT].oen_sel = 1;
    GPIO.enable_w1ts =
        static_cast<uint32_t>(1UL << board_config::I2S_DATA_OUT);
    active_ = true;
    lastSuccessfulWriteMs_ = millis();
    lastPcmWriteMs_ = lastSuccessfulWriteMs_;
    idleDmaCleared_ = true;
    lastExternalClockTimeoutMs_ = 0;
    missingClockTimeouts_ = 0;
    dmaEventCount_ = 0;
    firstAudioQueuedMs_ = 0;
    txResyncDone_ = false;

    Serial.printf(
        "[i2s] slave-tx initialized: source=%lu Hz/%u-bit/%u ch, wire=%lu Hz Philips I2S, 24 valid bits in 32-bit slots\n",
        static_cast<unsigned long>(sampleRate),
        bitsPerSample,
        channels,
        static_cast<unsigned long>(app_config::I2S_EXTERNAL_SAMPLE_RATE));
    Serial.printf("[i2s] gpio bck_in=%d lrck_in=%d data_out=%d mclk=disconnected\n",
                  board_config::I2S_BCK_IN,
                  board_config::I2S_LRCK_IN,
                  board_config::I2S_DATA_OUT);
    Serial.printf(
        "[i2s] data matrix gpio=%d selected=%lu expected=%d forced_oen=%u output_en=%u\n",
                  board_config::I2S_DATA_OUT,
                  static_cast<unsigned long>(
                      GPIO.func_out_sel_cfg[board_config::I2S_DATA_OUT].val &
                      0x1ffU),
                  dataSignal_,
                  GPIO.func_out_sel_cfg[board_config::I2S_DATA_OUT].oen_sel,
                  static_cast<unsigned int>(
                      (GPIO.enable >> board_config::I2S_DATA_OUT) & 0x1U));
    Serial.printf("[i2s] dma=%u x %u frames, write_timeout=%lums, mode=slave-tx\n",
                  dmaBufferCount,
                  dmaBufferFrames,
                  static_cast<unsigned long>(app_config::I2S_WRITE_TIMEOUT_MS));
    return true;
  }

  bool begin() override { return active_; }

  void end() override {
    if (active_) {
      i2s_zero_dma_buffer(port_);
      i2s_driver_uninstall(port_);
      active_ = false;
      eventQueue_ = nullptr;
    }
    holdDataLow();
  }

  void setAudioInfo(audio_tools::AudioInfo newInfo) override {
    audio_tools::AudioStream::setAudioInfo(newInfo);
    if (active_ && newInfo.sample_rate != app_config::I2S_EXTERNAL_SAMPLE_RATE) {
      Serial.printf(
          "[i2s] source sample rate %ld Hz will be clocked by external 48 kHz LRCK\n",
          static_cast<long>(newInfo.sample_rate));
    }
  }

  audio_tools::AudioInfo audioInfoOut() override {
    return audio_tools::AudioInfo(app_config::I2S_EXTERNAL_SAMPLE_RATE,
                                  app_config::AUDIO_CHANNELS,
                                  app_config::I2S_SLOT_BITS);
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (data == nullptr || len == 0 || !active_) {
      return 0;
    }

    lastPcmWriteMs_ = millis();
    idleDmaCleared_ = false;
    pollEvents();

    if (info.channels != app_config::AUDIO_CHANNELS ||
        info.bits_per_sample != app_config::AUDIO_BITS_PER_SAMPLE) {
      maybeLogUnsupportedFormat();
      return 0;
    }

    constexpr size_t kSourceBytesPerFrame = sizeof(int16_t) * 2;
    constexpr size_t kOutputSlotsPerPass = 256;
    int32_t outputSlots[kOutputSlotsPerPass];

    const size_t frameCount = len / kSourceBytesPerFrame;
    size_t framesConsumed = 0;

    while (framesConsumed < frameCount) {
      const size_t framesThisPass =
          min(kOutputSlotsPerPass / 2, frameCount - framesConsumed);
      const int16_t *input =
          reinterpret_cast<const int16_t *>(data +
                                            framesConsumed *
                                                kSourceBytesPerFrame);

      for (size_t i = 0; i < framesThisPass * 2; ++i) {
        outputSlots[i] = static_cast<int32_t>(input[i]) << 16;
      }

      size_t bytesWritten = 0;
      const size_t bytesToWrite = framesThisPass * 2 * sizeof(int32_t);
      const esp_err_t err = i2s_write(port_,
                                      outputSlots,
                                      bytesToWrite,
                                      &bytesWritten,
                                      pdMS_TO_TICKS(
                                          app_config::I2S_WRITE_TIMEOUT_MS));
      if (err != ESP_OK || bytesWritten == 0) {
        noteWriteTimeout(err);
        break;
      }

      lastSuccessfulWriteMs_ = millis();
      noteAudioQueued(outputSlots, framesThisPass * 2);
      if (!txResyncDone_ && firstAudioQueuedMs_ != 0) {
        txResyncDone_ = true;
        const esp_err_t stopErr = i2s_stop(port_);
        const esp_err_t startErr = i2s_start(port_);
        Serial.printf(
            "[i2s] first-pcm slave-tx resync stop=%s start=%s\n",
            esp_err_to_name(stopErr),
            esp_err_to_name(startErr));
      }
      framesConsumed += bytesWritten / (sizeof(int32_t) * 2);

      if (bytesWritten < bytesToWrite) {
        noteWriteTimeout(ESP_ERR_TIMEOUT);
        break;
      }
    }

    return framesConsumed * kSourceBytesPerFrame;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    (void)data;
    (void)len;
    return 0;
  }

  int available() override { return 0; }

  int availableForWrite() override {
    return active_ ? app_config::I2S_DMA_BUFFER_COUNT *
                         app_config::I2S_DMA_BUFFER_SIZE *
                         app_config::I2S_OUTPUT_BYTES_PER_FRAME
                   : 0;
  }

  void flush() override {
    if (active_) {
      i2s_zero_dma_buffer(port_);
    }
  }

  operator bool() override { return active_; }

  bool isActive() const { return active_; }

  bool externalClockMissingRecently(uint32_t timeoutMs) const {
    return lastExternalClockTimeoutMs_ > 0 &&
           (millis() - lastExternalClockTimeoutMs_) < timeoutMs;
  }

  void silenceIfIdle(uint32_t timeoutMs) {
    if (!active_ || idleDmaCleared_ ||
        (millis() - lastPcmWriteMs_) < timeoutMs) {
      return;
    }

    i2s_zero_dma_buffer(port_);
    idleDmaCleared_ = true;
  }

 private:
  static constexpr i2s_port_t port_ = I2S_NUM_1;
  static constexpr int dataSignal_ = I2S1O_DATA_OUT23_IDX;

  QueueHandle_t eventQueue_ = nullptr;
  bool active_ = false;
  uint32_t lastSuccessfulWriteMs_ = 0;
  volatile uint32_t lastPcmWriteMs_ = 0;
  volatile bool idleDmaCleared_ = true;
  uint32_t lastExternalClockTimeoutMs_ = 0;
  uint32_t lastClockTimeoutLogMs_ = 0;
  uint32_t lastDmaEventLogMs_ = 0;
  uint32_t lastUnsupportedFormatLogMs_ = 0;
  uint32_t missingClockTimeouts_ = 0;
  uint32_t dmaEventCount_ = 0;
  uint32_t firstAudioQueuedMs_ = 0;
  bool txResyncDone_ = false;

  bool validatePins() {
    if (board_config::I2S_BCK_IN < 0 ||
        board_config::I2S_LRCK_IN < 0 ||
        board_config::I2S_DATA_OUT < 0) {
      Serial.printf(
          "[i2s] Zeppelin GPIOs are not configured: I2S_BCK_IN=%d I2S_LRCK_IN=%d I2S_DATA_OUT=%d\n",
          board_config::I2S_BCK_IN,
          board_config::I2S_LRCK_IN,
          board_config::I2S_DATA_OUT);
      return false;
    }
    return true;
  }

  void holdDataLow() {
    if (board_config::I2S_DATA_OUT >= 0) {
      pinMode(board_config::I2S_DATA_OUT, OUTPUT);
      digitalWrite(board_config::I2S_DATA_OUT, LOW);
    }
  }

  void noteAudioQueued(const int32_t *slots, size_t slotCount) {
    if (firstAudioQueuedMs_ != 0) {
      return;
    }
    for (size_t i = 0; i < slotCount; ++i) {
      if (slots[i] != 0) {
        firstAudioQueuedMs_ = millis();
        return;
      }
    }
  }

  void noteWriteTimeout(esp_err_t err) {
    ++missingClockTimeouts_;
    lastExternalClockTimeoutMs_ = millis();
    const uint32_t nowMs = millis();
    if (missingClockTimeouts_ == 1 ||
        nowMs - lastClockTimeoutLogMs_ >=
            app_config::I2S_DIAGNOSTIC_LOG_INTERVAL_MS) {
      Serial.printf(
          "[i2s] write stalled waiting for external BCK/LRCK err=%s count=%lu last_ok_ms=%lu\n",
          esp_err_to_name(err),
          static_cast<unsigned long>(missingClockTimeouts_),
          static_cast<unsigned long>(lastSuccessfulWriteMs_));
      lastClockTimeoutLogMs_ = nowMs;
    }
  }

  void pollEvents() {
    if (eventQueue_ == nullptr) {
      return;
    }

    i2s_event_t event = {};
    while (xQueueReceive(eventQueue_, &event, 0) == pdTRUE) {
      if (event.type == I2S_EVENT_DMA_ERROR ||
          event.type == I2S_EVENT_TX_Q_OVF) {
        ++dmaEventCount_;
        const uint32_t nowMs = millis();
        if (dmaEventCount_ == 1 ||
            nowMs - lastDmaEventLogMs_ >=
                app_config::I2S_DIAGNOSTIC_LOG_INTERVAL_MS) {
          Serial.printf("[i2s] dma event type=%d count=%lu\n",
                        static_cast<int>(event.type),
                        static_cast<unsigned long>(dmaEventCount_));
          lastDmaEventLogMs_ = nowMs;
        }
      }
    }
  }

  void maybeLogUnsupportedFormat() {
    const uint32_t nowMs = millis();
    if (nowMs - lastUnsupportedFormatLogMs_ <
        app_config::I2S_DIAGNOSTIC_LOG_INTERVAL_MS) {
      return;
    }

    Serial.printf(
        "[i2s] unsupported source format for Zeppelin output: %ld Hz/%d-bit/%d ch\n",
        static_cast<long>(info.sample_rate),
        info.bits_per_sample,
        info.channels);
    lastUnsupportedFormatLogMs_ = nowMs;
  }

};
