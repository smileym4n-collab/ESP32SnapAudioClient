# Release Notes - ESP32 Audio Client v1.2.0

Release date: 2026-06-13

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [1.2.0]

- Switched Snapclient mode from the project-local PCM decoder to `OpusAudioDecoder`.
- Changed the Snapclient output fallback format to 48 kHz, 16-bit, stereo to match Opus decode output.
- Kept the compressed transport queue at `131072` bytes and retuned startup/rebuffer thresholds to `20%` and `10% -> 40%`.
- Updated Snapserver documentation for `codec=opus` with a `44100:16:2` librespot FIFO input profile.
- Updated visible firmware version fields for `1.2.0`.

## Summary

This is a test build for weaker Wi-Fi conditions. PCM transport worked, but still needed a very strong ESP32 Wi-Fi signal. This build moves the Snapcast transport to Opus so the network carries compressed audio, while the ESP32 decodes locally to 48 kHz stereo PCM before writing to the same I2S DAC path.

## Snapserver Profile

Use a 44.1 kHz stereo PCM FIFO input from librespot, but encode the Snapcast transport as Opus:

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

- Previous version: `1.1.4`
- New version: `1.2.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `1.2.0`
- `firmwareVersion`: `1.2.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

The current `1.2.0` build output size is comfortably below the OTA slot limit:

- App slot size: `6553600` bytes
- Built firmware image: `2015632` bytes

## Manual Test Checklist

- Flash `1.2.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 1.2.0`.
- Confirm `GET /api/status` reports `firmwareVersion` as `1.2.0` after Wi-Fi connects.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus`.
- Confirm boot logs report `decoder=OpusAudioDecoder`.
- Confirm boot logs report the Snapclient/I2S output format as 48 kHz, 16-bit, 2 channel.
- Test playback at weaker Wi-Fi locations that dropped out with PCM.
- Listen for startup silence, distortion, repeated rebuffering, pitch/speed errors, and stop/track-change behavior.
- Confirm channel routing still works.
- Confirm OTA support still reports correctly.
