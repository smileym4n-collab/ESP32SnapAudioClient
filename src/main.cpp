/*
  Project: ESP32 audio client (SnapApp channel control API)
  Version: 2.0.0
  Framework: Arduino (PlatformIO)

  Pin map (ESP32-WROVER-IE-N16R8 -> external I2S DAC):
    GPIO26 -> I2S BCLK
    GPIO25 -> I2S LRCLK / WS
    GPIO13 -> I2S DOUT
    GPIO34 -> SENSE / battery voltage divider input
    GPIO23 -> Runtime mode-toggle button (active low with internal pull-up)
    GPIO32 -> Wi-Fi/Snapclient status LED (active low, common-anode RGB)
    GPIO33 -> Bluetooth status LED (active low, common-anode RGB)
    GPIO14 -> Low battery warning LED (active low, common-anode RGB)

  Notes:
  - Cold boot always starts in Snapclient mode.
  - Press the runtime mode button to reboot into the other mode.
  - Snapclient mode exposes a local HTTP control API on port 8080.
  - Snapclient mode blinks the Wi-Fi LED while connecting and keeps it solid once connected.
  - Bluetooth mode blinks the BT LED while waiting for a source.
  - Wi-Fi and Snapserver settings are in include/snapclient_config.h.
  - Hardware pin assignments are in include/board_config.h.
*/

#include "AudioTools/AudioLibs/MemoryManager.h"

#include "bluetooth_mode.h"
#include "boot_mode_selector.h"
#include "board_config.h"
#include "mode_led_controller.h"
#include "mode_switch_controller.h"
#include "runtime_mode.h"
#include "snapclient_config.h"
#include "snapclient_mode.h"

namespace {

audio_tools::MemoryManager gMemoryManager;
SnapclientMode gSnapclientMode;
BluetoothMode gBluetoothMode;
ModeLedController gModeLed;
ModeSwitchController gModeSwitch;
RuntimeMode *gActiveMode = nullptr;

void configurePsramAllocator() {
  if (!psramFound()) {
    Serial.println("[psram] not detected");
    return;
  }

  gMemoryManager.begin(app_config::PSRAM_ALLOC_THRESHOLD_BYTES);
  Serial.printf("[psram] size=%lu bytes, free=%lu bytes\n",
                static_cast<unsigned long>(ESP.getPsramSize()),
                static_cast<unsigned long>(ESP.getFreePsram()));
}

}  // namespace

void setup() {
  Serial.begin(app_config::SERIAL_BAUD);
  delay(200);

  gModeLed.begin();
  setCpuFrequencyMhz(app_config::CPU_FREQ_MHZ);

  Serial.printf("\n[boot] %s\n", app_config::PROJECT_TITLE);
  Serial.printf("[version] %s\n", app_config::FIRMWARE_VERSION);
  Serial.printf("[target] %s\n", app_config::TARGET_MODULE);
  Serial.printf("[cpu] %u MHz\n", getCpuFrequencyMhz());
  configurePsramAllocator();

  const auto selectedMode = detectOperatingMode();
  gActiveMode = selectedMode == app_config::OperatingMode::Snapclient
                    ? static_cast<RuntimeMode *>(&gSnapclientMode)
                    : static_cast<RuntimeMode *>(&gBluetoothMode);

  Serial.printf("[boot] selected mode=%s\n", gActiveMode->name());
  Serial.printf("[led] wifi=GPIO%d blink connecting/solid connected, bt=GPIO%d blink waiting/solid connected, low_battery=GPIO%d below %d%%\n",
                board_config::WIFI_STATUS_LED_PIN,
                board_config::BT_STATUS_LED_PIN,
                board_config::LOW_BATTERY_LED_PIN,
                app_config::BATTERY_LOW_WARNING_PERCENT);
  Serial.printf("[button] pin=%d, press while running to toggle mode and reboot\n",
                board_config::BOOT_MODE_BUTTON_PIN);
  Serial.printf("[battery] sense=%s pin=%d\n",
                board_config::BATTERY_SENSE_ENABLED ? "enabled" : "disabled",
                board_config::BATTERY_SENSE_PIN);
  Serial.println("[serial] commands: 'b' -> Bluetooth, 's' -> Snapclient, 't' -> toggle");
  gModeLed.setMode(selectedMode);
  gModeSwitch.begin(selectedMode);
  gModeSwitch.setRuntimeMode(gActiveMode);

  if (!gActiveMode->begin()) {
    Serial.println("[boot] mode start failed, restarting...");
    delay(app_config::RESTART_DELAY_MS);
    ESP.restart();
  }
}

void loop() {
  if (gActiveMode != nullptr) {
    gModeLed.setBluetoothClientConnected(gActiveMode->bluetoothClientConnected());
    gModeLed.setWifiConnected(gActiveMode->wifiConnected());
    gModeLed.setLowBatteryWarningActive(gActiveMode->lowBatteryWarningActive());
  }
  gModeLed.update();
  gModeSwitch.update();

  if (gActiveMode != nullptr) {
    gActiveMode->loop();
  } else {
    delay(100);
  }
}
