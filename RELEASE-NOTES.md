# Release Notes - ESP32 Audio Client v1.3.1

Release date: 2026-06-13

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [1.3.1]

- Quiesce the Snapclient audio pipeline when an OTA upload starts: the music fades out quickly, then the decode and network tasks stop and I2S flushes, so the flash write and upload run on an idle device. Far more reliable OTA while music is playing.
- A failed OTA after audio was stopped now reboots to recover playback (the update never commits, so it stays on the current firmware).
- Removed the unused optional I2S MCLK support and its `board_config.h` settings (PCM5102-style DACs derive their clocks from BCLK).
- Cleaned up the repository documentation for sharing and aligned visible firmware version fields.

## Summary

A small follow-up to 1.3.0. The main functional change makes OTA updates reliable
while audio is playing: previously the OTA HTTP receive competed with Opus decode
for CPU (and the upload shared Wi-Fi with the Snapcast stream), which could stall
or fail the update. Now an incoming update fades the music out and stops the audio
pipeline first, handing the flash write a quiet, idle device. The rest of the
release removes the never-used optional MCLK output and tidies the docs.

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

- Previous version: `1.3.0`
- New version: `1.3.1`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `1.3.1`
- `firmwareVersion`: `1.3.1`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2016016` bytes (well within the OTA slot limit)

## Manual Test Checklist

- Flash `1.3.1` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 1.3.1` and `GET /api/status` reports `firmwareVersion` as `1.3.1`.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` (e.g. `2000`).
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- **OTA failure recovery:** if an upload is interrupted after audio stops, confirm the device reboots and resumes playback on the existing firmware.
- **Channel routing:** with audio playing, POST `stereo` / `left` / `right` and confirm the output audibly switches with no reboot and survives a reboot.
- Confirm Bluetooth mode still plays normally and is unchanged (stereo, no routing).
- Confirm playback through the I2S DAC works with no MCLK line connected.
