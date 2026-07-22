#include "snapclient_mode.h"

#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>

#include "bluetooth_name_store.h"
#include "board_config.h"
#include "channel_mode_store.h"
#include "power_source_store.h"
#include "project_snap_processor_rtos.h"
#include "snapclient_config.h"

namespace {

constexpr uint8_t kEspImageHeaderMagic = 0xE9;
constexpr unsigned int kStatusJsonReserveBytes = 2048;

void writeWifiStatusLed(bool on) {
  const int level = on
                        ? (board_config::WIFI_STATUS_LED_ACTIVE_HIGH ? HIGH : LOW)
                        : (board_config::WIFI_STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(board_config::WIFI_STATUS_LED_PIN, level);
}

size_t otaPartitionSize() {
  const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
  return partition != nullptr ? partition->size : 0;
}

bool otaFirmwareUpdateSupported() {
  return app_config::OTA_FIRMWARE_UPDATE_ENABLED && otaPartitionSize() > 0;
}

bool extractChannelMode(const String &body, app_config::ChannelMode &mode) {
  app_config::ChannelMode rawMode = app_config::ChannelMode::Stereo;
  if (app_config::parseChannelMode(body, rawMode)) {
    mode = rawMode;
    return true;
  }

  const int keyIndex = body.indexOf("\"channel_mode\"");
  if (keyIndex < 0) {
    return false;
  }

  const int colonIndex = body.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return false;
  }

  const int valueStart = body.indexOf('"', colonIndex + 1);
  if (valueStart < 0) {
    return false;
  }

  const int valueEnd = body.indexOf('"', valueStart + 1);
  if (valueEnd < 0) {
    return false;
  }

  const String value = body.substring(valueStart + 1, valueEnd);
  return app_config::parseChannelMode(value, mode);
}

bool extractPowerSource(const String &body, app_config::PowerSource &source) {
  app_config::PowerSource rawSource = app_config::PowerSource::Battery;
  if (app_config::parsePowerSource(body, rawSource)) {
    source = rawSource;
    return true;
  }

  const int keyIndex = body.indexOf("\"power_source\"");
  if (keyIndex < 0) {
    return false;
  }

  const int colonIndex = body.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return false;
  }

  const int valueStart = body.indexOf('"', colonIndex + 1);
  if (valueStart < 0) {
    return false;
  }

  const int valueEnd = body.indexOf('"', valueStart + 1);
  if (valueEnd < 0) {
    return false;
  }

  const String value = body.substring(valueStart + 1, valueEnd);
  return app_config::parsePowerSource(value, source);
}

}  // namespace

SnapclientMode::SnapclientMode()
    : controlServer_(app_config::CONTROL_API_PORT),
      pcmProbe_(audioOutput_.stream()),
      snapOutput_(audio_tools::AudioInfo(app_config::AUDIO_SAMPLE_RATE,
                                         app_config::AUDIO_CHANNELS,
                                         app_config::AUDIO_BITS_PER_SAMPLE),
                  app_config::SNAPCLIENT_USE_RESAMPLER,
                  false),
      snapProcessor_(new ProjectSnapProcessorRTOS(
          snapOutput_,
          app_config::SNAP_OUTPUT_QUEUE_BYTES,
          app_config::SNAP_OUTPUT_ACTIVATION_PERCENT)),
      snapClient_(wifiClient_, pcmProbe_, codec_),
      dynamicTimeSync_(app_config::SNAPCLIENT_PROCESSING_LAG_MS,
                       app_config::SNAPCLIENT_SYNC_INTERVAL,
                       app_config::SNAPCLIENT_MIN_PLAYBACK_FACTOR,
                       app_config::SNAPCLIENT_MAX_PLAYBACK_FACTOR,
                       app_config::SNAPCLIENT_UNITY_DEADBAND) {
  pcmProbe_.setPcmGain(app_config::SNAPCLIENT_FINAL_PCM_GAIN);
  pcmProbe_.setChannelController(&audioOutput_);
  pcmProbe_.setPeriodicStatsEnabled(app_config::SNAPCLIENT_PERIODIC_STATS_ENABLED);
  snapProcessor_->setPeriodicStatsEnabled(
      app_config::SNAPCLIENT_PERIODIC_STATS_ENABLED);
  snapProcessor_->setRebufferEnabled(app_config::SNAPCLIENT_REBUFFER_ENABLED);
  snapProcessor_->setRebufferThresholds(
      app_config::SNAP_OUTPUT_REBUFFER_START_PERCENT,
      app_config::SNAP_OUTPUT_REBUFFER_RESUME_PERCENT);
}
SnapclientMode::~SnapclientMode() = default;

