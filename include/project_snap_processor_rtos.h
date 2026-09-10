#pragma once

#include <vector>

#include <Arduino.h>

#include "api/SnapProcessorRTOS.h"

class ProjectSnapProcessorRTOS : public snap_arduino::SnapProcessorRTOS {
 public:
  ProjectSnapProcessorRTOS(snap_arduino::SnapOutput &output,
                           int bufferSizeBytes,
                           int activationAtPercent)
      : snap_arduino::SnapProcessorRTOS(output,
                                        bufferSizeBytes,
                                        activationAtPercent) {
    instance() = this;
  }

  ProjectSnapProcessorRTOS(int bufferSizeBytes, int activationAtPercent)
      : snap_arduino::SnapProcessorRTOS(bufferSizeBytes, activationAtPercent) {
    instance() = this;
  }

  void setPeriodicStatsEnabled(bool enabled) { periodicStatsEnabled_ = enabled; }
  void setRebufferEnabled(bool enabled) {
    rebufferEnabled_ = enabled;
    if (!rebufferEnabled_) {
      rebuffering_ = false;
    }
  }
  void setRebufferThresholds(uint8_t startPercent, uint8_t resumePercent) {
    rebufferStartPercent_ = constrain(startPercent, 1, 99);
    rebufferResumePercent_ = constrain(resumePercent, 1, 99);
    if (rebufferResumePercent_ < rebufferStartPercent_) {
      rebufferResumePercent_ = rebufferStartPercent_;
    }
  }

  ~ProjectSnapProcessorRTOS() {
    if (instance() == this) {
      instance() = nullptr;
    }
  }

  bool begin() override {
    instance() = this;
    const bool started = snap_arduino::SnapProcessorRTOS::begin();

    if (started) {
      // Reserve the consume-side scratch buffer up front so the first large
      // chunk does not trigger a malloc inside the audio copy task.
      chunkBuffer_.reserve(kChunkBufferReserveBytes);
      Serial.printf("[snapclient-buf] byte_queue=%lu activation=%d slots=%d\n",
                    static_cast<unsigned long>(buffer.size()),
                    bufferTaskActivationLimit(),
                    RTOS_MAX_QUEUE_ENTRY_COUNT);
      lastLogMs_ = millis();
      peakFillBytes_ = 0;
    }

    return started;
  }

  void end() override {
    if (instance() == this) {
      instance() = nullptr;
    }
    snap_arduino::SnapProcessorRTOS::end();
  }

  void logRuntime() {
    maybeLogRuntime(nullptr);
  }

  void logRuntime(const char *reason, bool force) {
    maybeLogRuntime(reason, force);
  }

  bool isOutputTimedOut(uint32_t timeoutMs) const {
    return playedChunkCount_ > 0 &&
           p_snap_output != nullptr && p_snap_output->isStarted() &&
           !p_snap_output->isActive(timeoutMs);
  }

  bool hasBufferedAudio() { return buffer.available() > 0; }

  bool inputActiveRecently(uint32_t timeoutMs) const {
    return lastEnqueueActivityMs_ > 0 &&
           (millis() - lastEnqueueActivityMs_) < timeoutMs;
  }

 protected:
  void processExt() override {
    // The base SnapProcessor adds a 5 ms delay here. With Opus, decoding runs
    // in the output task, so keep the network loop responsive while still
    // yielding briefly to lower-priority work.
    delay(1);
  }

