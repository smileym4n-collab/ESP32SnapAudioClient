# Mission 730 active speaker DSP — unreleased bench implementation

This working tree adds an electrical two-way crossover. It does not prescribe an
acoustic alignment for the unmeasured Mission drivers. `VERSION` remains `2.4.0`;
this is intentionally not tagged or published as a release before bench validation.

## Existing pipeline and integration

The existing Snapcast path is Opus decode → SnapOutput server volume and fixed
0.85 headroom factor → existing clock-drift resampler → AudioProbeStream.
`AudioProbeStream` still calls the existing `routeStereo16` helper with the live
companion-app channel setting. LEFT duplicates source L; RIGHT duplicates source R.
The DSP averages that selected pair to mono, so the chosen source channel feeds
both crossover branches. The existing `stereo` setting averages L/R at this new mono
boundary. The companion API, available channel-mode names, persistence, and HTTP
responses are unchanged; PCM5122 output mapping is only a shell DSP setting.

Bluetooth's existing stream-reader callback still calls AudioOutputController.
Its samples always become `0.5*L + 0.5*R`, independent of Snapcast channel mode.
The callback registration and Bluetooth transport are unchanged. The default-rate
assignment was moved immediately before A2DP start, so a negotiated-rate callback
during start cannot be overwritten by the fallback afterward.

AudioOutputController now runs the shared Engine before its existing I2S writer.
Snapcast's fixed final trim and OTA fade gate the processed output, including the
test tone. Existing restart gain handling remains in place. No source PCM can
bypass the crossover on either configured path. Invalid formats and malformed
partial frames are discarded/zeroed, not passed through as broadband HIGH audio.
The existing I2S DMA auto-clear handles underruns; no partial-frame assembler or
sample-rate conversion was added.

The input/output format remains interleaved signed 16-bit stereo. Snapcast uses
48 kHz; Bluetooth defaults to 44.1 kHz and follows negotiated-rate notifications.
DSP supports 32–96 kHz. Coefficients use the actual notified rate. A runtime rate
change that invalidates a configured frequency holds output at zero until the
settings are corrected; `dsp defaults` is the complete recovery option.

Existing scheduling is preserved: Snapcast receive task core 0, priority 5;
Opus/output task core 1, priority 4 with a 10 KiB stack. The compressed queue remains
131072 bytes. I2S settings remain Snapcast 24 × 1024, Bluetooth 8 × 512 in the
existing library's DMA configuration units. The library translates those settings
to DMA lengths. The DSP works in at most 128-frame/512-byte sub-blocks within the
existing writers. AudioProbeStream's growable routing buffer was replaced with a
fixed 512-byte buffer, eliminating allocation there.

## Files changed and scope boundary

| Files | Purpose |
| --- | --- |
| `include/audio_dsp.h`, `src/audio_dsp.cpp` | Portable DSP configuration, math, mailbox, filters, delay and oscillator |
| `include/dsp_control.h`, `src/dsp_control.cpp` | Existing Preferences integration, command handlers, main-loop coefficient preparation, ADC and optional PCM5122 control |
| `include/audio_output_controller.h`, `src/audio_output_controller.cpp` | Shared DSP-before-I2S boundary, source-rate notification and restart mute hook; atomic live channel-mode access |
| `include/audio_probe_stream.h` | Keep existing Snapcast source selection upstream; fixed routing blocks, DSP output handoff and final trim/OTA tone gate |
| `include/board_config.h` | Confirmed PCM5122 pins/address, disabled INA236 and remaining ADC placeholder |
| `include/mode_switch_controller.h`, `src/mode_switch_controller.cpp` | Extend the existing serial dispatcher with bounded DSP lines while retaining single-key mode commands |
| `src/main.cpp` | Initialize DSP before source startup and service controls from the existing main loop |
| `src/bluetooth_mode.cpp` | Move fallback-rate assignment before A2DP start so an early negotiated-rate callback survives |
| `scripts/test_dsp.py`, `tests/dsp/core_test.cpp`, `tests/dsp/control_test.cpp` | Host DSP, routing and shell/persistence checks |
| `tests/dsp/stubs/Arduino.h`, `tests/dsp/stubs/Preferences.h`, `tests/dsp/stubs/Wire.h` | Host-only hardware/persistence substitutes |
| `README.md`, `CHANGELOG.md`, `RELEASE-NOTES.md`, `docs/speaker-dsp.md` | Unreleased status, build evidence, configuration and bench instructions |

Outside the audio engine, only the serial dispatcher, startup/service hooks,
hardware configuration, and the narrow Bluetooth rate-order fix changed. No
Snapcast network, Wi-Fi, Bluetooth transport, companion API, OTA implementation,
task affinity, DMA sizing, dependency revision or partition configuration changed.

