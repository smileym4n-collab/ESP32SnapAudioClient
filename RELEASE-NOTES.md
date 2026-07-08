# Release Notes - ESP32 Audio Client v2.2.0

Release date: 2026-07-08

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [2.2.0]

- Removed the Snapclient DSP/EQ engine and the `/api/dsp` control endpoints.
- Simplified `/api/status` so it reports firmware identity, OTA, power source, channel mode, Bluetooth name, battery, and capabilities without a DSP object.
- Advertised `capabilities.snapclient_dsp: false` and `capabilities.snapclient_dsp_update: false` for companion apps.
- Made channel mode apply immediately and persist during `POST /api/channel-mode`, so `stereo`, `left`, and `right` survive reboot.
- Kept OTA firmware upload behavior unchanged.

## Summary

This minor release removes the unused DSP/EQ layer and focuses the firmware on
stable Snapclient playback with instant local controls. Channel routing still
runs in the final PCM output probe, so mode changes take effect while audio is
playing without restarting Snapclient or reconfiguring I2S.

Bluetooth output remains unchanged.

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

- Previous version: `2.1.6`
- New version: `2.2.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.2.0`
- `firmwareVersion`: `2.2.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2018544` bytes

## Manual Test Checklist

- Flash `2.2.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.2.0` and `GET /api/status` reports `firmwareVersion` as `2.2.0`.
- Confirm `GET /api/status` does not include a `dsp` object and reports `capabilities.snapclient_dsp: false` plus `capabilities.snapclient_dsp_update: false`.
- Confirm `GET /api/dsp`, `POST /api/dsp`, and `POST /api/dsp/reset` are no longer available.
- With Snapclient playback running, POST `stereo`, `left`, and `right` to `/api/channel-mode`; confirm the output switches immediately with no reboot and no audio dropout.
- Reboot and confirm the selected channel mode persists.
- From the companion web app, move volume and channel controls during playback and confirm the UI updates immediately while audio continues.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` such as `2000`.
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- Confirm Bluetooth mode still plays normally and is unchanged.
- Confirm playback through the I2S DAC works with no MCLK line connected.
