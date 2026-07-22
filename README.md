# ESP32 Audio Client

Firmware that turns an **ESP32-WROVER** + external I2S DAC into a dual-mode audio
receiver:

- a **synchronized Snapcast speaker** (multi-room audio over Wi-Fi, Opus transport), or
- a **standalone Bluetooth speaker** (A2DP sink).

It boots into Snapcast mode and you flip to Bluetooth (and back) with a single
button press. Both modes share the same I2S DAC output path.

Version: **2.4.0**

## Features

- **Two modes, one device** — Snapcast client over Wi-Fi, or Bluetooth A2DP sink.
- **Opus Snapcast transport** — compressed audio keeps playback stable on weaker
  ESP32 Wi-Fi, decoded to 48 kHz PCM on-device.
- **Channel routing** — play `stereo`, or fold `left`/`right` to both DAC channels
  (great for using one board as a mono left or right speaker in a stereo pair).
- **Companion-app HTTP API** — a small local API on port `8080` for controls
  Snapserver doesn't expose (channel routing, power source, Bluetooth name, OTA).
- **OTA firmware updates** — push a new build over the local network; audio fades
  out and the pipeline quiesces first so the update lands reliably.
- **Battery monitoring** — INA236 pack voltage, 4S percentage, current, and power.
- **Status LEDs** — separate Wi-Fi, Bluetooth, and low-battery indicators.

## Target hardware

- Module: **ESP32-WROVER-IE-N16R8** (16 MB flash / 8 MB PSRAM, external antenna)
- Audio output: an **external I2S DAC** (e.g. a PCM5102-style module)
- The on-chip DAC is not used.

PSRAM is required — it backs the larger audio buffers used for Wi-Fi jitter
resistance.

## Pin map

All hardware assignments live in [board_config.h](include/board_config.h).

| Function | GPIO | Notes |
| --- | --- | --- |
| I2S BCLK | `GPIO26` | External DAC bit clock |
| I2S LRCLK / WS | `GPIO25` | External DAC word select |
| I2S DOUT | `GPIO13` | External DAC serial data input |
| INA236 SDA | `GPIO21` | I2C battery telemetry data |
| INA236 SCL | `GPIO19` | I2C battery telemetry clock |
| Mode button | `GPIO23` | Momentary, active-low, internal pull-up |
| Wi-Fi LED | `GPIO32` | Snapclient/Wi-Fi status, active-low common-anode |
| Bluetooth LED | `GPIO33` | Bluetooth status, active-low common-anode |
| Low-battery LED | `GPIO14` | Red low-battery warning, active-low common-anode |

No MCLK line is used — PCM5102-style DACs generate their own clocks from BCLK.
If your LEDs are wired as GPIO → resistor → LED → GND instead of common-anode,
flip the matching `*_ACTIVE_HIGH` flag in `board_config.h`.

## Quick start

1. Copy the secrets template and add your Wi-Fi credentials:

   ```bash
   cp include/secrets.example.h include/secrets.h
   # edit include/secrets.h — it is gitignored, keep it local
   ```

2. Build, flash, and watch the serial log:

   ```bash
   pio run
   pio run -t upload
   pio device monitor -b 115200
   ```

3. Point the device at your Snapserver by editing `snapServerIp()` in
   [snapclient_config.h](include/snapclient_config.h), and set up an Opus stream
   on the server — see [docs/snapserver.md](docs/snapserver.md).

The build uses PlatformIO's `default_16MB.csv` partition table, which provides
two OTA app slots.

## How it works

### Snapclient mode (default)

Cold boot always starts here. The device joins Wi-Fi as a station, connects to
Snapserver, decodes the Opus stream to 48 kHz PCM, and plays it through the I2S
DAC. A deep compressed buffer absorbs Wi-Fi jitter, and the local control API
comes up on port `8080`.

Expected Snapserver stream: `codec=opus`, `sampleformat=44100:16:2`. See
[docs/snapserver.md](docs/snapserver.md) for a worked example and the recommended
`buffer` setting.

### Bluetooth mode

Entered after a mode-button press and reboot. Wi-Fi is disabled and the device
becomes a Bluetooth A2DP sink, advertising as `CoolCube` by default, writing
received audio to the same I2S DAC. Bluetooth mode is a simple connect-and-play
stereo receiver and does not use the control API or channel routing.

### Switching modes

The mode button is a runtime toggle, not a boot selector:

- power-up / cold boot → **Snapclient mode**
- press while running → store the other mode, reboot into it

During bring-up you can also switch from the serial monitor: send `b` for
Bluetooth, `s` for Snapclient, `t` to toggle, or `?` for help.

### Status LEDs

- **Snapclient:** Wi-Fi LED blinks while connecting, solid once connected.
- **Bluetooth:** BT LED blinks while waiting for a source, solid once connected.
- **Low battery:** the red LED alternates with the active-mode LED once per second
  when running on battery at or below 20%.

## Companion-app control API

Snapclient mode exposes a local HTTP API on port `8080`:

- `GET /api/status` — firmware identity, mode, power source, channel mode,
  battery, and capabilities.
- `POST /api/channel-mode` — `{"channel_mode":"stereo"|"left"|"right"}`.
- `POST /api/power-source` — `{"power_source":"battery"|"mains"}`.
- `POST /api/bluetooth-name` — `{"bluetooth_name":"CoolCube Kitchen"}`.
- `POST /api/firmware` — raw `.bin` app image for OTA update.

Channel mode and power source are saved in flash and restored on later boots.
The Bluetooth name is saved and applied the next time the device boots into
Bluetooth mode.

See [API.md](API.md) for the compact reference and
[docs/control-api.md](docs/control-api.md) for full request/response examples.

OTA updates are intended for trusted local-network use. First-time setup,
partition-table changes, and recovery from a broken Wi-Fi config still require a
USB flash.

## Battery monitor

The board reads pack voltage, current draw, and power from an INA236 over I2C,
then applies the existing 4S lithium voltage-to-percentage curve. Wire SDA to
`GPIO21` and SCL to `GPIO19`; the firmware detects all INA236A and INA236B A0
address combinations. The configured `20 mΩ` shunt and ±81.92 mV measurement
range support approximately ±4.096 A. Current is positive when the load draws
from INA236 `IN+` toward `IN-`.

`GET /api/status` reports volts, amps, watts, and estimated percentage. Set the
power source to `mains` to hide battery telemetry in companion apps. Hardware
pins and measurement constants are in [board_config.h](include/board_config.h)
and [snapclient_config.h](include/snapclient_config.h).

## Configuration files

| File | Purpose |
| --- | --- |
| [include/secrets.example.h](include/secrets.example.h) | Template for your local `include/secrets.h` Wi-Fi credentials |
| [include/board_config.h](include/board_config.h) | Hardware pin assignments and LED/battery options |
| [include/snapclient_config.h](include/snapclient_config.h) | Snapserver address, audio format, buffering and runtime tuning |

## Documentation

- [docs/snapserver.md](docs/snapserver.md) — Snapserver stream setup
- [docs/control-api.md](docs/control-api.md) — full companion-app API reference
- [API.md](API.md) — compact API reference
- [CHANGELOG.md](CHANGELOG.md) — versioned change history
- [RELEASE-NOTES.md](RELEASE-NOTES.md) — current release notes

## Limitations

- Mode changes happen by software reboot; only one audio mode is active per boot.
- Bluetooth mode does not talk to Snapserver and ignores channel routing.
- Snapclient mode depends on Wi-Fi; keep the ESP32 on strong 2.4 GHz signal and,
  where possible, the Snapserver on wired Ethernet.
