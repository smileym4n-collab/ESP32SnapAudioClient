# Release Notes - ESP32 Audio Client v2.1.2

Release date: 2026-06-19

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [2.1.2]

- Live Snapclient EQ/DSP updates are now hardened against invalid numeric values from API requests or saved preferences.
- Generated biquad filter coefficients are validated before use; invalid filter math is bypassed instead of being allowed into the audio stream.
- The Snapclient PCM DSP work buffer is reserved during stream startup to avoid a first-use heap allocation while audio is running.
- Snapclient now recovers from an output stall: if output writes stop while stream data is still buffered or recently arriving, it logs diagnostics and restarts instead of remaining silent until a manual reboot or mode switch.

## Summary

This patch release targets crashes or lockups seen when applying EQ while
Snapclient playback is running, and also adds conservative recovery for normal
playback stalls where stream data is active but output writes have stopped.

Bluetooth output remains unchanged and does not use the Snapclient DSP path.

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

- Previous version: `2.1.1`
- New version: `2.1.2`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.1.2`
- `firmwareVersion`: `2.1.2`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2037184` bytes (well within the OTA slot limit)

## Manual Test Checklist

- Flash `2.1.2` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.1.2` and `GET /api/status` reports `firmwareVersion` as `2.1.2`.
- Confirm `GET /api/status` includes `dsp`, `capabilities.snapclient_dsp: true`, and `capabilities.snapclient_dsp_update: true`.
- Confirm `GET /api/dsp` returns the same DSP object shape as `/api/status`.
- With Snapclient playback running, send a partial `POST /api/dsp` changing `eq_profile`, `bass_boost_db`, `balance`, and `loudness.enabled`; confirm audio continues while the response returns clamped updated values.
- Leave Snapclient playback running long enough to cover prior random-dropout timing; if output stalls, confirm the serial log reports `snap-output-stall` and the device restarts rather than staying silent.
- From the web app, change DSP and volume-adjacent controls and confirm the browser no longer reports a port `8080` response failure.
- Reboot and confirm the changed DSP settings persist.
- Send `POST /api/dsp/reset` and confirm settings return to firmware defaults.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` (e.g. `2000`).
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- **Channel routing:** with audio playing, POST `stereo` / `left` / `right` and confirm the output audibly switches with no reboot and survives a reboot.
- Confirm Bluetooth mode still plays normally and is unchanged (stereo, no routing).
- Confirm playback through the I2S DAC works with no MCLK line connected.
