#pragma once

#include <Arduino.h>

namespace board_config {

// ---------- User-editable hardware pin assignments ----------
// Zeppelin install: the original PCM1808 remains the I2S clock master.
// Set these three pins before flashing the firmware into the speaker:
//   PCM1808 BCK  -> ESP32 I2S_BCK_IN
//   PCM1808 LRCK -> ESP32 I2S_LRCK_IN
//   ESP32 DATA   -> Zeppelin DSP input through a 22-47 ohm series resistor
// Leave MCLK/SCKI disconnected from the ESP32.
static constexpr int I2S_BCK_IN = 26;
static constexpr int I2S_LRCK_IN = 25;
static constexpr int I2S_DATA_OUT = 13;

// Backward-compatible names for existing code/docs.
static constexpr int I2S_BCLK_PIN = I2S_BCK_IN;
static constexpr int I2S_LRCLK_PIN = I2S_LRCK_IN;
static constexpr int I2S_DOUT_PIN = I2S_DATA_OUT;

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

// 4S battery monitor input. Use ADC1-capable pins while Wi-Fi is active.
// Good choices on classic ESP32 are GPIO34, GPIO35, GPIO36, and GPIO39.
static constexpr bool BATTERY_SENSE_ENABLED = true;
static constexpr int SENSE_PIN = 34;  // Battery divider output to ADC1 input
static constexpr int BATTERY_SENSE_PIN = SENSE_PIN;

}  // namespace board_config
