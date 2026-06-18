# Release Notes - ESP32 Audio Client v2.1.1

Release date: 2026-06-18

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [2.1.1]

- The local control API now returns CORS headers and handles browser `OPTIONS` preflight requests for web companion apps.
- JSON responses from the control API now include CORS headers, including error responses and firmware-upload responses.

## Summary

This patch release fixes browser access to the local control API. Web companion
apps can now send DSP and adjacent JSON updates to port `8080` without failing on
CORS preflight or missing CORS headers in JSON responses.

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

- Previous version: `2.1.0`
- New version: `2.1.1`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.1.1`
- `firmwareVersion`: `2.1.1`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2036800` bytes (well within the OTA slot limit)

## Manual Test Checklist

- Flash `2.1.1` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.1.1` and `GET /api/status` reports `firmwareVersion` as `2.1.1`.
- Confirm `GET /api/status` includes `dsp`, `capabilities.snapclient_dsp: true`, and `capabilities.snapclient_dsp_update: true`.
- Confirm `GET /api/dsp` returns the same DSP object shape as `/api/status`.
- With Snapclient playback running, send a partial `POST /api/dsp` changing `eq_profile`, `bass_boost_db`, `balance`, and `loudness.enabled`; confirm audio continues while the response returns clamped updated values.
- From the web app, change DSP and volume-adjacent controls and confirm the browser no longer reports a port `8080` response failure.
- Reboot and confirm the changed DSP settings persist.
- Send `POST /api/dsp/reset` and confirm settings return to firmware defaults.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` (e.g. `2000`).
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- **Channel routing:** with audio playing, POST `stereo` / `left` / `right` and confirm the output audibly switches with no reboot and survives a reboot.
- Confirm Bluetooth mode still plays normally and is unchanged (stereo, no routing).
- Confirm playback through the I2S DAC works with no MCLK line connected.
