# Release Notes - ESP32 Audio Client v1.1.4

Release date: 2026-06-12

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- PCM Snapserver stream

## [1.1.4]

- Increased the Snapclient PCM output queue from `65536` bytes to `131072` bytes.
- Re-enabled Snapclient rebuffering so low queue fill triggers a short refill pause instead of playing through underruns as distortion.
- Raised the Snapclient initial activation threshold to `85%` and changed rebuffer thresholds to `35% -> 80%`.
- Updated visible firmware version fields and release documentation for `1.1.4`.

## Summary

This release focuses on reducing audible Snapclient dropouts after the move to a 44.1 kHz PCM stream. It gives the ESP32 a deeper PCM queue, waits for more audio before starting playback, and restores low-fill rebuffering so the firmware prefers a short refill pause over distorted playback during an underrun.

The stream profile remains `44100:16:2` PCM.

## What This Firmware Does

This firmware turns an ESP32-WROVER module into a Wi-Fi Snapcast audio client with a small local control API for companion apps.

In normal use, the device boots into Snapclient mode, connects to Wi-Fi, connects to the configured Snapserver, receives a PCM Snapcast stream, and outputs stereo audio through an external I2S DAC. It is intended for DIY whole-home or multi-room audio builds where Snapserver provides synchronized audio and the ESP32 board acts as a local network speaker endpoint.

The firmware also includes an alternate Bluetooth mode. Pressing the runtime mode button stores a Bluetooth-mode request and reboots the device. In Bluetooth mode, the ESP32 works as a simple Bluetooth A2DP audio sink using the same external I2S DAC output path.

SnapControl, the companion iOS app, can use the local HTTP API while the device is running in Snapclient mode to:

- read firmware identity and status
- show battery voltage/percentage when battery sensing is wired and enabled
- change local output routing between stereo, left-to-both, and right-to-both
- save the Bluetooth device name used on later Bluetooth-mode boots
- upload compatible firmware `.bin` builds over OTA

Bluetooth mode does not expose the local HTTP API. App control and OTA update are Snapclient/Wi-Fi-mode features.

## Required Hardware

Minimum hardware required:

- ESP32-WROVER-IE-N16R8 module or board
- 16 MB flash / 8 MB PSRAM target
- external I2S DAC module
- stable 3.3 V power supply for the ESP32
- 2.4 GHz Wi-Fi network with access to the Snapserver
- Snapserver configured to provide a PCM stream

Default I2S DAC wiring:

| Function | GPIO | Notes |
| --- | --- | --- |
| I2S BCLK | `GPIO26` | External DAC bit clock |
| I2S LRCLK / WS | `GPIO25` | External DAC word select |
| I2S DOUT | `GPIO13` | External DAC serial data input |
| I2S MCLK | `GPIO0` | Optional only; disabled by default |

Optional hardware:

- momentary mode button on `GPIO23` to toggle between Snapclient and Bluetooth mode by reboot
- Wi-Fi status LED on `GPIO32`
- Bluetooth status LED on `GPIO33`
- battery voltage divider connected to `GPIO34`

Default battery-sense assumption:

- battery positive -> `270k` resistor -> `GPIO34` sense input -> `47k` resistor -> `GND`
- intended for a 4S lithium pack

Hardware notes:

- Common PCM5102-style DAC modules usually do not require MCLK, so MCLK is disabled by default.
- If MCLK is enabled, classic ESP32 MCLK routing is limited and `GPIO0` is a boot-strapping pin.
- The ESP32 must be on the same trusted local network as SnapControl for OTA upload.
- USB access should remain available for first flash and recovery.

## Highlights

- Increased Snapclient PCM buffering for Wi-Fi jitter tolerance.
- Re-enabled clean stop-and-refill behavior when the output queue gets low.
- Kept the 44.1 kHz Snapserver PCM profile from `1.1.3`.
- Kept channel routing, Bluetooth-name control, battery reporting, OTA upload, and existing Snapclient API behavior available.

## Changes Since v1.1.3

- Changed `SNAP_OUTPUT_QUEUE_BYTES` from `65536` to `131072`.
- Changed `SNAP_OUTPUT_ACTIVATION_PERCENT` from `75` to `85`.
- Changed `SNAP_OUTPUT_REBUFFER_START_PERCENT` from `55` to `35`.
- Changed `SNAP_OUTPUT_REBUFFER_RESUME_PERCENT` from `75` to `80`.
- Changed `SNAPCLIENT_REBUFFER_ENABLED` from `false` to `true`.
- Updated visible firmware version examples from `1.1.3` to `1.1.4`.