bool SnapclientMode::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool SnapclientMode::lowBatteryWarningActive() const {
  if (powerSource_ != app_config::PowerSource::Battery) {
    return false;
  }

  const BatteryStatus battery = batteryMonitor_.status();
  return battery.available &&
         battery.percent <= app_config::BATTERY_LOW_WARNING_PERCENT;
}

bool SnapclientMode::begin() {
  Serial.println("[mode] starting Snapclient over Wi-Fi");
  Serial.println("[wifi] connecting...");

  if (!connectWifiWithTimeout()) {
    Serial.println("[wifi] failed to connect");
    logWifiFailureDiagnostics();
    wifiStartupFailed_ = true;
    lastWifiStartupRetryMs_ = millis();
    Serial.printf("[wifi] will retry every %lums without rebooting\n",
                  static_cast<unsigned long>(
                      app_config::SNAP_WIFI_STARTUP_RETRY_INTERVAL_MS));
    return true;
  }

  return startSnapclientServices();
}

bool SnapclientMode::startSnapclientServices() {
  Serial.print("[wifi] connected, ip=");
  Serial.println(WiFi.localIP());

  if (!audioOutput_.begin(app_config::AUDIO_SAMPLE_RATE)) {
    Serial.println("[i2s] begin failed");
    return false;
  }
  audioOutput_.setChannelMode(loadChannelModePreference());
  Serial.printf("[channel] snapclient channel mode=%s\n",
                app_config::channelModeName(audioOutput_.channelMode()));
  powerSource_ = loadPowerSourcePreference();
  pendingPowerSource_ = powerSource_;
  bluetoothName_ = loadBluetoothNamePreference();
  pendingBluetoothName_ = bluetoothName_;
  Serial.printf("[power] source=%s\n", app_config::powerSourceName(powerSource_));
  if (powerSource_ == app_config::PowerSource::Battery) {
    batteryMonitor_.begin();
    batteryMonitor_.update(true);
  }
  beginControlApi();

  snapClient_.setSnapProcessor(*snapProcessor_);
  snapClient_.setSnapTimeSync(dynamicTimeSync_);
  snapClient_.setWiFi(true);
  snapClient_.setServerIP(app_config::snapServerIp());
  snapClient_.snapProcessor().setServerPort(app_config::SNAP_SERVER_PORT);
  snapClient_.snapProcessor().setHostName(app_config::SNAP_HOST_NAME);
  snapClient_.snapProcessor().setClientName(app_config::SNAP_CLIENT_NAME);
  snapClient_.snapProcessor().setFastLoop(app_config::SNAP_USE_FAST_LOOP);
  snapClient_.setVolumeFactor(app_config::SNAPCLIENT_OUTPUT_GAIN);

  Serial.print("[snapclient] server=");
  Serial.print(app_config::snapServerIp());
  Serial.print(":");
  Serial.println(app_config::SNAP_SERVER_PORT);
  Serial.printf(
      "[audio] configured fallback=%lu Hz, %u-bit, %u ch, codec=opus\n",
      static_cast<unsigned long>(app_config::AUDIO_SAMPLE_RATE),
      app_config::AUDIO_BITS_PER_SAMPLE,
      app_config::AUDIO_CHANNELS);
  Serial.println(
      "[audio] path=Snapserver Opus -> OpusAudioDecoder -> shared I2S DAC");
  Serial.printf("[i2s] initial format=%lu Hz, %u-bit, %u ch\n",
                static_cast<unsigned long>(app_config::AUDIO_SAMPLE_RATE),
                app_config::AUDIO_BITS_PER_SAMPLE,
                app_config::AUDIO_CHANNELS);
  Serial.printf("[snapclient] queue=%lu bytes, free_psram=%lu\n",
                static_cast<unsigned long>(app_config::SNAP_OUTPUT_QUEUE_BYTES),
                static_cast<unsigned long>(ESP.getFreePsram()));
  Serial.printf("[snapclient] queue activation=%u%%\n",
                app_config::SNAP_OUTPUT_ACTIVATION_PERCENT);
  Serial.printf("[snapclient] rebuffer=%u%% -> %u%%\n",
                app_config::SNAP_OUTPUT_REBUFFER_START_PERCENT,
                app_config::SNAP_OUTPUT_REBUFFER_RESUME_PERCENT);
  Serial.printf("[snapclient] queue entry slots=%d\n",
                RTOS_MAX_QUEUE_ENTRY_COUNT);
  Serial.printf("[snapclient] output task priority=%d\n",
                RTOS_TASK_PRIORITY);
  Serial.println("[snapclient] decoder=OpusAudioDecoder");
  Serial.printf("[snapclient] output gain=%.2f\n",
                app_config::SNAPCLIENT_OUTPUT_GAIN);
  Serial.printf("[snapclient] final pcm gain=%.2f\n",
                app_config::SNAPCLIENT_FINAL_PCM_GAIN);
  Serial.printf("[snapclient] sync=dynamic-clamped range=%.4f..%.4f deadband=%.4f lag=%dms interval=%d\n",
                app_config::SNAPCLIENT_MIN_PLAYBACK_FACTOR,
                app_config::SNAPCLIENT_MAX_PLAYBACK_FACTOR,
                app_config::SNAPCLIENT_UNITY_DEADBAND,
                app_config::SNAPCLIENT_PROCESSING_LAG_MS,
                app_config::SNAPCLIENT_SYNC_INTERVAL);
  Serial.printf("[snapclient] resampler=%s\n",
                app_config::SNAPCLIENT_USE_RESAMPLER ? "on" : "off");

  if (!snapClient_.begin()) {
    Serial.println("[snapclient] begin failed");
    return false;
  }

  if (!startSnapClientTask()) {
    return false;
  }

  Serial.printf("[snapclient] task core=%ld prio=%lu stack=%lu delay=%lu\n",
                static_cast<long>(app_config::SNAPCLIENT_TASK_CORE),
                static_cast<unsigned long>(app_config::SNAPCLIENT_TASK_PRIORITY),
                static_cast<unsigned long>(app_config::SNAPCLIENT_TASK_STACK_WORDS),
                static_cast<unsigned long>(app_config::SNAPCLIENT_TASK_DELAY_MS));
  Serial.printf("[snapclient] periodic stats=%s\n",
                app_config::SNAPCLIENT_PERIODIC_STATS_ENABLED ? "on" : "off");
  Serial.println("[snapclient] running");
  snapclientStarted_ = true;
  wifiStartupFailed_ = false;
  return true;
}