## Pin configuration

Confirmed DAC and volume-pot wiring in `include/board_config.h`:

```cpp
PCM5122_I2C_SDA_PIN = 21;
PCM5122_I2C_SCL_PIN = 22;
VOLUME_ADC_PIN = 35;       // IO35
```

GPIO35 is an ADC1 input-only pin and is connected to the pot wiper.
ADC2 is unsuitable during Wi-Fi use. The unused INA236 is disabled, allowing the
DAC to use GPIO21 on `Wire1`; do not re-enable it on overlapping pins. Both DAC
ADR straps are low, so
`PCM5122_I2C_ADDRESS` is the confirmed 7-bit address `0x4C`. Both
SDA/SCL must be configured together. Detected setup/unmute failures hold DSP at zero.

Pot ADC configuration is 12-bit, 11 dB attenuation, 20 ms polling, exponential
smoothing alpha 0.15, and 0.5 dB update hysteresis. Measure the usable raw endpoints
and edit `VOLUME_ADC_MIN/MAX` (provisional 0/4095); these are not calibrated voltage
endpoints. `VOLUME_MIN_DB` defaults to −60 dB. The bottom 2% mutes, the remaining
travel maps linearly in dB (logarithmic amplitude), and the top 0.5% reaches 0 dB.
The pot controls one common master target; it never changes driver trims. Ensure
the wiper voltage stays within the ESP32 input limits and check both endpoints
before connecting populated power amplifiers. During movement, throttled
`[dsp-pot]` serial lines show the raw reading, filtered reading, normalized
position and applied master gain for bench verification.

## Defaults and gain handling

- LR4 crossover: 2400 Hz, two Butterworth Q=1/sqrt(2) biquads per branch.
- HIGH → PCM5122 LEFT; LOW → PCM5122 RIGHT.
- HIGH/LOW trims 0 dB; both unmuted, normal polarity, zero delay.
- Six PEQ slots per branch, all bypassed (stored definition: peak, 1000 Hz,
  Q=0.70710678, 0 dB).
- Master 0 dB with ADC disabled; an enabled pot starts muted pending its reading.
- Tone off on every boot; default command level −30 dBFS.

The local master gain is in the DSP before the split. A configured pot owns this
same target; the shell master setter is then refused. Without a pot, the shell
controls it. PCM5122 digital volume is fixed at unity, so there is no second local
listening-volume stage. Existing upstream Snapcast/phone source volume remains
unchanged and can still attenuate programme audio; the internally generated tone
replaces that programme input and therefore bypasses upstream source volume.

DSP filtering uses floating point, preserving headroom between sections. Final
PCM conversion saturates to −32768…32767 and counts clipped samples in status.
There is no limiter, automatic headroom reduction, or acoustic driver protection.
Positive trims and overlapping PEQ boosts can clip; attenuate the master and/or
trims and check the clip counter. Saturation prevents integer wraparound but
clipped HIGH output can still be harmful. The provisional crossover alone does
not establish safe drive levels for an unmeasured tweeter.

