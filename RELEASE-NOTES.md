# Release Notes - ESP32 Audio Client v2.4.0

Release date: 2026-07-22

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- INA236 power monitor with `20 mΩ` shunt
- Opus Snapserver stream

## [2.4.0]

- Replaced the GPIO34 resistor-divider/ADC battery measurement with an INA236 on I2C (`SDA GPIO21`, `SCL GPIO19`).
- Added pack current and power telemetry while retaining the existing 4S voltage-to-percentage lookup curve.
- Added `battery.current` in amperes, `battery.power` in watts, and `capabilities.battery_telemetry: true` to `GET /api/status`.
- Detects all INA236A and INA236B A0 address combinations and verifies the TI manufacturer and INA236 device IDs before reporting data.
- Configured the `20 mΩ` shunt for a ±4.096 A current range with 64-sample hardware averaging.

## Summary

This minor release updates the firmware for the revised battery-power hardware.
Companion apps can now show pack voltage, estimated 4S charge percentage,
signed current draw, and power usage from the local status API. Existing
`battery.voltage`, `battery.percent`, and mains-mode hiding behavior remain
compatible.

Current is positive when load current flows from the INA236 `IN+` side toward
`IN-`. With the configured `20 mΩ` shunt and ±81.92 mV INA236 range, measurable
current is approximately ±4.096 A.

Bluetooth output and OTA upload behavior are unchanged.

## Snapserver Profile

Use a 44.1 kHz stereo PCM FIFO input from librespot, encoded as Opus for
transport. Keep a generous `buffer` - it is the master jitter cushion:

```ini
[stream]
buffer = 2000
source = pipe:///tmp/snapfifo_spotify?name=Spotify&sampleformat=44100:16:2&codec=opus&chunk_ms=20
```

Restart Snapserver after changing the config:

```bash
sudo systemctl restart snapserver
```

## Firmware Version

- Previous version: `2.2.1`
- New version: `2.4.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.4.0`
- `firmwareVersion`: `2.4.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2034880` bytes

## Manual Test Checklist

- Flash `2.4.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.4.0` and detects an INA236 address on SDA `GPIO21` / SCL `GPIO19` with a `0.020 ohm` shunt.
- Confirm `GET /api/status` reports `firmwareVersion` as `2.4.0` and `capabilities.battery_telemetry: true`.
- With `power_source` set to `battery`, confirm `battery.available: true` and plausible `voltage`, `percent`, `current`, and `power` values.
- Apply a known load and verify positive current flows from INA236 `IN+` toward `IN-`; compare current and power against a reference meter.
- Remove or disconnect the INA236 and confirm `battery.available: false` without affecting audio or the rest of the control API.
- Set `power_source` to `mains` and confirm battery telemetry is suppressed with `battery.available: false`.
- Confirm the low-battery LED still activates at or below 20% based on the INA236 pack voltage.
- Confirm Snapclient playback and channel routing continue normally.
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- Confirm Bluetooth mode still plays normally and is unchanged.