void SnapclientMode::loop() {
  if (!snapclientStarted_) {
    handleWifiStartupRetry();
    delay(app_config::MAIN_LOOP_DELAY_MS);
    return;
  }

  if (powerSource_ == app_config::PowerSource::Battery) {
    batteryMonitor_.update();
  }
  handleControlApi();
  flushPendingControlSaves(false);

  if (otaRebootPending_ &&
      static_cast<int32_t>(millis() - otaRestartAtMs_) >= 0) {
    Serial.println("[ota] rebooting to apply or recover firmware");
    prepareForRestart();
    delay(app_config::OTA_REBOOT_DELAY_MS);
    ESP.restart();
  }

  const uint32_t nowMs = millis();

  if (nowMs - lastWifiCheckMs_ >= app_config::SNAP_WIFI_MONITOR_INTERVAL_MS) {
    lastWifiCheckMs_ = nowMs;
    if (WiFi.status() != WL_CONNECTED) {
      // Don't nuke playback on a single missed beacon. Give the stack a few
      // monitor intervals (and an explicit reconnect nudge) to recover before
      // falling back to a full restart.
      ++wifiLossStreak_;
      if (wifiLossStreak_ == 1) {
        Serial.println("[wifi] link down, waiting for auto-reconnect...");
        WiFi.reconnect();
      }
      if (wifiLossStreak_ >= app_config::SNAP_WIFI_LOSS_GRACE_CHECKS) {
        Serial.printf("[wifi] link lost for %u checks, restarting...\n",
                      static_cast<unsigned>(wifiLossStreak_));
        logDiagnosticSnapshot("wifi-link-lost");
        prepareForRestart();
        delay(app_config::RESTART_DELAY_MS);
        ESP.restart();
      }
    } else if (wifiLossStreak_ > 0) {
      Serial.printf("[wifi] link recovered after %u checks\n",
                    static_cast<unsigned>(wifiLossStreak_));
      wifiLossStreak_ = 0;
    }
  }

  recoverFromOutputStall();
  snapProcessor_->logRuntime();

  delay(app_config::MAIN_LOOP_DELAY_MS);
}

bool extractJsonStringValue(const String &body, const char *key, String &value) {
  String quotedKey = "\"";
  quotedKey += key;
  quotedKey += "\"";

  const int keyIndex = body.indexOf(quotedKey);
  if (keyIndex < 0) {
    return false;
  }

  const int colonIndex = body.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return false;
  }

  const int valueStart = body.indexOf('"', colonIndex + 1);
  if (valueStart < 0) {
    return false;
  }

  const int valueEnd = body.indexOf('"', valueStart + 1);
  if (valueEnd < 0) {
    return false;
  }

  value = body.substring(valueStart + 1, valueEnd);
  value.trim();
  return true;
}

