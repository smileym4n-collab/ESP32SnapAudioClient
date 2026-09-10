# Release Notes - ESP32 Audio Client v2.3.0

Release date: 2026-07-13

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- First-generation B&W Zeppelin I2S retrofit
- PCM1808 left fitted and clocked from its external 12.288 MHz oscillator
- Opus Snapserver stream

## [2.3.0]

- Replaced the previous internally clocked I2S output with ESP32 TX-only slave
  mode for the Zeppelin install.
- Uses external PCM1808 BCK/LRCK as the authoritative I2S clock domain:
  48 kHz LRCK, 3.072 MHz BCK, 64 BCK per stereo frame.
- Drives only ESP32 DATA into the Zeppelin DSP input; MCLK/SCKI is not connected
  to the ESP32.
- Sends standard Philips I2S, stereo, MSB first, with 24 valid audio bits in
  32-bit slots.
- Expands the existing decoded signed 16-bit PCM path into signed 32-bit
  containers for I2S output, with the sample shifted left 16 bits so the active
  audio bits are MSB-aligned and the low eight bits of the 24-bit field are zero.
- Added `I2S_BCK_IN`, `I2S_LRCK_IN`, and `I2S_DATA_OUT` board configuration
  definitions for the Zeppelin wiring.
- Added initialization, format, GPIO, DMA-event, and missing-external-clock
  diagnostics without falling back to I2S master mode.
- Prevented Snapclient output-stall recovery from rebooting the ESP32 while
  I2S writes are blocked by missing external BCK/LRCK.

## Summary

This minor release changes only the final hardware I2S endpoint needed for the
Zeppelin retrofit. Snapcast protocol handling, Opus decode, buffering, channel
routing, dynamic Snapcast resampling, Wi-Fi behavior, OTA, and the local control
API remain intact.

The external PCM1808 clock domain is authoritative. The ESP32 does not generate
BCK, LRCK, or MCLK for the Zeppelin path.

## Required GPIO Setup

The Zeppelin I2S wiring is configured in [board_config.h](include/board_config.h):

- `I2S_BCK_IN`: GPIO26, PCM1808 BCK input to the ESP32
- `I2S_LRCK_IN`: GPIO25, PCM1808 LRCK/WS input to the ESP32
- `I2S_DATA_OUT`: GPIO13, ESP32 DATA output to the Zeppelin DSP input through a
  22-47 ohm series resistor

The original PCM1808 DOUT signal must be physically disconnected from the
Zeppelin DSP. Leave PCM1808 SCKI/MCLK disconnected from the ESP32.

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

- Previous version: `2.2.1`
- New version: `2.3.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.3.0`
- `firmwareVersion`: `2.3.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Built firmware image: `1999809` bytes

## Manual Test Checklist

- Flash `2.3.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.3.0` and `GET /api/status` reports
  `firmwareVersion` as `2.3.0`.
- Confirm the boot log reports I2S slave TX, 48 kHz Philips I2S, 24 valid bits
  in 32-bit slots, and the configured `I2S_BCK_IN`, `I2S_LRCK_IN`, and
  `I2S_DATA_OUT` pins.
- With an oscilloscope or logic analyser, confirm external LRCK is approximately
  48 kHz and external BCK is approximately 3.072 MHz before starting playback.
- Confirm no ESP32-generated BCK/LRCK appears; the ESP32 should only drive DATA.
- Confirm digital silence is framed as zero DATA during boot, underrun, and stream
  interruption.
- During Snapclient playback, confirm DATA is synchronous to the external
  BCK/LRCK and uses standard Philips I2S timing with the MSB one BCK after LRCK
  transition.
- Temporarily stop the external clocks and confirm the firmware logs throttled
  I2S write-stall diagnostics without repeatedly rebooting.
- Restore the external clocks and confirm playback can recover when the stream
  refills.
- Confirm `stereo`, `left`, and `right` channel modes still apply immediately.
- **OTA while playing:** start an OTA from the companion app with music playing
  and confirm the audio fades out cleanly, the update completes, and playback
  resumes on the new firmware after reboot.
