# Release Notes - ESP32 Audio Client v1.2.1

Release date: 2026-06-13

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [1.2.1]

- Raised the Snapclient network receive task priority above the Opus decode/output task.
- Lowered the Opus decode/output RTOS task priority from `5` to `4`.
- Removed the extra fixed 1 ms delay after each Snapclient loop pass.
- Reduced the Snapclient processor fast-loop yield from 5 ms to 1 ms.
- Updated visible firmware version fields for `1.2.1`.

## Summary

This is a follow-up Opus test build for the repeating 3-5 second play / half-second pause pattern. In `1.2.0`, the Opus decode/output task could outrank and throttle the Snapclient network receive loop. This build gives packet receive priority over decode/output and shortens the loop yield so the compressed queue can stay fed while audio is decoded.

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

- Previous version: `1.2.0`
- New version: `1.2.1`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `1.2.1`
- `firmwareVersion`: `1.2.1`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

The current `1.2.1` build output size is comfortably below the OTA slot limit:

- App slot size: `6553600` bytes
- Built firmware image: `2015616` bytes

## Manual Test Checklist

- Flash `1.2.1` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 1.2.1`.
- Confirm `GET /api/status` reports `firmwareVersion` as `1.2.1` after Wi-Fi connects.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus`.
- Confirm boot logs report `decoder=OpusAudioDecoder`.
- Confirm boot logs report Snapclient task priority `5` and output task priority `4`.
- Confirm boot logs report the Snapclient/I2S output format as 48 kHz, 16-bit, 2 channel.
- Test playback at weaker Wi-Fi locations that dropped out with PCM.
- Listen for startup silence, distortion, repeated rebuffering, pitch/speed errors, and stop/track-change behavior.
- Confirm channel routing still works.
- Confirm OTA support still reports correctly.