void SnapclientMode::prepareForRestart() {
  if (restartPrepared_) {
    return;
  }

  restartPrepared_ = true;
  flushPendingControlSaves(true);
  logDiagnosticSnapshot("prepare-restart");
  stopSnapClientTask(app_config::SNAPCLIENT_TASK_STOP_TIMEOUT_MS);
  snapProcessor_->end();
  audioOutput_.muteForRestart(app_config::AUDIO_MODE_CHANGE_MUTE_RAMP_MS);
}

void SnapclientMode::logDiagnosticSnapshot(const char *reason) {
  const uint32_t largestDmaBlock =
      heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  const int wifiStatus = static_cast<int>(WiFi.status());
  const long rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  const char *taskState =
      snapTaskHandle_ != nullptr ? (snapTaskRunning_ ? "running" : "stopping")
                                 : "stopped";

  Serial.printf(
      "[diag] reason=%s uptime=%lu wifi=%d rssi=%ld heap=%lu minheap=%lu psram=%lu dma_largest=%lu snap_task=%s\n",
      reason != nullptr ? reason : "-",
      static_cast<unsigned long>(millis()),
      wifiStatus,
      rssi,
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getMinFreeHeap()),
      static_cast<unsigned long>(ESP.getFreePsram()),
      static_cast<unsigned long>(largestDmaBlock),
      taskState);

  snapProcessor_->logRuntime(reason, true);
}

void SnapclientMode::recoverFromOutputStall() {
  if (otaUpdateInProgress_ || restartPrepared_ || snapProcessor_ == nullptr) {
    return;
  }

  if (!snapProcessor_->isOutputTimedOut(
          app_config::SNAPCLIENT_OUTPUT_STALL_TIMEOUT_MS)) {
    return;
  }

  if (!snapProcessor_->hasBufferedAudio() &&
      !snapProcessor_->inputActiveRecently(
          app_config::SNAPCLIENT_INPUT_RECENT_TIMEOUT_MS)) {
    return;
  }

  Serial.println("[snapclient] output stalled with active stream data; restarting");
  logDiagnosticSnapshot("snap-output-stall");
  prepareForRestart();
  delay(app_config::RESTART_DELAY_MS);
  ESP.restart();
}

bool SnapclientMode::startSnapClientTask() {
  snapTaskRunning_ = true;
  snapTaskHandle_ = nullptr;

  const BaseType_t result =
      xTaskCreatePinnedToCore(snapClientTaskEntry,
                              "snap-loop",
                              app_config::SNAPCLIENT_TASK_STACK_WORDS,
                              this,
                              app_config::SNAPCLIENT_TASK_PRIORITY,
                              &snapTaskHandle_,
                              app_config::SNAPCLIENT_TASK_CORE);

  if (result == pdPASS) {
    return true;
  }

  snapTaskRunning_ = false;
  snapTaskHandle_ = nullptr;
  Serial.printf("[snapclient] task creation failed result=%ld\n",
                static_cast<long>(result));
  logDiagnosticSnapshot("snap-task-create-failed");
  snapProcessor_->end();
  return false;
}

void SnapclientMode::stopSnapClientTask(uint32_t timeoutMs) {
  if (!snapTaskRunning_ && snapTaskHandle_ == nullptr) {
    return;
  }

  Serial.println("[snapclient] stopping snap loop task");
  snapTaskRunning_ = false;

  const uint32_t startMs = millis();
  while (snapTaskHandle_ != nullptr && (millis() - startMs) < timeoutMs) {
    delay(10);
  }

  if (snapTaskHandle_ != nullptr) {
    Serial.printf("[snapclient] snap loop task stop timeout after %lums\n",
                  static_cast<unsigned long>(millis() - startMs));
    return;
  }

  Serial.printf("[snapclient] snap loop task stopped in %lums\n",
                static_cast<unsigned long>(millis() - startMs));
}

void SnapclientMode::snapClientTaskEntry(void *context) {
  auto *self = static_cast<SnapclientMode *>(context);
  if (self != nullptr) {
    self->snapClientTaskLoop();
  }
  vTaskDelete(nullptr);
}

