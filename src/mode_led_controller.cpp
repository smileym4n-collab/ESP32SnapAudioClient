#include "mode_led_controller.h"

#include "board_config.h"

void ModeLedController::begin() {
  pinMode(board_config::WIFI_STATUS_LED_PIN, OUTPUT);
  pinMode(board_config::BT_STATUS_LED_PIN, OUTPUT);
  writeWifiLed(false);
  writeBtLed(false);
}

void ModeLedController::setMode(app_config::OperatingMode mode) {
  activeMode_ = mode;
  bluetoothClientConnected_ = false;
  wifiConnected_ = false;
  lastToggleMs_ = millis();

  if (activeMode_ == app_config::OperatingMode::Snapclient) {
    writeWifiLed(false);
    writeBtLed(false);
  } else {
    writeWifiLed(false);
    writeBtLed(false);
  }
}

void ModeLedController::setWifiConnected(bool connected) {
  if (wifiConnected_ == connected) {
    return;
  }

  wifiConnected_ = connected;
  lastToggleMs_ = millis();

  if (activeMode_ == app_config::OperatingMode::Snapclient) {
    writeWifiLed(wifiConnected_);
  }
}

void ModeLedController::setBluetoothClientConnected(bool connected) {
  if (bluetoothClientConnected_ == connected) {
    return;
  }

  bluetoothClientConnected_ = connected;
  lastToggleMs_ = millis();

  if (activeMode_ == app_config::OperatingMode::Bluetooth) {
    writeBtLed(bluetoothClientConnected_);
  }
}

void ModeLedController::update() {
  if (activeMode_ == app_config::OperatingMode::Snapclient) {
    if (wifiConnected_) {
      writeWifiLed(true);
      return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - lastToggleMs_ <
        app_config::MODE_LED_WIFI_BLINK_INTERVAL_MS) {
      return;
    }

    lastToggleMs_ = nowMs;
    writeWifiLed(!wifiLedOn_);
    return;
  }

  if (activeMode_ != app_config::OperatingMode::Bluetooth) {
    return;
  }

  if (bluetoothClientConnected_) {
    writeBtLed(true);
    return;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastToggleMs_ < app_config::MODE_LED_BLUETOOTH_BLINK_INTERVAL_MS) {
    return;
  }

  lastToggleMs_ = nowMs;
  writeBtLed(!btLedOn_);
}

void ModeLedController::writeWifiLed(bool on) {
  wifiLedOn_ = on;
  const int level = on
                        ? (board_config::WIFI_STATUS_LED_ACTIVE_HIGH ? HIGH : LOW)
                        : (board_config::WIFI_STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(board_config::WIFI_STATUS_LED_PIN, level);
}

void ModeLedController::writeBtLed(bool on) {
  btLedOn_ = on;
  const int level = on
                        ? (board_config::BT_STATUS_LED_ACTIVE_HIGH ? HIGH : LOW)
                        : (board_config::BT_STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(board_config::BT_STATUS_LED_PIN, level);
}
