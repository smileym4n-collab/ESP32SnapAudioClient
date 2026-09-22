# Release Notes - ESP32 Audio Client v2.4.0

## [Unreleased] — Mission 730 bench DSP

The volume-pot wiper is mapped to GPIO35 (ADC1). It now owns the
common DSP master level; the provisional raw range remains 0–4095 until measured
on the bench. The serial terminal prints a throttled `[dsp-pot]` line when the
control moves, showing raw and filtered ADC readings, position percentage and
the resulting master gain.

The serial shell now provides `dsp dac`, which performs PCM5122 register readback over I2C and reports configured mute/format/volume values together with the chip's clock, analogue-mute, XSMUTE, short-detect, boot and power-state monitors.

The PCM5122 is now explicitly configured for software-controlled three-wire I2S. BCK is selected as the PLL reference and the TI-recommended ratio-32 PLL plus DSP, DAC, negative-charge-pump and OSR dividers are programmed for both 44.1 and 48 kHz. Clock-configuration register readback was added to `dsp dac`.

Live verification on 2026-09-15 confirmed the programmed clock values read back correctly during 48-kHz playback. The PLL locked, the active clock-error flag cleared and the DAC advanced from power state 1 (waiting for charge-pump voltage valid) to power state 5 (run). The analogue-mute monitor still read muted pending confirmation at the physical outputs.

Snapclient stall recovery now distinguishes an actual stuck output from the intentional wait for initial-buffer or rebuffer thresholds. This prevents Spotify stream startup pauses from being misclassified as a stall and triggering a software reset.

Snapclient's dependency patch now also bounds the wait for a partially delivered TCP message body. If Snapserver closes mid-message, the ESP32 abandons the stale socket after at most five seconds and reconnects; previously the receive task waited forever while I2S continued clocking silence.

PCM5122 wiring is now configured: SDA GPIO21, SCL GPIO22, 7-bit address 0x4C with both ADR straps low. The unused INA236 monitor is disabled to release GPIO21. DAC initialization logs successful I2C acknowledgements or holds output at zero on failure. On 2026-09-15, live write/readback verified bidirectional ESP32 communication during playback. The DAC detected a 32-BCK frame and valid sample-rate/BCK clocks, but remained in power state 1 (waiting for charge-pump voltage valid) with both analogue outputs hardware-muted; check CPVDD, the CAPP-CAPM flying capacitor and VNEG decoupling before acoustic testing.

Serial-monitor usability: project monitor settings now enable local echo, send-on-Enter and LF endings, with DTR/RTS deasserted. Restart the monitor to apply; no firmware reflash is required.

Bench follow-up: the first flashed DSP build rebooted because Snapcast task creation failed with only 8104 bytes of free heap and a 3828-byte largest DMA/internal block. The DSP workspace now allocates 11788 bytes explicitly in PSRAM once at startup, leaving only 884 bytes of engine state internally. Allocation failure forces silence; zero-delay programme playback bypasses external sample storage. Task stacks and DMA settings remain unchanged. The INA236-not-found warning is separate. On 2026-09-14, serial inspection of the corrected board confirmed `[snapclient] running` and a responsive DSP shell, without the prior task-allocation failure. Streaming verification remains pending: the inspected DSP had processed no PCM blocks.

The working tree adds a two-way active crossover and is intentionally not release-ready until scope and streaming soak tests are complete. `VERSION` stays `2.4.0`; no tag or push is part of this bench change.

- LR4 at 2400 Hz by default, actual-rate coefficients, six PEQs per semantic HIGH/LOW channel, independent trims/mutes/polarity/0–10 ms delay, and persistent output mapping (HIGH=LEFT by default).
- Existing Snapcast L/R source selection remains before DSP; Bluetooth always averages L/R. Existing companion API and task/core/DMA configuration are retained.
- Existing serial dispatcher accepts `dsp` commands; Preferences saves only on `dsp save`. Tone is off at boot and never stored.
- PCM5122 SDA/SCL are GPIO21/22 at address 0x4C. The pot wiper is GPIO35; while enabled, it owns master volume and `dsp master` is unavailable.
- Complete command reference, implementation boundaries and manual checks: [speaker DSP guide](docs/speaker-dsp.md).