void SnapclientMode::snapClientTaskLoop() {
  while (snapTaskRunning_) {
    if (WiFi.status() == WL_CONNECTED) {
      snapClient_.doLoop();
    } else {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (app_config::SNAPCLIENT_TASK_DELAY_MS > 0) {
      vTaskDelay(pdMS_TO_TICKS(app_config::SNAPCLIENT_TASK_DELAY_MS));
    }
  }
  snapTaskHandle_ = nullptr;
}

bool SnapclientMode::connectWifiWithTimeout() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setHostname(app_config::SNAP_HOST_NAME);
  Serial.printf("[wifi] ssid=%s timeout=%lums\n",
                app_config::SNAP_WIFI_SSID,
                static_cast<unsigned long>(
                    app_config::SNAP_WIFI_CONNECT_TIMEOUT_MS));
  WiFi.begin(app_config::SNAP_WIFI_SSID, app_config::SNAP_WIFI_PASSWORD);

  const uint32_t startMs = millis();
  uint32_t lastLedToggleMs = startMs;
  bool wifiLedOn = false;
  writeWifiStatusLed(wifiLedOn);
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - startMs) < app_config::SNAP_WIFI_CONNECT_TIMEOUT_MS) {
    delay(app_config::SNAP_WIFI_RETRY_DELAY_MS);
    const uint32_t nowMs = millis();
    if (nowMs - lastLedToggleMs >= app_config::MODE_LED_WIFI_BLINK_INTERVAL_MS) {
      lastLedToggleMs = nowMs;
      wifiLedOn = !wifiLedOn;
      writeWifiStatusLed(wifiLedOn);
    }
    Serial.print('.');
  }

  Serial.println();
  writeWifiStatusLed(WiFi.status() == WL_CONNECTED);
  return WiFi.status() == WL_CONNECTED;
}

void SnapclientMode::handleWifiStartupRetry() {
  if (!wifiStartupFailed_) {
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastWifiStartupRetryMs_ <
      app_config::SNAP_WIFI_STARTUP_RETRY_INTERVAL_MS) {
    return;
  }

  lastWifiStartupRetryMs_ = nowMs;
  Serial.println("[wifi] retrying startup connection...");
  if (!connectWifiWithTimeout()) {
    Serial.println("[wifi] retry failed");
    logWifiFailureDiagnostics();
    return;
  }

  Serial.println("[wifi] retry connected, starting Snapclient services");
  if (!startSnapclientServices()) {
    Serial.println("[snapclient] startup after Wi-Fi retry failed, restarting...");
    delay(app_config::RESTART_DELAY_MS);
    ESP.restart();
  }
}

void SnapclientMode::logWifiFailureDiagnostics() {
  const wl_status_t status = WiFi.status();
  Serial.printf("[wifi] status=%d ssid=%s\n",
                static_cast<int>(status),
                app_config::SNAP_WIFI_SSID);

  const int networkCount = WiFi.scanNetworks();
  if (networkCount < 0) {
    Serial.printf("[wifi] scan failed result=%d\n", networkCount);
    return;
  }

  bool targetSeen = false;
  int bestRssi = -1000;
  for (int i = 0; i < networkCount; ++i) {
    if (WiFi.SSID(i) == app_config::SNAP_WIFI_SSID) {
      targetSeen = true;
      bestRssi = max(bestRssi, WiFi.RSSI(i));
    }
  }

  Serial.printf("[wifi] scan networks=%d target_ssid_seen=%s",
                networkCount,
                targetSeen ? "true" : "false");
  if (targetSeen) {
    Serial.printf(" best_rssi=%d", bestRssi);
  }
  Serial.println();
  WiFi.scanDelete();
}

void SnapclientMode::beginControlApi() {
  static const char *kCollectedHeaders[] = {
      "Content-Type",
      "X-Firmware-Filename",
  };
  controlServer_.collectHeaders(kCollectedHeaders,
                                sizeof(kCollectedHeaders) /
                                    sizeof(kCollectedHeaders[0]));

  controlServer_.on("/api/status", HTTP_GET, [this]() { sendControlStatus(); });
  controlServer_.on("/api/status", HTTP_OPTIONS, [this]() { sendControlApiOptions(); });
  controlServer_.on("/api/channel-mode", HTTP_POST, [this]() { handleSetChannelMode(); });
  controlServer_.on("/api/channel-mode", HTTP_OPTIONS, [this]() { sendControlApiOptions(); });
  controlServer_.on("/api/bluetooth-name", HTTP_POST, [this]() { handleSetBluetoothName(); });
  controlServer_.on("/api/bluetooth-name", HTTP_OPTIONS, [this]() { sendControlApiOptions(); });
  controlServer_.on("/api/power-source", HTTP_POST, [this]() { handleSetPowerSource(); });
  controlServer_.on("/api/power-source", HTTP_OPTIONS, [this]() { sendControlApiOptions(); });
  controlServer_.on("/api/firmware", HTTP_OPTIONS, [this]() { sendControlApiOptions(); });
  controlServer_.on(
      "/api/firmware",
      HTTP_POST,
      [this]() { handleFirmwareUploadComplete(); },
      [this]() { handleFirmwareUploadRaw(); });
  controlServer_.onNotFound([this]() {
    if (controlServer_.method() == HTTP_OPTIONS) {
      sendControlApiOptions();
      return;
    }
    sendControlJson(404, "{\"error\":\"not_found\"}");
  });
  controlServer_.begin();

  Serial.printf("[control] api=http://%s:%u/api/status\n",
                WiFi.localIP().toString().c_str(),
                app_config::CONTROL_API_PORT);
}

