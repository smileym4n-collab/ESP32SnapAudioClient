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
  "version": "2.0.0",
  "firmwareVersion": "2.0.0",
  "board": "ESP32-WROVER-IE-N16R8",
  "flash_size_mb": 16,
  "ota_partition_size": 6553600,
  "ota_supported": true,
  "update_in_progress": false,
  "runtime_mode": "snapclient",
  "power_source": "battery",
  "channel_mode": "stereo",
  "dsp": {
    "enabled": true,
    "eq": {
      "low_shelf_hz": 120.0,
      "low_shelf_db": 0.0,
      "mid_hz": 1000.0,
      "mid_q": 0.80,
      "mid_db": 0.0,
      "high_shelf_hz": 8000.0,
      "high_shelf_db": 0.0
    },
    "left_gain_db": 0.0,
    "right_gain_db": 0.0,
    "balance": 0.00,
    "loudness": {
      "enabled": true,
      "bass_max_db": 3.0,
      "full_boost_volume": 0.30,
      "flat_volume": 0.80
    },
    "headroom_db": 0.0,
    "soft_limiter": {
      "enabled": true,
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
| `dsp` | object | Snapclient-only DSP configuration currently compiled into the firmware |
| `bluetooth_name` | string | Saved Bluetooth device name used on later Bluetooth-mode boots |
| `battery.available` | boolean | `true` when battery sensing is enabled and a reading is available |
| `battery.voltage` | number | Reconstructed 4S pack voltage in volts, not ADC divider voltage |
| `battery.percent` | number | Estimated 4S battery percentage, `0..100` |
| `capabilities.channel_modes` | string array | Channel modes accepted by `POST /api/channel-mode` |
| `capabilities.bluetooth_name` | boolean | `true` when `POST /api/bluetooth-name` is available |
| `capabilities.power_source` | boolean | `true` when `POST /api/power-source` is available |
| `capabilities.snapclient_dsp` | boolean | `true` when the Snapclient PCM DSP status object is available |
| `capabilities.firmware_update` | boolean | `true` when `POST /api/firmware` is available |

The `dsp` object is informational in this API version. Tune the EQ, channel
gains, balance, loudness bass boost, headroom, and limiter values in
`SNAPCLIENT_DSP_CONFIG` before building firmware.

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
