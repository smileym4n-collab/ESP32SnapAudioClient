# Snapserver Notes For ESP32 Audio Client

This branch's **Snapclient mode** expects an **Opus** Snapserver stream on an **ESP32-WROVER-IE-N16R8** target.

## Recommended stream profile

Use these settings for the stream this client connects to:

- codec: `opus`
- sample format: `44100:16:2`

That means the source FIFO is:

- 44100 Hz
- 16-bit samples
- 2 channels

Snapserver will encode that source to Opus for transport, and the ESP32 decodes Opus to 48 kHz PCM for I2S output.

## Example `snapserver.conf` source line

```ini
source = pipe:///tmp/snapfifo_spotify?name=Spotify&sampleformat=44100:16:2&codec=opus&chunk_ms=20
```

## Why this branch uses Opus

- the shared ESP32 I2S/DAC path has already been validated through Bluetooth mode
- PCM transport is too bandwidth-heavy for anything but a very strong ESP32 Wi-Fi signal
- Opus transport should reduce Wi-Fi bandwidth substantially
- this is a test branch intended to validate whether compressed transport is stable enough before replacing the PCM release line

## After changing Snapserver

Restart the server so the new stream settings are applied.

Typical Linux service command:

```bash
sudo systemctl restart snapserver
```

## What to watch for

- Opus adds CPU decode work on the ESP32
- Wi-Fi quality still matters, but transport bandwidth is much lower than PCM
- this build expects `codec=opus` with a `44100:16:2` FIFO input profile

## Bring-up recommendations

- keep the Snapserver on wired Ethernet if possible
- start with a strong 2.4 GHz Wi-Fi signal at the ESP32
- validate Snapclient mode before adding extra hardware changes around the DAC