void SnapclientMode::handleControlApi() {
  controlServer_.handleClient();
}

void SnapclientMode::addControlApiCorsHeaders() {
  controlServer_.sendHeader("Access-Control-Allow-Origin", "*");
  controlServer_.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  controlServer_.sendHeader("Access-Control-Allow-Headers",
                            "Content-Type,X-Firmware-Filename");
  controlServer_.sendHeader("Access-Control-Max-Age", "600");
  controlServer_.sendHeader("Connection", "close");
}

void SnapclientMode::sendControlApiOptions() {
  addControlApiCorsHeaders();
  controlServer_.send(204);
  delay(0);
}

void SnapclientMode::sendControlJson(int statusCode, const String &body) {
  addControlApiCorsHeaders();
  controlServer_.send(statusCode, "application/json", body);
  delay(0);
}

void SnapclientMode::sendControlJson(int statusCode, const char *body) {
  addControlApiCorsHeaders();
  controlServer_.send(statusCode, "application/json", body);
  delay(0);
}

void SnapclientMode::sendControlStatus() {
  if (powerSource_ == app_config::PowerSource::Battery) {
    batteryMonitor_.update();
  }
  const BatteryStatus battery =
      powerSource_ == app_config::PowerSource::Battery ? batteryMonitor_.status()
                                                       : BatteryStatus{};
  const size_t partitionSize = otaPartitionSize();
  const bool otaSupported =
      app_config::OTA_FIRMWARE_UPDATE_ENABLED && partitionSize > 0;

  String response = "{";
  response.reserve(kStatusJsonReserveBytes);
  response += "\"project\":\"";
  response += app_config::PROJECT_TITLE;
  response += "\",\"version\":\"";
  response += app_config::FIRMWARE_VERSION;
  response += "\",\"firmwareVersion\":\"";
  response += app_config::FIRMWARE_VERSION;
  response += "\",\"board\":\"";
  response += app_config::TARGET_MODULE;
  response += "\",\"flash_size_mb\":";
  response += ESP.getFlashChipSize() / (1024 * 1024);
  response += ",\"ota_partition_size\":";
  response += partitionSize;
  response += ",\"ota_supported\":";
  response += otaSupported ? "true" : "false";
  response += ",\"update_in_progress\":";
  response += otaUpdateInProgress_ ? "true" : "false";
  response += ",\"runtime_mode\":\"snapclient\"";
  response += ",\"power_source\":\"";
  response += app_config::powerSourceName(powerSource_);
  response += "\"";
  response += ",\"channel_mode\":\"";
  response += app_config::channelModeName(audioOutput_.channelMode());
  response += "\",\"bluetooth_name\":\"";
  response += bluetoothName_;
  response += "\",\"battery\":{";
  response += "\"available\":";
  response += battery.available ? "true" : "false";
  if (battery.available) {
    response += ",\"voltage\":";
    response += String(battery.voltage, 2);
    response += ",\"percent\":";
    response += battery.percent;
    response += ",\"current\":";
    response += String(battery.current, 3);
    response += ",\"power\":";
    response += String(battery.power, 2);
  }
  response += "}";
  response += ",\"capabilities\":{\"channel_modes\":[\"stereo\",\"left\",\"right\"],\"bluetooth_name\":true,\"power_source\":true,\"battery_telemetry\":true,\"snapclient_dsp\":false,\"snapclient_dsp_update\":false,\"firmware_update\":";
  response += otaSupported ? "true" : "false";
  response += "}";
  response += "}";

  sendControlJson(200, response);
}

void SnapclientMode::schedulePendingControlSave() {
  controlSaveDueMs_ =
      millis() + app_config::SNAPCLIENT_CONTROL_SAVE_DEBOUNCE_MS;
}

void SnapclientMode::flushPendingControlSaves(bool force) {
  if (!pendingPowerSourceSave_ && !pendingBluetoothNameSave_) {
    return;
  }

  if (!force && static_cast<int32_t>(millis() - controlSaveDueMs_) < 0) {
    return;
  }

  if (pendingPowerSourceSave_) {
    pendingPowerSourceSave_ = false;
    savePowerSourcePreference(pendingPowerSource_);
  }

  if (pendingBluetoothNameSave_) {
    pendingBluetoothNameSave_ = false;
    saveBluetoothNamePreference(pendingBluetoothName_);
  }
}

