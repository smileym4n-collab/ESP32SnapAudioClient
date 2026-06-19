# Snapclient Companion App API

Base URL:

```text
http://<esp-ip>:8080
```

Snapclient mode exposes this HTTP API. Bluetooth mode does not expose it.

## GET /api/status

Returns the current local ESP32 firmware state for SnapApp.

Example response:

```json
{
  "project": "ESP32 Audio Client",
  "version": "2.1.4",
  "firmwareVersion": "2.1.4",
  "board": "ESP32-WROVER-IE-N16R8",
  "flash_size_mb": 16,
  "ota_partition_size": 6553600,
  "ota_supported": true,
  "update_in_progress": false,
  "runtime_mode": "snapclient",
  "power_source": "battery",
  "channel_mode": "stereo",
  "dsp": {
    "enabled": false,
    "eq_profile": "Flat",
    "eq_profile_display": "Flat",
    "eq_profiles": [
      {"name": "Flat", "display_name": "Flat"},
      {"name": "Pop", "display_name": "Pop"},
      {"name": "Rock", "display_name": "Rock"}
    ],
    "eq": {
      "preamp_db": 0.0,
      "bands": [
        {"type": "low_shelf", "frequency_hz": 80.0, "gain_db": 0.0, "q": 0.707, "enabled": true},
        {"type": "peaking", "frequency_hz": 250.0, "gain_db": 0.0, "q": 1.000, "enabled": true},
        {"type": "peaking", "frequency_hz": 1000.0, "gain_db": 0.0, "q": 1.000, "enabled": true},
        {"type": "peaking", "frequency_hz": 3500.0, "gain_db": 0.0, "q": 1.000, "enabled": true},
        {"type": "high_shelf", "frequency_hz": 10000.0, "gain_db": 0.0, "q": 0.707, "enabled": true}
      ]
    },
    "bass_boost_db": 0.0,
    "left_gain_db": 0.0,
    "right_gain_db": 0.0,
    "balance": 0.00,
    "loudness": {
      "enabled": false,
      "bass_max_db": 3.0,
      "full_boost_volume": 0.30,
      "flat_volume": 0.80
    },
    "headroom_db": 0.0,
    "soft_limiter": {
      "enabled": false,
      "ceiling": 0.98
    }
  },
  "bluetooth_name": "CoolCube",
  "battery": {
    "available": true,
    "voltage": 16.42,
    "percent": 95
  },
  "capabilities": {
    "channel_modes": ["stereo", "left", "right"],
    "bluetooth_name": true,
    "power_source": true,
    "snapclient_dsp": true,
    "snapclient_dsp_update": true,
    "firmware_update": true
  }
}
```

Fields:

| Field | Type | Notes |
| --- | --- | --- |
| `project` | string | Human-readable firmware/project name |
| `version` | string | Legacy firmware version field, same value as `firmwareVersion` |
| `firmwareVersion` | string | Firmware version to display in SnapApp |
| `board` | string | Target board/module name |
| `flash_size_mb` | number | Detected flash chip size in MB |
| `ota_partition_size` | number | Inactive OTA app partition size in bytes, or `0` if unavailable |
| `ota_supported` | boolean | `true` when `POST /api/firmware` can accept app-image uploads |
| `update_in_progress` | boolean | `true` while a firmware upload is active |
| `runtime_mode` | string | Current mode; `/api/status` is available in Snapclient mode |
| `power_source` | string | Saved power source: `battery` or `mains` |
| `channel_mode` | string | Current local output routing: `stereo`, `left`, or `right` |
| `dsp` | object | Current Snapclient-only DSP configuration |
| `bluetooth_name` | string | Saved Bluetooth device name used on later Bluetooth-mode boots |
| `battery.available` | boolean | `true` when battery sensing is enabled and a reading is available |
| `battery.voltage` | number | Reconstructed 4S pack voltage in volts, not ADC divider voltage |
| `battery.percent` | number | Estimated 4S battery percentage, `0..100` |
| `capabilities.channel_modes` | string array | Channel modes accepted by `POST /api/channel-mode` |
| `capabilities.bluetooth_name` | boolean | `true` when `POST /api/bluetooth-name` is available |
| `capabilities.power_source` | boolean | `true` when `POST /api/power-source` is available |
| `capabilities.snapclient_dsp` | boolean | `true` when the Snapclient PCM DSP status object is available |
| `capabilities.snapclient_dsp_update` | boolean | `true` when `POST /api/dsp` can update DSP settings |
| `capabilities.firmware_update` | boolean | `true` when `POST /api/firmware` is available |

The `dsp` object reports the live DSP settings. Firmware defaults are a true
bypass: `enabled: false`, Flat profile, loudness disabled, soft limiter disabled,
and zero gain/balance/headroom. Values are loaded from ESP32 preferences when
present, otherwise from `SNAPCLIENT_DSP_CONFIG`. Companion apps should usually
expose `eq_profile`, `bass_boost_db`, `loudness.enabled`, and `balance` as the
main user controls.

