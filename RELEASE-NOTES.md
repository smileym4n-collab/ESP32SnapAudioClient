# Release Notes - ESP32 Audio Client v2.1.4

Release date: 2026-06-19

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [2.1.4]

- Persisted DSP preference writes are now debounced so rapid preset/toggle/slider changes no longer synchronously write ESP32 NVS on every HTTP request while audio is playing.
- Live DSP reconfiguration now prepares filter state before taking the audio DSP mutex, then swaps the prepared state under a short lock.
- Pending DSP settings are flushed before restart so the latest settings survive mode switches, OTA reboots, and recovery restarts.

## Summary

This patch release targets hangs and audio dropouts while toggling DSP or
changing EQ. Live changes still apply immediately, but flash persistence is
delayed until updates settle, avoiding repeated NVS writes on the playback path.
DSP filter setup is also moved outside the short audio-state lock.

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

- Previous version: `2.1.3`
- New version: `2.1.4`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.1.4`
- `firmwareVersion`: `2.1.4`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2037760` bytes (well within the OTA slot limit)

## Manual Test Checklist

- Flash `2.1.4` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.1.4` and `GET /api/status` reports `firmwareVersion` as `2.1.4`.
- Confirm `GET /api/status` includes `dsp`, `capabilities.snapclient_dsp: true`, and `capabilities.snapclient_dsp_update: true`.
- Confirm `GET /api/dsp` returns the same DSP object shape as `/api/status` with `enabled: false`, Flat profile, loudness disabled, and soft limiter disabled after OTA/reset.
- Confirm default Snapclient playback sounds like the pre-DSP baseline with no bass lift or limiter pumping.
- With Snapclient playback running, send a partial `POST /api/dsp` changing `eq_profile`, `bass_boost_db`, `balance`, and `loudness.enabled`; confirm audio continues while the response returns clamped updated values.
- While playback is running, repeatedly toggle DSP and change EQ presets/sliders from the companion app; confirm audio does not hang and settings persist after the debounce window and reboot.
- From the web app, change DSP and volume-adjacent controls and confirm the browser no longer reports a port `8080` response failure.
- Reboot and confirm the changed DSP settings persist.
- Send `POST /api/dsp/reset` and confirm settings return to firmware defaults.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` (e.g. `2000`).
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- **Channel routing:** with audio playing, POST `stereo` / `left` / `right` and confirm the output audibly switches with no reboot and survives a reboot.
- Confirm Bluetooth mode still plays normally and is unchanged (stereo, no routing).
- Confirm playback through the I2S DAC works with no MCLK line connected.