void SnapclientMode::handleSetChannelMode() {
  const String body = controlServer_.arg("plain");
  app_config::ChannelMode requestedMode = app_config::ChannelMode::Stereo;

  if (!extractChannelMode(body, requestedMode)) {
    sendControlJson(
        400,
        "{\"error\":\"invalid_channel_mode\",\"allowed\":[\"stereo\",\"left\",\"right\"]}");
    return;
  }

  const app_config::ChannelMode currentMode = audioOutput_.channelMode();
  audioOutput_.setChannelMode(requestedMode);
  if (requestedMode != currentMode) {
    saveChannelModePreference(requestedMode);
  }
  sendControlStatus();
}

void SnapclientMode::handleSetBluetoothName() {
  const String body = controlServer_.arg("plain");
  String requestedName;

  if (!extractJsonStringValue(body, "bluetooth_name", requestedName) ||
      !isValidBluetoothName(requestedName)) {
    sendControlJson(
        400,
        "{\"error\":\"invalid_bluetooth_name\",\"max_length\":31}");
    return;
  }

  if (requestedName != bluetoothName_) {
    bluetoothName_ = requestedName;
    pendingBluetoothName_ = requestedName;
    pendingBluetoothNameSave_ = true;
    schedulePendingControlSave();
    Serial.printf("[bluetooth] queued device name=%s\n", requestedName.c_str());
  }
  sendControlStatus();
}

void SnapclientMode::handleSetPowerSource() {
  const String body = controlServer_.arg("plain");
  app_config::PowerSource requestedSource = app_config::PowerSource::Battery;

  if (!extractPowerSource(body, requestedSource)) {
    sendControlJson(
        400,
        "{\"error\":\"invalid_power_source\",\"allowed\":[\"battery\",\"mains\"]}");
    return;
  }

  const bool wasBattery = powerSource_ == app_config::PowerSource::Battery;
  const app_config::PowerSource previousSource = powerSource_;
  powerSource_ = requestedSource;
  if (powerSource_ != previousSource) {
    pendingPowerSource_ = powerSource_;
    pendingPowerSourceSave_ = true;
    schedulePendingControlSave();
    Serial.printf("[power] queued source=%s\n",
                  app_config::powerSourceName(powerSource_));
  }

  if (!wasBattery && powerSource_ == app_config::PowerSource::Battery) {
    batteryMonitor_.begin();
    batteryMonitor_.update(true);
  }

  sendControlStatus();
}

void SnapclientMode::handleFirmwareUploadRaw() {
  HTTPRaw &upload = controlServer_.raw();

  switch (upload.status) {
    case RAW_START: {
      if (otaUpdateInProgress_) {
        failFirmwareUpload(
            409, "update_in_progress", "Firmware update already in progress");
        return;
      }

      otaUpdateInProgress_ = true;
      otaUpdateAccepted_ = false;
      otaUpdateFailed_ = false;
      otaExpectedSize_ = 0;
      otaWritten_ = 0;
      otaPartitionSize_ = 0;
      otaResponseStatus_ = 202;
      otaError_ = "";
      otaMessage_ = "";

      if (!otaFirmwareUpdateSupported()) {
        failFirmwareUpload(
            409, "ota_unavailable", "OTA firmware update is unavailable");
        return;
      }

      const int contentLength = controlServer_.clientContentLength();
      if (contentLength <= 0) {
        failFirmwareUpload(
            400, "invalid_content_length", "Content-Length is required");
        return;
      }

      const String contentType = controlServer_.header("Content-Type");
      if (!contentType.equalsIgnoreCase("application/octet-stream")) {
        failFirmwareUpload(400,
                           "unsupported_content_type",
                           "Content-Type must be application/octet-stream");
        return;
      }

      otaExpectedSize_ = static_cast<size_t>(contentLength);
      otaPartitionSize_ = otaPartitionSize();
      if (otaExpectedSize_ + app_config::OTA_PARTITION_HEADROOM_BYTES >
          otaPartitionSize_) {
        failFirmwareUpload(413,
                           "image_too_large",
                           "Firmware image is larger than the OTA partition");
        return;
      }

      Update.clearError();
      if (!Update.begin(otaExpectedSize_, U_FLASH)) {
        failFirmwareUpload(500, "ota_begin_failed", Update.errorString());
        return;
      }

      const String filename = controlServer_.header("X-Firmware-Filename");
      Serial.printf(
          "[ota] upload start size=%lu partition=%lu filename=%s\n",
          static_cast<unsigned long>(otaExpectedSize_),
          static_cast<unsigned long>(otaPartitionSize_),
          filename.length() > 0 ? filename.c_str() : "-");
      quiesceAudioForOta();
      break;
    }

    case RAW_WRITE: {
      if (!otaUpdateInProgress_ || otaUpdateFailed_) {
        return;
      }

      if (upload.currentSize == 0) {
        return;
      }

      if (otaWritten_ == 0 && upload.buf[0] != kEspImageHeaderMagic) {
        failFirmwareUpload(400,
                           "invalid_image",
                           "Firmware image has an invalid ESP header");
        return;
      }

      if (otaWritten_ + upload.currentSize > otaExpectedSize_) {
        failFirmwareUpload(
            400, "invalid_content_length", "Uploaded body exceeded Content-Length");
        return;
      }

      const size_t written = Update.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        failFirmwareUpload(500, "ota_write_failed", Update.errorString());
        return;
      }

      otaWritten_ += written;
      break;
    }

    case RAW_END: {
      if (otaUpdateFailed_) {
        return;
      }

      if (!otaUpdateInProgress_) {
        failFirmwareUpload(500, "ota_state_error", "Firmware update was not active");
        return;
      }

      if (otaWritten_ != otaExpectedSize_) {
        failFirmwareUpload(400,
                           "truncated_upload",
                           "Uploaded body did not match Content-Length");
        return;
      }

      if (!Update.end(false)) {
        failFirmwareUpload(500, "ota_validation_failed", Update.errorString());
        return;
      }

      otaUpdateInProgress_ = false;
      otaUpdateAccepted_ = true;
      otaUpdateFailed_ = false;
      otaResponseStatus_ = 202;
      otaMessage_ = "Firmware accepted. Rebooting.";
      Serial.printf("[ota] upload accepted bytes=%lu\n",
                    static_cast<unsigned long>(otaWritten_));
      break;
    }

    case RAW_ABORTED:
      failFirmwareUpload(400, "upload_aborted", "Firmware upload was aborted");
      break;
  }
}

