# Local control API

Snapclient mode exposes a small HTTP API for companion apps. This API is for ESP32-specific controls that Snapserver does not expose, such as local channel routing, the saved Bluetooth device name, and local OTA firmware uploads.

Bluetooth mode does not expose this API and ignores the saved channel-routing preference.

## Base URL

```text
http://<client-ip>:8080
```

The companion app can get `<client-ip>` from Snapserver status using the Snapcast client host IP.

## Status

```text
GET /api/status
```

Example response:

```json
{
  "project": "ESP32 Audio Client",
  "version": "2.4.0",
  "firmwareVersion": "2.4.0",
  "board": "ESP32-WROVER-IE-N16R8",
  "flash_size_mb": 16,
  "ota_partition_size": 6553600,
  "ota_supported": true,
  "update_in_progress": false,
  "runtime_mode": "snapclient",
  "power_source": "battery",
  "channel_mode": "stereo",
  "bluetooth_name": "CoolCube",
  "battery": {
    "available": true,
    "voltage": 16.42,
    "percent": 95,
    "current": 1.238,
    "power": 20.32
  },
  "capabilities": {
    "channel_modes": ["stereo", "left", "right"],
    "bluetooth_name": true,
    "power_source": true,
    "battery_telemetry": true,
    "snapclient_dsp": false,
    "snapclient_dsp_update": false,
    "firmware_update": true
  }
}
```

Battery fields:

- `power_source`: saved power source setting, either `battery` or `mains`
- `available`: `true` when the INA236 is detected and a reading has been taken
- `voltage`: INA236 bus/4S pack voltage in volts
- `percent`: estimated 4S state of charge from the firmware lookup curve
- `current`: signed current in amperes; positive for load draw from `IN+` to `IN-`
- `power`: power usage in watts
- `capabilities.battery_telemetry`: `true` when these INA236 fields are supported

OTA fields:

- `ota_supported`: `true` when the firmware upload endpoint is available
- `update_in_progress`: `true` while a firmware upload is active
- `ota_partition_size`: inactive OTA app partition size in bytes, or `0` if unavailable
- `capabilities.firmware_update`: `true` when `POST /api/firmware` is available

If INA236 monitoring is disabled, the device is absent, or I2C communication
fails, the response includes:

```json
{
  "battery": {
    "available": false
  }
}
```

Mains-powered devices also report `battery.available: false`. Companion apps
should hide battery UI when `power_source` is `mains`.

## Set Channel Mode

```text
POST /api/channel-mode
Content-Type: application/json
```

Request body:

```json
{
  "channel_mode": "left"
}
```

Allowed values:

- `stereo`: left DAC channel plays left, right DAC channel plays right
- `left`: both DAC channels play the left input channel
- `right`: both DAC channels play the right input channel

The selected mode is saved in ESP32 preferences and restored on later Snapclient boots.

Successful responses return the same shape as `GET /api/status`.

## Set Power Source

```text
POST /api/power-source
Content-Type: application/json
```

Request body:

```json
{
  "power_source": "mains"
}
```

Allowed values:

- `battery`: report INA236 voltage, percentage, current, and power when available
- `mains`: suppress battery reporting so companion apps can hide battery UI

The selected power source is saved in ESP32 preferences and restored on later
Snapclient boots, including after OTA updates.

Successful responses return the same shape as `GET /api/status`.

## Upload Firmware

```text
POST /api/firmware
Content-Type: application/octet-stream
Content-Length: <firmware size in bytes>
X-Firmware-Filename: firmware.bin
```

Request body:

- raw PlatformIO firmware `.bin` app image
- no multipart wrapper
- no JSON envelope

The firmware writes the image to the inactive OTA partition, validates it, marks it as the next boot partition, returns success, and then reboots.

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
  "error": "invalid_image",
  "message": "Firmware image has an invalid ESP header"
}
```

## Set Bluetooth Name

```text
POST /api/bluetooth-name
Content-Type: application/json
```

Request body:

```json
{
  "bluetooth_name": "CoolCube Kitchen"
}
```

The selected name is saved in ESP32 preferences and used the next time the device boots into Bluetooth mode. It does not change Snapclient channel routing, and Bluetooth mode still does not expose this API.

Allowed value:

- `bluetooth_name`: 1 to 31 printable ASCII characters, excluding `"` and `\`

Successful responses return the same shape as `GET /api/status`.