PEQ uses the [RBJ biquad equations documented by W3C](https://www.w3.org/TR/audio-eq-cookbook/).
Shelves take **Q**, not REW shelf slope S; translate exported settings accordingly.

## Complete serial command reference

Use the existing 115200-baud serial connection. Project monitor settings enable
local echo and send-on-Enter; restart the monitor after updating the project.
Only one monitor may hold the serial port at a time. Commands are lowercase and finish
with CR or LF. Existing `b`, `s`, `t`, `?` and help shortcuts continue to work
outside DSP lines. Maximum DSP line length is 191 characters; overflow is discarded.
`<output>` means `high` or `low`; `<slot>` is an integer 1–6.

| Command | Effect / range |
| --- | --- |
| `dsp help` | Show command summary |
| `dsp status` | Desired settings, pending state, source/route, requested/applied Fs, coefficient validity, mapping, master target/ADC, every PEQ, mutes, polarity, delays/samples, DAC state, tone, clip counter and block timing |
| `dsp dac` | Read back PCM5122 PLL/divider and mute/format/volume settings plus detected clock, analogue mute, XSMUTE, short, boot and power-state monitors over I2C |
| `dsp crossover <Hz>` | 100…0.45 × Fs; LR4 remains mandatory |
| `dsp map high-left` | HIGH=LEFT, LOW=RIGHT |
| `dsp map high-right` | HIGH=RIGHT, LOW=LEFT |
| `dsp master <dB>` | −80…0; −80=mute; available only with ADC disabled |
| `dsp <output> gain <dB>` | −24…+6 |
| `dsp <output> mute on` / `off` | Mute/unmute that semantic output |
| `dsp <output> polarity normal` / `invert` | Set polarity |
| `dsp <output> delay <ms>` | 0…10 ms, rounded to the nearest sample; status shows sample count |
| `dsp <output> peq <slot> peak <Hz> <Q> <dB>` | Define and enable peaking EQ |
| `dsp <output> peq <slot> lowshelf <Hz> <Q> <dB>` | Define and enable low shelf |
| `dsp <output> peq <slot> highshelf <Hz> <Q> <dB>` | Define and enable high shelf |
| `dsp <output> peq <slot> notch <Hz> <Q>` | Define and enable notch |
| `dsp <output> peq <slot> on` / `off` | Enable / bypass existing definition |
| `dsp <output> peq <slot> clear` | Reset one slot to bypassed flat default |
| `dsp <output> peq clear` | Reset all six slots for that output |
| `dsp tone <Hz> [dBFS]` | Replace mono programme input; omitted level is −30 dBFS |
| `dsp tone off` | Fade back to normal programme input |
| `dsp defaults` | Restore all DSP defaults in RAM and turn tone off |
| `dsp save` | Save the complete current configuration, excluding tone |

PEQ frequency: 20…0.45 × Fs; Q: 0.1…20; gain: −24…+12 dB. Bypassed definitions
are also validated. Tone frequency: 20…0.45 × Fs; level: −80…−12 dBFS. Numbers must
be finite, and extra arguments or invalid values produce errors without changing
settings. High shelf Q can produce resonant overshoot; begin with 0.707.

Changes apply without reboot on incoming audio blocks. Topology, mapping, trim,
mute, delay, polarity and tone edits fade both outputs down over 10 ms, exchange
prepared settings at zero, clear all filter/delay state, and fade back over 10 ms.
This intentional short level dip avoids interpolating unstable coefficients or
crossfading LOW audio into the HIGH connector during a map change. Updates can
wait for the current exchange, with later RAM edits coalescing into the next one.
Master/pot changes use a per-sample gain slew taking at most 20 ms for a full-scale
change, without resetting filters. Existing queued I2S audio adds control latency.

## Persistence and startup

The existing Arduino `Preferences` mechanism stores one 232-byte blob in namespace
`speaker-dsp`, key `config`, schema 1. Size, schema, finite ranges, enums and generated
coefficients are checked on boot. Missing/incompatible/invalid data selects complete
defaults. Settings are shared by both sources; source-channel selection continues
to use its existing separate store. Tone frequency/level are outside the persisted
structure. Neither shell edits nor pot movement writes flash. `dsp defaults` does
not erase the saved configuration; only `dsp save` commits RAM settings. Pot
position is read live at boot and does not overwrite the saved shell-master value.

Save executes in the main loop, never the audio writer. ESP32 flash operations may
still stall execution even when requested from another task; pause streaming for
critical measurement saves if required. Hardware NVS endurance, power-loss recovery
and save-under-playback behaviour need bench verification.

DSP is held at zero initially. Setup loads/validates settings and prepares
coefficients before starting sources. If configured, PCM5122 is soft-muted before
I2S starts, set to 16-bit I2S, and left at 0 dB on both channels. I2S's existing DMA
initialization zeros its buffers. A complete prepared configuration must have been
accepted by the writer at the current rate before main-loop DAC unmute and DSP
ramp-up are allowed. Restart also requests DAC mute. Software-controlled three-wire
I2S explicitly selects BCK as the PLL reference and programs the TI ratio-32 clock
dividers for 44.1/48 kHz; missing-SCK detection is ignored while BCK, sample-rate
and PLL-lock detection remain active. Register choices follow the
[TI PCM5122 datasheet](https://www.ti.com/lit/ds/symlink/pcm5122.pdf).

With GPIOs disabled, protection is digital zeros and sample ramps; firmware cannot
control analogue power-up behaviour before the CPU runs. Verify DAC clocking, I2C
straps/pull-ups, analogue DC behaviour and startup transients at the pads before
fitting the amplifiers. Configuring I2C later requires a new build; mapping and DSP
parameters do not.

## Validation and resource use

Run `python3 scripts/test_dsp.py` for dependency-free C++11 host tests with address
and undefined-behaviour sanitizers. Tests exercise actual DSP code, the original
channel-routing helper, shell parsing and mocked Preferences restore. They cover
LR4 magnitude/phase over multiple crossover frequencies at 32/44.1/48/96 kHz;
PEQ responses; semantic mapping; L/R source selection before DSP; averaging;
startup/rate mismatch zeros; delays/polarity/mutes; saturation; tone on/off;
mailbox concurrency; invalid settings; defaults and tone exclusion from storage.
There was no pre-existing automated test framework. Mocks do not establish real
flash or DAC behaviour.

The ESP32 engine occupies 884 internal static bytes. Its two 961-sample float delay
lines and 1025-point sine table occupy a separate 11788-byte workspace allocated
once explicitly in PSRAM before source startup. There is no internal-heap fallback;
allocation failure holds output at zero. Default zero-delay programme playback does
not access the external sample buffers. The earlier all-static 12668-byte engine
exhausted internal RAM and prevented the 8192-byte Snapcast receive task from being
created on the user's board; this memory-placement fix preserves task/DMA sizes. Config is 232 bytes; each prepared bank is 360 bytes.
The Snapcast routing buffer and processed output buffer each use 512 bytes of
stack; existing gain-ramp scratch space is retained. Default processing runs four
biquads/frame; all twelve enabled PEQs raise that to sixteen. Bypassed PEQs cost no
filter operations. Coefficients/trigonometry, ADC/I2C, flash and command parsing
are outside the sample loop. The audio consumer uses acquire/release publication
without locks or waits; it retains only the existing I2S write backpressure.

`dsp status` reports worst observed DSP block wall time (including preemption,
excluding I2S waits), clipped sample count, and engine size. Compare 128 frames at
48 kHz with its 2667 µs audio duration, and at 44.1 kHz with 2902 µs. This is not a
CPU-utilization measurement and must leave ample time for Opus. Measure on-board
with all filters enabled and both sources before claiming dropout-free operation.
Build results and image size are recorded in `RELEASE-NOTES.md`.

## Scope procedure before installing LM1875s

1. Leave both LM1875s unpopulated. Probe the two input pads relative to the correct
   circuit ground using equal scope probe attenuation and channel settings. Confirm
   the PCB schematic identifies which pad corresponds to PCM5122 LEFT/RIGHT. Check
   DC levels first. Capture power-up, reset and source changes with no music and
   again with music ready; reject unexpected large pulses or broadband bursts.
2. Start a continuous Snapcast or Bluetooth stream (digital silence is suitable).
   Tone generation only advances when source PCM arrives; it does not create a
   new playback task. Run `dsp defaults`, `dsp map high-left`, `dsp master 0` with
   the pot disabled, then `dsp status`. If the pot is enabled, set it to maximum
   and confirm masterTarget=0 dB. Confirm Fs=48000 for the values below, both trims
   0, both unmuted/normal, delays 0, all PEQs bypassed, and no coefficient/DAC fault.
3. Run `dsp tone 100 -30`. RIGHT/LOW should dominate and LEFT/HIGH should be near
   the noise floor. Record LOW amplitude as its passband reference. At this level,
   the calculated HIGH leakage is below the 16-bit PCM resolution; do not infer
   exact stopband attenuation from a scope noise-floor reading.
4. Run `dsp tone 10000 -30`. LEFT/HIGH should dominate; RIGHT/LOW should be strongly
   attenuated. Record HIGH amplitude as its passband reference. Do not expect the
   two pad voltages to be identical unless the analogue paths have identical gain.
5. Run `dsp tone 2400 -30`. Each output should be approximately half its OWN
   passband reference voltage (−6.02 dB), with HIGH/LOW electrical phase aligned
   modulo 360° before any analogue inversion. The digital sum is unity magnitude;
   verify using scope math only, never physically short the DAC outputs together.
6. Run `dsp map high-right`: the HIGH/LOW frequency roles must exchange pads. Return
   to `dsp map high-left`. Test `dsp high mute on/off`, `dsp low mute on/off`, a
   `dsp high gain -6` change (approximately half voltage), and polarity inversion
   at 2.4 kHz (180° relative change). A `dsp high delay 1` adds 48 samples at 48 kHz;
   use a lower-frequency tone/phase measurement to resolve phase wrapping. Restore
   defaults between checks; use `dsp status` to verify the actual configuration.
7. Turn tone off and play distinct left-only/right-only material. In Snapcast,
   select LEFT/RIGHT through the existing companion app: only that source channel
   must feed both HIGH/LOW branches. In Bluetooth, equal L-only and R-only tests
   should give equal output levels, each 6.02 dB below identical in-phase L+R.
   Opposite-phase L/R should cancel.
8. Make a distinctive trim/map/PEQ/delay change, run `dsp save`, enable a tone and
   reboot. Confirm settings restore and tone is OFF. Try `dsp defaults` without
   saving, reboot, and confirm the saved tuning returns. End with `dsp defaults`
   and `dsp save` if those are the intended starting settings. Verify pot mute,
   smooth sweeps and maximum before fitting the amplifiers.

After REW/UMIK measurements, revisit crossover frequency, acoustic slopes,
sensitivity trims, PEQ, polarity and alignment delay. No driver response,
impedance, sensitivity, enclosure alignment or acoustic phase was assumed. This
implementation does not include fractional delay, a protection limiter, or a
standalone idle-source tone clock.