void SnapclientMode::handleFirmwareUploadComplete() {
  if (otaUpdateAccepted_) {
    sendControlJson(
        otaResponseStatus_,
        "{\"ok\":true,\"message\":\"Firmware accepted. Rebooting.\"}");
    scheduleFirmwareRestart();
    return;
  }

  if (!otaUpdateFailed_) {
    failFirmwareUpload(500,
                       "ota_incomplete",
                       "Firmware upload did not complete cleanly");
  }

  String response = "{\"ok\":false,\"error\":\"";
  response += otaError_;
  response += "\",\"message\":\"";
  response += otaMessage_;
  response += "\"}";
  sendControlJson(otaResponseStatus_, response);
}

void SnapclientMode::failFirmwareUpload(int statusCode,
                                        const char *error,
                                        const char *message) {
  if (Update.isRunning()) {
    Update.abort();
  }

  otaUpdateInProgress_ = false;
  otaUpdateAccepted_ = false;
  otaUpdateFailed_ = true;
  otaResponseStatus_ = statusCode;
  otaError_ = error != nullptr ? error : "ota_failed";
  otaMessage_ = message != nullptr ? message : "Firmware update failed";

  Serial.printf("[ota] failed status=%d error=%s message=%s written=%lu/%lu\n",
                otaResponseStatus_,
                otaError_.c_str(),
                otaMessage_.c_str(),
                static_cast<unsigned long>(otaWritten_),
                static_cast<unsigned long>(otaExpectedSize_));

  if (restartPrepared_) {
    // Audio was already quiesced for the flash, so the device is silent and the
    // snap tasks are stopped. The update did not commit, so the bootloader keeps
    // the current firmware - reboot to recover playback cleanly.
    Serial.println("[ota] audio was stopped for flashing; scheduling recovery reboot");
    scheduleFirmwareRestart();
  }
}

void SnapclientMode::scheduleFirmwareRestart() {
  if (otaRebootPending_) {
    return;
  }

  otaRebootPending_ = true;
  otaRestartAtMs_ = millis() + app_config::OTA_REBOOT_DELAY_MS;
}

void SnapclientMode::quiesceAudioForOta() {
  if (restartPrepared_) {
    return;
  }

  // Fade the music out, then stop the decode/network tasks and flush I2S so the
  // flash write and HTTP upload get an idle device. The decode task applies the
  // fade as it keeps writing; the delay lets it ramp down and the DMA drain
  // before the tasks are stopped, so the stop is click-free.
  Serial.println("[ota] fading out audio before flashing");
  pcmProbe_.beginFadeOut(app_config::OTA_AUDIO_FADE_MS);
  delay(app_config::OTA_AUDIO_QUIESCE_DELAY_MS);
  prepareForRestart();
  Serial.println("[ota] audio stopped; receiving firmware on an idle device");
}