When battery sensing is unavailable:

```json
{
  "battery": {
    "available": false
  }
}
```

When `power_source` is `mains`, companion apps should hide battery UI. The
status response reports `battery.available: false`.

## GET /api/dsp

Returns the current Snapclient DSP settings. The response body has the same shape
as the `dsp` object in `GET /api/status`.

## POST /api/dsp

Partially updates Snapclient DSP settings, applies them live, and saves them in
ESP32 preferences so they survive reboot and OTA updates. EQ frequency, gain,
and Q values come from the selected firmware preset.

Request:

```http
POST /api/dsp
Content-Type: application/json
```

```json
{
  "enabled": true,
  "eq_profile": "Rock",
  "bass_boost_db": 2.0,
  "balance": 0.0,
  "loudness": {
    "enabled": true,
    "bass_max_db": 3.0
  },
  "headroom_db": -2.0,
  "soft_limiter": {
    "enabled": true,
    "ceiling": 0.98
  }
}
```

Accepted writable fields:

| Field | Range |
| --- | --- |
| `enabled` | boolean |
| `eq_profile` / `profile` / `preset` | one of the names in `eq_profiles` |
| `eq.profile` / `eq.preset` | one of the names in `eq_profiles` |
| `bass_boost_db` | `0.0..6.0` dB |
| `left_gain_db` / `right_gain_db` | `-12.0..12.0` dB |
| `balance` | `-1.0..1.0` |
| `loudness.enabled` | boolean |
| `loudness.bass_max_db` | `0.0..9.0` dB |
| `headroom_db` | `-12.0..0.0` dB |
| `soft_limiter.enabled` | boolean |
| `soft_limiter.ceiling` | `0.50..1.00` |

Out-of-range numeric values are clamped. Successful responses return the updated
DSP object.

## POST /api/dsp/reset

Clears saved DSP preferences, reapplies `SNAPCLIENT_DSP_CONFIG`, and returns the
updated DSP object.

## POST /api/channel-mode

Sets local Snapclient output routing.

Request:

```http
POST /api/channel-mode
Content-Type: application/json
```

```json
{
  "channel_mode": "left"
}
```

Allowed `channel_mode` values:

| Value | Behavior |
| --- | --- |
| `stereo` | Left input to left DAC channel, right input to right DAC channel |
| `left` | Left input duplicated to both DAC channels |
| `right` | Right input duplicated to both DAC channels |

Successful responses return the same shape as `GET /api/status`.

Invalid requests return:

```json
{
  "error": "invalid_channel_mode",
  "allowed": ["stereo", "left", "right"]
}
```

## POST /api/power-source

Sets whether this device should be treated as a battery-powered or mains-powered
client. The setting is saved in ESP32 preferences and survives reboots and OTA
updates.

Request:

```http
POST /api/power-source
Content-Type: application/json
```

```json
{
  "power_source": "mains"
}
```

Allowed `power_source` values:

| Value | Behavior |
| --- | --- |
| `battery` | Report battery voltage/percentage when the configured ADC sense input is available |
| `mains` | Suppress battery readings and report `battery.available: false` |

Successful responses return the same shape as `GET /api/status`.

Invalid requests return:

```json
{
  "error": "invalid_power_source",
  "allowed": ["battery", "mains"]
}
```

## POST /api/bluetooth-name

Sets the Bluetooth device name that will be used the next time the firmware boots into Bluetooth mode. This endpoint is only available while the device is running in Snapclient mode on Wi-Fi.

Request:

```http
POST /api/bluetooth-name
Content-Type: application/json
```

```json
{
  "bluetooth_name": "CoolCube Kitchen"
}
```

Allowed `bluetooth_name` value:

| Rule | Value |
| --- | --- |
| Length | 1 to 31 characters |
| Characters | Printable ASCII, excluding `"` and `\` |

Successful responses return the same shape as `GET /api/status`.

Invalid requests return:

```json
{
  "error": "invalid_bluetooth_name",
  "max_length": 31
}
```

## POST /api/firmware

Uploads a prebuilt PlatformIO firmware `.bin` app image to the inactive OTA partition. This endpoint is only available while the device is running in Snapclient mode on Wi-Fi.

Request:

```http
POST /api/firmware
Content-Type: application/octet-stream
Content-Length: <firmware size in bytes>
X-Firmware-Filename: firmware.bin
```

Request body:

- raw firmware `.bin` app image
- no multipart wrapper
- no JSON envelope

Successful response:

```json
{
  "ok": true,
  "message": "Firmware accepted. Rebooting."
}
```

Failure response:

```json
{
  "ok": false,
  "error": "image_too_large",
  "message": "Firmware image is larger than the OTA partition"
}
```

After a successful upload the device reboots. Companion apps should poll `GET /api/status` until the device returns and reports its new `firmwareVersion`.
