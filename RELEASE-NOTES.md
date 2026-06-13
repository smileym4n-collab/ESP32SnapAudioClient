# Release Notes - ESP32 Audio Client v1.3.0

Release date: 2026-06-13

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- Opus Snapserver stream

## [1.3.0]

- Fixed Snapclient channel routing so `stereo` / `left` / `right` actually change the audio (previously saved and reported but never applied). Takes effect immediately, no reboot.
- Moved the Snapclient network receive task to core 0 to give Opus decode/output a dedicated core 1.
- Dropped oversized compressed chunks whole instead of partially writing them, preventing an Opus frame-boundary desync.
- Retuned the rebuffer thresholds from `10% -> 40%` to `5% -> 15%` for short refills instead of multi-second silences.
- Added a Wi-Fi loss grace period so a brief link blip no longer reboots the device.
- Removed unused PCM-era code and updated visible firmware version fields for `1.3.0`.
- Left the Bluetooth A2DP -> I2S path unchanged.

## Summary

The headline fix is **channel routing**: companion apps (web/iOS) could already select stereo / left / right, but the firmware never applied it because the Snapclient audio wrote straight to I2S, bypassing the routing stage. Routing now runs in the final PCM output probe and reads the live mode on every write, so the existing `POST /api/channel-mode` buttons work with no app changes and no reboot.

The rest of the build targets the occasional Spotify dropout (the periodic short play/pause pattern). Opus decode is CPU-heavy and was sharing core 1 with the higher-priority packet-receive loop; the receive task is now on core 0 so decode gets a full core. Rebuffer thresholds were lowered so a low-buffer event is a short blip rather than a multi-second pause, the compressed queue no longer desyncs on overflow, and a brief Wi-Fi blip recovers in place instead of forcing a reboot.

## Snapserver Profile

Use a 44.1 kHz stereo PCM FIFO input from librespot, but encode the Snapcast transport as Opus. Keep a generous `buffer` - it is the master jitter cushion:

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

- Previous version: `1.2.1`
- New version: `1.3.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `1.3.0`
- `firmwareVersion`: `1.3.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `2015600` bytes (well within the OTA slot limit)

## Manual Test Checklist

- Flash `1.3.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 1.3.0`.
- Confirm `GET /api/status` reports `firmwareVersion` as `1.3.0` after Wi-Fi connects.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=opus` and a healthy `buffer` (e.g. `2000`).
- Confirm boot logs report `decoder=OpusAudioDecoder` and the Snapclient/I2S output format as 48 kHz, 16-bit, 2 channel.
- **Channel routing:** with audio playing, `POST /api/channel-mode` `stereo`, then `left`, then `right`, and confirm the output audibly changes (left-only and right-only both play the selected channel on both speakers) with no reboot and no glitch.
- Confirm the selected channel mode survives a reboot (restored from preferences) and is reported by `/api/status`.
- Test playback at weaker Wi-Fi locations: confirm the periodic short play/pause stutter is reduced and that any rebuffer is a brief blip rather than a multi-second silence.
- Briefly drop Wi-Fi (e.g. AP blip) and confirm the device recovers in place where possible instead of immediately rebooting.
- Confirm Bluetooth mode still plays normally and is unchanged (stereo, no routing).
- Confirm OTA support still reports and works correctly.
