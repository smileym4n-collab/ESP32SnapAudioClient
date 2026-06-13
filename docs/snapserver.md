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

## Sample rates: why 44.1 kHz in but 48 kHz out

Opus only supports 48/24/16/12/8 kHz - it has no 44.1 kHz mode. So the `44100` in
the profile above is just the **source FIFO** rate (what librespot/Spotify feeds
in). Snapserver resamples that up to 48 kHz before Opus encoding, which it logs as:

```text
Resampling input from 44100:16:2 to 48000:16:2 as required by Opus
```

So the full chain is:

```text
Spotify / librespot (44.1 kHz)
  -> Snapserver resample to 48 kHz
  -> Opus encode @ 48 kHz
  -> Wi-Fi
  -> ESP32 Opus decode
  -> 48 kHz PCM
  -> I2S DAC
```

The ESP32 therefore always plays at **48 kHz** in Opus mode. You can confirm it in
the device serial log:

```text
[snapclient-pcm] format=48000 Hz, 16-bit, 2 ch
[i2s] format update=48000 Hz, 16-bit, 2 ch
```

Note: earlier PCM-transport builds ran the client at 44.1 kHz end to end. Moving to
Opus changed the on-device rate to 48 kHz - the 44.1 kHz now lives only on the
server's input side.

## Example `snapserver.conf` source line

```ini
[stream]
buffer = 2000
source = pipe:///tmp/snapfifo_spotify?name=Spotify&sampleformat=44100:16:2&codec=opus&chunk_ms=20
```

### About `buffer`

`buffer` (end-to-end latency, in ms) is the **master jitter cushion** for the
whole system. The ESP32's compressed queue only ever holds about this much audio
in steady state, so a generous value here does more for dropout resistance than
any client-side tuning. `2000` ms is a good starting point for music over Wi-Fi;
raise it if you still see dropouts at weaker signal, lower it only if you need
tighter sync. It does not affect Bluetooth mode.

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
