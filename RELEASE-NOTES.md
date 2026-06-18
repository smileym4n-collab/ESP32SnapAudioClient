# Release Notes - ESP32 Audio Client v2.0.0

Release date: 2026-06-18

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [2.0.0]

- Added a Snapclient-only EQ/DSP stage after Opus decode and channel routing: 3-band biquad EQ, per-channel gain/balance, volume-aware loudness bass boost, optional headroom trim, and a soft limiter.
- Added `/api/status` reporting for the compiled DSP settings under `dsp`, plus `capabilities.snapclient_dsp`.
- Bluetooth mode remains unchanged and does not use the Snapclient DSP path.

## Summary

This release adds the first Snapclient DSP pipeline. Decoded PCM now passes
through a compile-time EQ and gain stage before I2S output, with optional
loudness bass boost, output headroom, and a soft limiter. The status API reports
the compiled DSP settings so companion apps can show what tuning is active.

This is a major-version release because `/api/status` now exposes a new `dsp`
object and `capabilities.snapclient_dsp` capability, and the Snapclient audio
path has a meaningful new processing stage. Bluetooth output remains unchanged.

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

- Previous version: `1.3.1`
- New version: `2.0.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.0.0`
- `firmwareVersion`: `2.0.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2024672` bytes (well within the OTA slot limit)

## Manual Test Checklist

- Flash `2.0.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.0.0` and `GET /api/status` reports `firmwareVersion` as `2.0.0`.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` (e.g. `2000`).
- **DSP status:** confirm `GET /api/status` includes `dsp.enabled`, `dsp.eq`, `dsp.loudness`, `dsp.soft_limiter`, and `capabilities.snapclient_dsp: true`.
- **DSP audio path:** with Snapclient playback running, confirm audio still plays cleanly through the I2S DAC and volume changes do not produce limiter artifacts.
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- **Channel routing:** with audio playing, POST `stereo` / `left` / `right` and confirm the output audibly switches with no reboot and survives a reboot.
- Confirm Bluetooth mode still plays normally and is unchanged (stereo, no routing).
- Confirm playback through the I2S DAC works with no MCLK line connected.