Validation: `pio run` passed; `python3 scripts/test_dsp.py` passed with address/undefined-behaviour sanitizers. Tests cover filter math, routing, update publication, invalid settings, and mocked NVS restore/tone exclusion. The original build was flashed and failed on-board task allocation as recorded above. Corrected-board startup and shell response were verified on 2026-09-14; acoustic, actual NVS save/restore, CPU-load and dropout testing remain pending. Existing dependency `htons`/`ntohs` redefinition warnings remain.

Final unreleased build measurements:

- Built firmware image: `2069808` bytes.
- Linked flash usage: `2063229` bytes / `6553600`.
- Static RAM: `73104` bytes / `327680` (excludes dynamic DMA/codec buffers and task stacks).
- DSP Engine: `884` internal static bytes; workspace: `11788` bytes allocated once in PSRAM; Config: `232` bytes.

Manual bench checklist: confirm zero/ramped startup and DAC fault handling; verify 100 Hz / 10 kHz routing and 2.4 kHz half-amplitude/phase; exercise mapping/mutes/trims/polarity/delay and the pot; verify existing Snapcast L/R selection and Bluetooth averaging; save/reboot and confirm tone remains off; soak both sources with all PEQs enabled, inspect clip/block-time status, and test OTA/restart fades. Full steps are in the guide. Do not fit LM1875s before validating the two input pads.

---

The sections below describe the previously released v2.4.0 firmware.

Release date: 2026-07-22

Target hardware:

- ESP32-WROVER-IE-N16R8
- 16 MB flash / 8 MB PSRAM
- External I2S DAC
- INA236 power monitor with `20 mΩ` shunt
- Opus Snapserver stream

## [2.4.0]

- Replaced the GPIO34 resistor-divider/ADC battery measurement with an INA236 on I2C (`SDA GPIO21`, `SCL GPIO19`).
- Added pack current and power telemetry while retaining the existing 4S voltage-to-percentage lookup curve.
- Added `battery.current` in amperes, `battery.power` in watts, and `capabilities.battery_telemetry: true` to `GET /api/status`.
- Detects all INA236A and INA236B A0 address combinations and verifies the TI manufacturer and INA236 device IDs before reporting data.
- Configured the `20 mΩ` shunt for a ±4.096 A current range with 64-sample hardware averaging.

## Summary

This minor release updates the firmware for the revised battery-power hardware.
Companion apps can now show pack voltage, estimated 4S charge percentage,
signed current draw, and power usage from the local status API. Existing
`battery.voltage`, `battery.percent`, and mains-mode hiding behavior remain
compatible.

Current is positive when load current flows from the INA236 `IN+` side toward
`IN-`. With the configured `20 mΩ` shunt and ±81.92 mV INA236 range, measurable
current is approximately ±4.096 A.

Bluetooth output and OTA upload behavior are unchanged.

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
- New version: `2.4.0`

Visible firmware version fields:

- `project`: `ESP32 Audio Client`
- `version`: `2.4.0`
- `firmwareVersion`: `2.4.0`

## Build Notes

Build command:

```bash
pio run -e esp32-wrover-ie-n16r8
```

The build uses PlatformIO's `default_16MB.csv` partition table, which provides two OTA app slots.

- App slot size: `6553600` bytes
- Released v2.4.0 firmware image: `2034880` bytes (unreleased working-tree image is reported above)

## Manual Test Checklist

- Flash `2.4.0` by USB or OTA from an OTA-capable build.
- Confirm the boot log reports `[version] 2.4.0` and detects an INA236 address on SDA `GPIO21` / SCL `GPIO19` with a `0.020 ohm` shunt.
- Confirm `GET /api/status` reports `firmwareVersion` as `2.4.0` and `capabilities.battery_telemetry: true`.
- With `power_source` set to `battery`, confirm `battery.available: true` and plausible `voltage`, `percent`, `current`, and `power` values.
- Apply a known load and verify positive current flows from INA236 `IN+` toward `IN-`; compare current and power against a reference meter.
- Remove or disconnect the INA236 and confirm `battery.available: false` without affecting audio or the rest of the control API.
- Set `power_source` to `mains` and confirm battery telemetry is suppressed with `battery.available: false`.
- Confirm the low-battery LED still activates at or below 20% based on the INA236 pack voltage.
- Confirm Snapclient playback and channel routing continue normally.
- **OTA while playing:** start an OTA from the companion app with music playing and confirm the audio fades out cleanly, the update completes, and playback resumes on the new firmware after reboot.
- Confirm Bluetooth mode still plays normally and is unchanged.