  size_t writeAudio(const uint8_t *data, size_t size) override {
    if (size > buffer.size()) {
      ++chunkTooLargeCount_;
      Serial.printf("[snapclient-buf] chunk-too-large bytes=%lu queue=%lu\n",
                    static_cast<unsigned long>(size),
                    static_cast<unsigned long>(buffer.size()));
      maybeLogRuntime("chunk-too-large", true);
      return 0;
    }

    if (!p_snap_output->isStarted() || size == 0) {
      ++notStartedDropCount_;
      if (size > 0) {
        Serial.printf("[snapclient-buf] drop-not-started bytes=%lu\n",
                      static_cast<unsigned long>(size));
      }
      maybeLogRuntime("not-started", true);
      return 0;
    }

    if (!p_snap_output->synchronizePlayback()) {
      ++syncWaitCount_;
      maybeLogSyncWait();
      return size;
    }

    // Keep size_queue and the byte buffer in lockstep: only commit a chunk if
    // the whole thing fits. A partial writeArray() would leave size_queue
    // claiming more bytes than were stored, desyncing every later chunk
    // boundary and feeding the Opus decoder garbage. Drop the chunk cleanly
    // instead - one lost packet is a brief glitch; a desync corrupts the
    // stream until restart.
    const int freeBytes = static_cast<int>(buffer.size()) - buffer.available();
    if (freeBytes <= static_cast<int>(size)) {
      ++bufferOverflowCount_;
      Serial.printf("[snapclient-buf] overflow-drop chunk=%lu free=%d fill=%d/%lu\n",
                    static_cast<unsigned long>(size),
                    freeBytes,
                    buffer.available(),
                    static_cast<unsigned long>(buffer.size()));
      maybeLogRuntime("overflow", true);
      return 0;
    }

    size_t queuedSize = size;
    if (!size_queue.enqueue(queuedSize)) {
      ++queueFullCount_;
      Serial.printf("[snapclient-buf] sizeq-full chunk=%lu fill=%d/%lu\n",
                    static_cast<unsigned long>(size),
                    buffer.available(),
                    static_cast<unsigned long>(buffer.size()));
      maybeLogRuntime("sizeq-full", true);
      return 0;
    }

    const size_t sizeWritten = buffer.writeArray(data, static_cast<int>(size));
    if (sizeWritten != size) {
      // Unreachable now that free space is checked up front, but keep the
      // counter so a regression in the buffer implementation stays visible.
      ++bufferOverflowCount_;
      Serial.printf("[snapclient-buf] overflow chunk=%lu wrote=%lu fill=%d/%lu\n",
                    static_cast<unsigned long>(size),
                    static_cast<unsigned long>(sizeWritten),
                    buffer.available(),
                    static_cast<unsigned long>(buffer.size()));
      maybeLogRuntime("overflow", true);
    }

    ++enqueuedChunkCount_;
    queuedBytesTotal_ += static_cast<uint32_t>(sizeWritten);
    lastEnqueueActivityMs_ = millis();
    lastActivityMs_ = millis();

    if (!task_started && buffer.available() > bufferTaskActivationLimit()) {
      Serial.printf("[snapclient-buf] output-task-start fill=%d/%lu\n",
                    buffer.available(),
                    static_cast<unsigned long>(buffer.size()));
      task_started = true;
      task.begin(taskCopy);
    }

    maybeLogRuntime(nullptr);
    return sizeWritten;
  }

 private:
  // Typical Opus wire chunks are well under this; sized to cover a few packets
  // so the consume task never reallocates mid-stream after warmup.
  static constexpr size_t kChunkBufferReserveBytes = 4096;

  std::vector<uint8_t> chunkBuffer_;
  uint32_t lastLogMs_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t peakFillBytes_ = 0;
  uint32_t queuedBytesTotal_ = 0;
  uint32_t playedBytesTotal_ = 0;
  uint32_t lastEnqueueActivityMs_ = 0;
  uint32_t lastPlayActivityMs_ = 0;
  uint32_t enqueuedChunkCount_ = 0;
  uint32_t playedChunkCount_ = 0;
  uint32_t queueFullCount_ = 0;
  uint32_t bufferOverflowCount_ = 0;
  uint32_t outputShortWriteCount_ = 0;
  uint32_t readShortCount_ = 0;
  uint32_t syncWaitCount_ = 0;
  uint32_t notStartedDropCount_ = 0;
  uint32_t chunkTooLargeCount_ = 0;
  uint32_t lastSyncWaitLogMs_ = 0;
  bool periodicStatsEnabled_ = true;
  bool rebufferEnabled_ = true;
  bool rebuffering_ = false;
  uint8_t rebufferStartPercent_ = 55;
  uint8_t rebufferResumePercent_ = 75;

  static void taskCopy() {
    while (instance() != nullptr) {
      instance()->copyTaskStep();
    }
  }

  static ProjectSnapProcessorRTOS *&instance() {
    static ProjectSnapProcessorRTOS *self = nullptr;
    return self;
  }