## Firmware Version

- Previous version: `1.1.3`
- New version: `1.1.4`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `1.1.4`
- `firmwareVersion`: `1.1.4`

Versioning policy:

- Small releases increment patch versions, for example `v1.0.x`.
- Medium releases increment minor versions and reset patch, for example `v1.x.0`.
- Large releases increment major versions and reset minor/patch, for example `vx.0.0`.
- `VERSION` stores the canonical version without the leading `v`; release tags include the leading `v`.
- `CHANGELOG.md` and `RELEASE-NOTES.md` must stay aligned with firmware version and release behavior changes.

## OTA Update Workflow

Install this release by OTA from an OTA-capable build such as `1.1.3`, or by USB flashing if the device is on an older build or needs recovery.

After this release is running on the ESP32:

1. Build future firmware with PlatformIO.
2. Use the generated file:
   `.pio/build/esp32-wrover-ie-n16r8/firmware.bin`
3. Upload that file from SnapControl to:
   `POST http://<esp-ip>:8080/api/firmware`
4. Wait for the ESP32 to reboot.
5. Confirm the device returns through `GET /api/status`.

## API Changes

`GET /api/status` now reports OTA support and target metadata:

```json
{
  "board": "ESP32-WROVER-IE-N16R8",
  "flash_size_mb": 16,
  "ota_partition_size": 6553600,
  "ota_supported": true,
  "update_in_progress": false,
  "capabilities": {
    "firmware_update": true
  }
}
```

New firmware upload endpoint:

```http
POST /api/firmware
Content-Type: application/octet-stream
Content-Length: <firmware size in bytes>
X-Firmware-Filename: firmware.bin
```

The request body must be the raw PlatformIO firmware `.bin` app image. Do not use multipart upload or a JSON wrapper.

Successful response:

```json
{
  "ok": true,
  "message": "Firmware accepted. Rebooting."
}
```

## Important Notes

- OTA can be used from an existing OTA-capable firmware build.
- USB flashing is required for first install, recovery, or devices running firmware without OTA support.
- OTA updates cannot change the partition table or bootloader.
- USB recovery is still required if Wi-Fi, Snapclient mode, or the OTA endpoint stops working.
- OTA upload is intended for trusted local-network use only.
- Do not expose the ESP32 HTTP API to the public internet.

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

The current `1.1.4` build output size is comfortably below the OTA slot limit:

- App slot size: `6553600` bytes
- Built firmware image: `1924096` bytes

## Manual Test Checklist

- Flash `1.1.4` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 1.1.4`.
- Confirm `GET /api/status` reports `project` as `ESP32 Audio Client`.
- Confirm `GET /api/status` reports `firmwareVersion` as `1.1.4` after Wi-Fi connects.
- Confirm Snapserver is configured with `sampleformat=44100:16:2&codec=pcm`.
- Confirm boot logs report the Snapclient/I2S format as `44100 Hz, 16-bit, 2 ch`.
- Confirm boot logs report `queue=131072 bytes`, `queue activation=85%`, and `rebuffer=35% -> 80%`.
- Confirm occasional network stalls produce short clean refill pauses rather than distorted underrun audio.
- Confirm normal Snapclient audio behavior still works without obvious pitch, speed, crackle, or repeated buffering artifacts.
- Confirm `GET /api/status` reports `power_source`.
- Confirm `POST /api/power-source` accepts `battery` and `mains` and persists after reboot.
- Confirm `power_source: "mains"` reports `battery.available: false`.
- Confirm a battery reading of `20%` or lower alternates the active mode LED with the red LED on `GPIO14` once per second.
- Confirm `power_source: "mains"` keeps the red low-battery LED off.
- Confirm the Wi-Fi LED blinks while connecting and stays solid once connected.
- Temporarily unavailable Wi-Fi should log diagnostics and retry without a reboot loop.
- Leaving the device powered on with no active audio should not trigger a playback-idle reboot.
- Confirm `ota_supported` is `true`.
- Confirm `capabilities.firmware_update` is `true`.
- Confirm channel routing still works.
- Confirm Bluetooth-name saving still works.
- Upload a known-good future `firmware.bin` through SnapControl.
- Confirm the ESP32 reboots and returns to `/api/status`.
- Confirm failed uploads leave the old firmware running.
