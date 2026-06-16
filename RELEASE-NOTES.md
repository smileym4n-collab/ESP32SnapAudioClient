# Release Notes - ESP32 Audio Client v1.3.2

Release date: 2026-06-16

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream
- One-off board variant with mode button on `GPIO34`

## [1.3.2]

- Moved the runtime mode-toggle button from `GPIO23` to `GPIO34` for a one-off board variant.
- Configured the mode button pin as plain `INPUT`; classic ESP32 `GPIO34` has no internal pull-up/down, so the board must provide external biasing.
- Disabled battery sensing because `GPIO34` is used by the mode button on this variant.
- Updated visible firmware version fields for `1.3.2`.

## Important Hardware Note

`GPIO34` is input-only and does not support the ESP32's internal pull-up or pull-down resistors. This firmware expects an active-low button on `GPIO34` with an external pull-up. Without an external pull-up, the input can float and mode switching will be unreliable.

Battery reporting is disabled in this build because the normal battery sense input also used `GPIO34`.

## Snapserver Profile

Use a 44.1 kHz stereo PCM FIFO input from librespot, encoded as Opus for transport:

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

- Previous version: `1.3.1`
- New version: `1.3.2`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `1.3.2`
- `firmwareVersion`: `1.3.2`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2,013,424` bytes

## Manual Test Checklist

- Flash `1.3.2` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 1.3.2` and `GET /api/status` reports `firmwareVersion` as `1.3.2`.
- Confirm the boot log reports the mode button on `GPIO34`.
- Confirm the boot log reports battery sensing as disabled.
- Confirm the externally biased `GPIO34` button toggles between Snapclient and Bluetooth modes.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus`.
- Confirm Snapclient audio playback still works.
- Confirm Bluetooth mode still plays normally.