  void copyTaskStep() {
    const uint32_t fillBytes = static_cast<uint32_t>(buffer.available());
    const uint32_t queueSize = static_cast<uint32_t>(buffer.size());
    const uint32_t resumeLimit =
        (queueSize * static_cast<uint32_t>(rebufferResumePercent_)) / 100U;
    const uint32_t lowWaterLimit =
        (queueSize * static_cast<uint32_t>(rebufferStartPercent_)) / 100U;

    if (rebufferEnabled_ && !rebuffering_ && fillBytes > 0 &&
        fillBytes < lowWaterLimit) {
      rebuffering_ = true;
      if (periodicStatsEnabled_) {
        Serial.printf("[snapclient-buf] rebuffer-start fill=%lu/%lu\n",
                      static_cast<unsigned long>(fillBytes),
                      static_cast<unsigned long>(buffer.size()));
        maybeLogRuntime("rebuffer-start", true);
      }
    }

    if (rebufferEnabled_ && rebuffering_) {
      if (fillBytes < resumeLimit) {
        delay(1);
        return;
      }

      rebuffering_ = false;
      if (periodicStatsEnabled_) {
        Serial.printf("[snapclient-buf] rebuffer-end fill=%lu/%lu\n",
                      static_cast<unsigned long>(fillBytes),
                      static_cast<unsigned long>(buffer.size()));
        maybeLogRuntime("rebuffer-end", true);
      }
    }

    size_t chunkSize = 0;
    bool copiedChunk = false;
    if (size_queue.dequeue(chunkSize)) {
      if (chunkBuffer_.size() < chunkSize) {
        chunkBuffer_.resize(chunkSize);
      }

      const int bytesRead =
          buffer.readArray(chunkBuffer_.data(), static_cast<int>(chunkSize));
      if (bytesRead != static_cast<int>(chunkSize)) {
        ++readShortCount_;
        Serial.printf("[snapclient-buf] read-short want=%lu got=%d fill=%d/%lu\n",
                      static_cast<unsigned long>(chunkSize),
                      bytesRead,
                      buffer.available(),
                      static_cast<unsigned long>(buffer.size()));
        maybeLogRuntime("read-short", true);
      }

      const size_t bytesToWrite =
          bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0;
      const size_t bytesWritten =
          bytesToWrite > 0
              ? p_snap_output->audioWrite(chunkBuffer_.data(), bytesToWrite)
              : 0;
      if (bytesWritten != bytesToWrite) {
        ++outputShortWriteCount_;
        Serial.printf(
            "[snapclient-buf] output-short want=%lu wrote=%lu fill=%d/%lu\n",
            static_cast<unsigned long>(bytesToWrite),
            static_cast<unsigned long>(bytesWritten),
            buffer.available(),
            static_cast<unsigned long>(buffer.size()));
        maybeLogRuntime("output-short", true);
      }

      ++playedChunkCount_;
      playedBytesTotal_ += static_cast<uint32_t>(bytesWritten);
      if (bytesWritten > 0) {
        lastPlayActivityMs_ = millis();
      }
      lastActivityMs_ = millis();
      maybeLogRuntime(nullptr);
      copiedChunk = bytesWritten > 0;
    }

    if (!copiedChunk) {
      delay(1);
    }
  }

  void maybeLogRuntime(const char *reason, bool force = false) {
    if (!force && !periodicStatsEnabled_) {
      return;
    }

    const uint32_t nowMs = millis();
    const uint32_t fillBytes = static_cast<uint32_t>(buffer.available());
    if (fillBytes > peakFillBytes_) {
      peakFillBytes_ = fillBytes;
    }

    if (!force && (nowMs - lastLogMs_) < 1000) {
      return;
    }

    Serial.printf(
        "[snapclient-buf] fill=%lu/%lu peak=%lu enq=%lu deq=%lu queued=%lu played=%lu qfull=%lu ovf=%lu outshort=%lu readshort=%lu syncwait=%lu drop=%lu toolarge=%lu reason=%s\n",
        static_cast<unsigned long>(fillBytes),
        static_cast<unsigned long>(buffer.size()),
        static_cast<unsigned long>(peakFillBytes_),
        static_cast<unsigned long>(enqueuedChunkCount_),
        static_cast<unsigned long>(playedChunkCount_),
        static_cast<unsigned long>(queuedBytesTotal_),
        static_cast<unsigned long>(playedBytesTotal_),
        static_cast<unsigned long>(queueFullCount_),
        static_cast<unsigned long>(bufferOverflowCount_),
        static_cast<unsigned long>(outputShortWriteCount_),
        static_cast<unsigned long>(readShortCount_),
        static_cast<unsigned long>(syncWaitCount_),
        static_cast<unsigned long>(notStartedDropCount_),
        static_cast<unsigned long>(chunkTooLargeCount_),
        reason != nullptr ? reason : "-");
    lastLogMs_ = nowMs;
  }

  void maybeLogSyncWait() {
    const uint32_t nowMs = millis();
    if (periodicStatsEnabled_ || syncWaitCount_ == 1 ||
        (nowMs - lastSyncWaitLogMs_) >= 250) {
      maybeLogRuntime("sync-wait", true);
      lastSyncWaitLogMs_ = nowMs;
    }
  }
};
