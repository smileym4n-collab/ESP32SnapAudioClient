# EQ and DSP operator guide

This guide covers the runtime serial controls for the Mission 730 two-way DSP.
Connect at 115200 baud, type each command in lowercase, and press Enter. Start
with `dsp status` to inspect the complete active configuration.

## Current verified configuration

The most recent live serial status showed the following settings. The HTTP API
confirms the unit is currently in Snapclient mode with source channel `left`, but
does not expose DSP details.

| Setting | Current value |
| --- | --- |
| Source | Snapcast, LEFT source channel duplicated to mono |
| Sample rate | 48 kHz |
| Crossover | 2400 Hz, fourth-order Linkwitz-Riley (LR4) |
| Output mapping | HIGH to PCM5122 LEFT; LOW to PCM5122 RIGHT |
| Master gain | 0 dB, controlled by serial shell because no pot is fitted |
| HIGH output | 0 dB, unmuted, normal polarity, 0 ms delay |
| LOW output | 0 dB, unmuted, normal polarity, 0 ms delay |
| HIGH PEQ 1–6 | All bypassed; default definition 1000 Hz peak, Q 0.707, 0 dB |
| LOW PEQ 1–6 | All bypassed; default definition 1000 Hz peak, Q 0.707, 0 dB |
| Test tone | Off |
| Persistence | Defaults loaded because no valid `speaker-dsp` NVS setting exists |

Run `dsp status` again whenever there is doubt. `settings=NVS` means a saved
configuration was loaded; `settings=RAM` means there are unsaved changes.

## Safe adjustment workflow

1. Reduce master level before making broad boosts:

   ```text
   dsp master -12
   ```

2. Apply one change, then inspect it:

   ```text
   dsp high peq 1 peak 3500 1.2 -2.5
   dsp status
   ```

3. Listen or measure both branches. Check that the status clip count is not
   increasing. Positive EQ and output gain consume headroom; there is no limiter.
4. Continue editing in RAM. Only save after the complete setup is verified:

   ```text
   dsp save
   ```

Edits take effect without a reboot. Most changes use a short mute/update/ramp
sequence to avoid unstable intermediate filter coefficients. If `pending=yes`,
the new settings are waiting for another incoming PCM block; confirm the source
is actually delivering audio.

## Crossover and output controls

Change the mandatory LR4 crossover frequency:

```text
dsp crossover 2200
```

The allowed range is 100 Hz to 0.45 times the current sample rate. This changes
both complementary branches together; independent crossover slopes are not
available.

Map the semantic driver outputs to the DAC channels:

```text
dsp map high-left
dsp map high-right
```

Adjust an individual branch:

```text
dsp high gain -3
dsp low mute on
dsp low mute off
dsp high polarity invert
dsp high polarity normal
dsp low delay 0.35
```

Output gain accepts -24 to +6 dB. Delay accepts 0 to 10 ms and is rounded to the
nearest sample. Use polarity and delay only after measuring acoustic phase and
driver arrival time.

## Parametric EQ

Each semantic output (`high` or `low`) has six independent PEQ slots. Defining a
filter also enables that slot.

Peaking filter:

```text
dsp high peq 1 peak 4200 1.4 -3
```

Low shelf:

```text
dsp low peq 1 lowshelf 180 0.707 2
```

High shelf:

```text
dsp high peq 2 highshelf 8000 0.707 -2
```

Notch filter:

```text
dsp low peq 3 notch 620 5
```

Limits are 20 Hz to 0.45 times the sample rate, Q 0.1–20, and gain -24 to +12 dB.
Shelf filters use Q rather than REW's shelf-slope `S`; do not copy an `S` value
into the Q field without conversion.

Temporarily bypass and re-enable an existing definition:

```text
dsp high peq 1 off
dsp high peq 1 on
```

Reset one slot or all slots on a branch:

```text
dsp high peq 1 clear
dsp high peq clear
```

`off` preserves the filter parameters; `clear` replaces them with the flat
default definition.

## Master level and test tones

The volume-pot wiper is mapped to GPIO35 and owns the common master
gain. The following shell commands are only available when the pot input is
disabled in `board_config.h`:

```text
dsp master -20
dsp master 0
dsp master -80
```

The range is -80 to 0 dB; -80 dB is mute. With GPIO35 configured, the shell
command is refused. The current ADC endpoints are provisional and should be
calibrated from the actual pot's minimum and maximum readings.

While the pot moves, the serial terminal emits a throttled diagnostic such as:

```text
[dsp-pot] GPIO35 raw=2048 filtered=2039.4 position=49.8% master=-30.72 dB
```

It reports at most four times per second and only after a meaningful ADC change.

Generate a source-clocked test tone:

```text
dsp tone 1000 -40
dsp tone off
```

Tone range is 20 Hz to 0.45 times the sample rate and -80 to -12 dBFS. The tone
replaces programme input but still needs incoming PCM callbacks, so it will not
run while the Snapcast output pipeline is idle. Tone settings are never saved.

## Saving, reverting and recovery

Show everything currently active:

```text
dsp status
```

Save the complete configuration, excluding the test tone:

```text
dsp save
```

Restore defaults in RAM without overwriting a previously saved configuration:

```text
dsp defaults
```

To make defaults persistent, run both commands:

```text
dsp defaults
dsp save
```

Invalid commands and out-of-range settings are rejected without changing the
configuration. If a sample-rate change makes a saved filter frequency invalid,
the firmware holds output at zero; use `dsp status`, correct the offending
frequency, or restore defaults.

## DAC diagnostics

```text
dsp dac
```

This confirms PCM5122 I2C communication and reports the programmed BCK-referenced
PLL/dividers, digital mute and volume, detected clocks, analogue mute, XSMUTE,
short detection, boot completion and power state. During healthy playback the
PLL should be locked, the active clock-error bit clear, and power state should be
`run`.

For implementation details, resource use, and the full electrical bench
procedure, see [speaker-dsp.md](speaker-dsp.md).
