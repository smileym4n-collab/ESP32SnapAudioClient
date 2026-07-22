#pragma once

#include <Arduino.h>

namespace board_config {

// ---------- User-editable hardware pin assignments ----------
static constexpr int I2S_BCLK_PIN = 26;   // I2S bit clock to external DAC BCLK/SCK
static constexpr int I2S_LRCLK_PIN = 25;  // I2S word select / LRCLK to DAC WS
static constexpr int I2S_DOUT_PIN = 13;   // I2S serial data output to DAC DIN
// No MCLK line is used. PCM5102-style I2S DACs derive their internal clocks from
// BCLK and do not need a separate master clock.

// Runtime mode-toggle button. Default wiring is a simple momentary switch to GND.
// Cold boot always starts in Snapclient mode.
// Pressing this button while the firmware is running toggles mode and reboots.
static constexpr int BOOT_MODE_BUTTON_PIN = 23;
static constexpr bool BOOT_MODE_BUTTON_USE_PULLUP = true;
static constexpr int BOOT_MODE_BUTTON_ACTIVE_LEVEL = LOW;

// Dedicated mode-status LEDs. Default wiring uses common-anode RGB LED channels:
// 3V3 -> common anode, cathode -> resistor -> GPIO. The GPIO sinks current.
static constexpr int WIFI_STATUS_LED_PIN = 32;
static constexpr bool WIFI_STATUS_LED_ACTIVE_HIGH = false;
static constexpr int BT_STATUS_LED_PIN = 33;
static constexpr bool BT_STATUS_LED_ACTIVE_HIGH = false;
static constexpr int LOW_BATTERY_LED_PIN = 14;
static constexpr bool LOW_BATTERY_LED_ACTIVE_HIGH = false;

// Backward-compatible aliases for older code/docs that refer to the single
// status LED. The Wi-Fi LED is the normal Snapclient-mode status indicator.
static constexpr int MODE_STATUS_LED_PIN = WIFI_STATUS_LED_PIN;
static constexpr bool MODE_STATUS_LED_ACTIVE_HIGH = WIFI_STATUS_LED_ACTIVE_HIGH;

// INA236 4S pack voltage/current/power monitor.
static constexpr bool BATTERY_MONITOR_ENABLED = true;
static constexpr int BATTERY_I2C_SDA_PIN = 21;
static constexpr int BATTERY_I2C_SCL_PIN = 19;

}  // namespace board_config
