#include "battery_monitor.h"

#include <Wire.h>

#include "board_config.h"
#include "snapclient_config.h"

namespace {

struct BatteryPoint {
  float voltage;
  int percent;
};

static const BatteryPoint kBatteryCurve4s[] = {
    {16.80f, 100},
    {16.60f, 98},
    {16.40f, 95},
    {16.20f, 90},
    {16.00f, 85},
    {15.80f, 78},
    {15.60f, 70},
    {15.40f, 62},
    {15.20f, 54},
    {15.00f, 46},
    {14.80f, 38},
    {14.60f, 30},
    {14.40f, 22},
    {14.20f, 15},
    {14.00f, 10},
    {13.80f, 6},
    {13.60f, 3},
    {13.20f, 1},
    {12.00f, 0},
};

static constexpr size_t kBatteryCurvePoints =
    sizeof(kBatteryCurve4s) / sizeof(kBatteryCurve4s[0]);

static constexpr uint8_t kConfigurationRegister = 0x00;
static constexpr uint8_t kBusVoltageRegister = 0x02;
static constexpr uint8_t kPowerRegister = 0x03;
static constexpr uint8_t kCurrentRegister = 0x04;
static constexpr uint8_t kCalibrationRegister = 0x05;
static constexpr uint8_t kManufacturerIdRegister = 0x3E;
static constexpr uint8_t kDeviceIdRegister = 0x3F;
static constexpr uint16_t kTiManufacturerId = 0x5449;
static constexpr uint16_t kIna236DeviceId = 0xA080;

// ADCRANGE=0, average 64 samples, 1.1 ms bus and shunt conversions,
// continuous shunt + bus measurement. Reserved bits retain their reset value.
static constexpr uint16_t kConfiguration = 0x4727;
static constexpr float kBusVoltageLsbVolts = 0.0016f;
static constexpr float kPowerLsbMultiplier = 32.0f;

static const uint8_t kIna236Addresses[] = {
    0x40, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4A, 0x4B,
};

}  // namespace

bool BatteryMonitor::begin() {
  enabled_ = false;
  initialized_ = false;

  if (!board_config::BATTERY_MONITOR_ENABLED) {
    return false;
  }

  Wire.begin(board_config::BATTERY_I2C_SDA_PIN,
             board_config::BATTERY_I2C_SCL_PIN);
  Wire.setClock(app_config::BATTERY_I2C_FREQUENCY_HZ);

  if (!detectDevice()) {
    Serial.println("[battery] INA236 not found on I2C bus");
    return false;
  }

  const float calibration =
      0.00512f / (app_config::BATTERY_CURRENT_LSB_AMPS *
                  app_config::BATTERY_SHUNT_RESISTANCE_OHMS);
  const uint16_t calibrationRegister =
      static_cast<uint16_t>(calibration + 0.5f);
  if (!writeRegister(kConfigurationRegister, kConfiguration) ||
      !writeRegister(kCalibrationRegister, calibrationRegister)) {
    Serial.println("[battery] failed to configure INA236");
    return false;
  }

  enabled_ = true;
  lastPollMs_ = 0;

  Serial.printf(
      "[battery] INA236 address=0x%02X sda=GPIO%d scl=GPIO%d shunt=%.3f ohm calibration=%u\n",
      i2cAddress_,
      board_config::BATTERY_I2C_SDA_PIN,
      board_config::BATTERY_I2C_SCL_PIN,
      app_config::BATTERY_SHUNT_RESISTANCE_OHMS,
      calibrationRegister);
  return true;
}

void BatteryMonitor::update(bool force) {
  if (!enabled_) {
    return;
  }

  const uint32_t nowMs = millis();
  if (!force && lastPollMs_ != 0 &&
      (nowMs - lastPollMs_) < app_config::BATTERY_POLL_INTERVAL_MS) {
    return;
  }
  lastPollMs_ = nowMs;

  uint16_t rawBusVoltage = 0;
  uint16_t rawCurrent = 0;
  uint16_t rawPower = 0;
  if (!readRegister(kBusVoltageRegister, rawBusVoltage) ||
      !readRegister(kCurrentRegister, rawCurrent) ||
      !readRegister(kPowerRegister, rawPower)) {
    initialized_ = false;
    Serial.println("[battery] INA236 read failed");
    return;
  }

  packVoltage_ = static_cast<float>(rawBusVoltage & 0x7FFF) *
                 kBusVoltageLsbVolts;
  currentAmps_ = static_cast<float>(static_cast<int16_t>(rawCurrent)) *
                 app_config::BATTERY_CURRENT_LSB_AMPS;
  powerWatts_ = static_cast<float>(rawPower) * kPowerLsbMultiplier *
                app_config::BATTERY_CURRENT_LSB_AMPS;
  const int instantPercent = percentFromVoltage(packVoltage_);

  if (!initialized_) {
    filteredPercent_ = static_cast<float>(instantPercent);
    initialized_ = true;
  } else {
    filteredPercent_ += app_config::BATTERY_PERCENT_SMOOTH_ALPHA *
                        (static_cast<float>(instantPercent) - filteredPercent_);
  }

  percent_ = constrain(static_cast<int>(filteredPercent_ + 0.5f), 0, 100);
}

BatteryStatus BatteryMonitor::status() const {
  BatteryStatus current;
  current.available = enabled_ && initialized_;
  current.voltage = packVoltage_;
  current.current = currentAmps_;
  current.power = powerWatts_;
  current.percent = percent_;
  return current;
}

bool BatteryMonitor::detectDevice() {
  for (const uint8_t address : kIna236Addresses) {
    i2cAddress_ = address;
    uint16_t manufacturerId = 0;
    uint16_t deviceId = 0;
    if (readRegister(kManufacturerIdRegister, manufacturerId) &&
        readRegister(kDeviceIdRegister, deviceId) &&
        manufacturerId == kTiManufacturerId &&
        deviceId == kIna236DeviceId) {
      return true;
    }
  }
  i2cAddress_ = 0;
  return false;
}

bool BatteryMonitor::writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(i2cAddress_);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>(value >> 8));
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  return Wire.endTransmission() == 0;
}

bool BatteryMonitor::readRegister(uint8_t reg, uint16_t &value) const {
  Wire.beginTransmission(i2cAddress_);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(i2cAddress_, static_cast<uint8_t>(2)) != 2) {
    return false;
  }

  value = static_cast<uint16_t>(Wire.read()) << 8;
  value |= static_cast<uint16_t>(Wire.read());
  return true;
}

int BatteryMonitor::percentFromVoltage(float packVoltage) const {
  if (packVoltage >= kBatteryCurve4s[0].voltage) {
    return 100;
  }
  if (packVoltage <= kBatteryCurve4s[kBatteryCurvePoints - 1].voltage) {
    return 0;
  }

  for (size_t i = 0; i < (kBatteryCurvePoints - 1); ++i) {
    const BatteryPoint &high = kBatteryCurve4s[i];
    const BatteryPoint &low = kBatteryCurve4s[i + 1];

    if (packVoltage <= high.voltage && packVoltage >= low.voltage) {
      const float spanVoltage = high.voltage - low.voltage;
      if (spanVoltage <= 0.0f) {
        return constrain(high.percent, 0, 100);
      }
      const float t = (packVoltage - low.voltage) / spanVoltage;
      const float interpolated =
          static_cast<float>(low.percent) +
          t * static_cast<float>(high.percent - low.percent);
      return constrain(static_cast<int>(interpolated + 0.5f), 0, 100);
    }
  }

  return 0;
}
