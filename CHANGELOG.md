# CHANGELOG

## [Unreleased]

## [2.1.1] - 2026-06-18

- Added CORS headers and `OPTIONS` preflight handling to the local control API so browser-based companion apps can reliably send JSON updates without losing contact with the ESP32.
- Updated visible firmware version fields and release documentation for `2.1.1`.

## [2.1.0] - 2026-06-18

- Added writable Snapclient DSP settings: `GET /api/dsp`, partial `POST /api/dsp`, and `POST /api/dsp/reset`.
- Reworked Snapclient DSP control around commercial-style EQ presets, with 14 firmware profiles, a 5-band biquad preset engine, layered bass boost, loudness on/off, and balance as the companion-app friendly controls.
- Persisted DSP settings in ESP32 preferences/NVS, with invalid stored values falling back to `SNAPCLIENT_DSP_CONFIG`.
- Applied DSP changes live to the Snapclient PCM stage while audio is running, with clamped ranges for bass boost, channel gain, balance, loudness boost, headroom, and limiter ceiling.
- Added `capabilities.snapclient_dsp_update` to `/api/status`.
- Updated visible firmware version fields and release documentation for `2.1.0`.

## [2.0.0] - 2026-06-18

- Added a Snapclient-only EQ/DSP stage after Opus decode and channel routing: 3-band biquad EQ (low shelf, mid peaking, high shelf), per-channel gain/balance, volume-aware loudness bass boost, optional output headroom, and a final soft limiter. Bluetooth mode remains unchanged.
- Added compiled DSP settings to `/api/status` under `dsp` and advertised `capabilities.snapclient_dsp` so companion apps can display the active firmware tuning.
- Documented `SNAPCLIENT_DSP_CONFIG` as the firmware tuning point for the first DSP implementation.
- Updated visible firmware version fields and release documentation for `2.0.0`.

## [1.3.1] - 2026-06-13

- Quiesce the Snapclient audio pipeline when an OTA firmware upload starts: the music fades out quickly, then the decode and network tasks are stopped and I2S is flushed, so the flash write and HTTP upload get an idle device instead of competing with Opus decode and the Snapcast stream for CPU and Wi-Fi. This makes OTA updates far more reliable while music is playing. If an OTA fails after audio was stopped, the device reboots to recover playback (the update does not commit, so it stays on the current firmware).
- Removed the unused optional I2S MCLK support and its `board_config.h` settings. The firmware never drives an MCLK pin; PCM5102-style DACs derive their clocks from BCLK.
- Cleaned up the repository documentation for sharing: rewrote the README to be friendlier and better structured, removed MCLK references throughout, and aligned the visible firmware version fields across the docs.
- Updated visible firmware version fields and release documentation for `1.3.1`.

## [1.3.0] - 2026-06-13

- Fixed Snapclient channel routing: `stereo` / `left` / `right` now actually re-routes the decoded audio. The setting was previously saved and reported over the API but never applied, because the Snapclient output wrote straight to the I2S stream and bypassed the routing path. Routing now runs in the final PCM output probe, so `POST /api/channel-mode` takes effect immediately with no reboot.
- Moved the Snapclient network receive task to core 0 so the Opus decode/output task (pinned to core 1 by the Snapclient library) gets a dedicated core. This targets the periodic short play/pause stutter caused by packet receive preempting Opus decode on the shared core.
- Hardened the compressed Snapclient queue against byte-buffer overflow: an oversized chunk is now dropped whole instead of being partially written, keeping the size queue and byte buffer in lockstep rather than desyncing Opus frame boundaries.
- Retuned the Snapclient rebuffer thresholds from `10% -> 40%` to `5% -> 15%` so a low-buffer refill is a short blip instead of a multi-second silence the higher resume target could force.
- Added a Wi-Fi loss grace period: Snapclient mode now waits up to five 1 s monitor checks (and nudges an explicit reconnect) before falling back to a restart, so a brief link blip no longer reboots the device mid-playback.
- Reserved the consume-side chunk buffer up front to avoid a mid-stream allocation in the audio copy task.
- Removed unused PCM-era code (`SnapcastPcmDecoder`, `PcmProbePrint`) left over from before the Opus transport switch.
- Left the Bluetooth A2DP -> I2S output path unchanged.
- Updated visible firmware version fields and release documentation for `1.3.0`.

## [1.2.1] - 2026-06-13

- Raised the Snapclient network receive task priority above the Opus decode/output task so compressed packets keep filling while audio is decoded.
- Lowered the Opus decode/output RTOS task priority from `5` to `4`.
- Removed the extra fixed 1 ms delay after each Snapclient loop pass and reduced the processor fast-loop yield from 5 ms to 1 ms.
- Updated visible firmware version fields and release documentation for `1.2.1`.

## [1.2.0] - 2026-06-13

- Switched Snapclient mode from the project-local PCM decoder to the upstream `OpusAudioDecoder` for compressed Snapcast transport testing.
- Changed the Snapclient output fallback format to 48 kHz, 16-bit, stereo to match Opus decode output.
- Retuned the Snapclient compressed transport queue to start at `20%` and rebuffer at `10% -> 40%` while keeping a `131072` byte queue.
- Updated Snapserver documentation for `codec=opus` with a `44100:16:2` librespot FIFO input profile.
- Updated visible firmware version fields and release documentation for `1.2.0`.

## [1.1.4] - 2026-06-12

- Increased the Snapclient PCM output queue from `65536` bytes to `131072` bytes to provide more Wi-Fi jitter cushion for 44.1 kHz PCM playback.
- Re-enabled Snapclient rebuffering so low queue fill triggers a short refill pause instead of playing through underruns as distortion.
- Raised the Snapclient initial activation threshold to `85%` and changed rebuffer thresholds to `35% -> 80%`.
- Updated visible firmware version fields and release documentation for `1.1.4`.

## [1.1.3] - 2026-06-10

- Changed the Snapclient PCM stream profile from `48000:16:2` to `44100:16:2` so the firmware default matches 44.1 kHz librespot and Snapserver sources.
- Updated the Snapserver setup documentation, API examples, OTA contract examples, and visible firmware version fields for `1.1.3`.

## [1.1.2] - 2026-05-27

- Increased Snapclient effective output level by changing `SNAPCLIENT_OUTPUT_GAIN` from `0.70` to `0.85` and `SNAPCLIENT_FINAL_PCM_GAIN` from `0.50` to `1.00`.
- Patched Snapclient server-settings handling so volume-only changes no longer trigger a silence burst through the mute path.
- Raised the Snapclient RTOS output-drain task priority above the Snapclient network loop so queued PCM gets handed to I2S with less scheduling jitter.
- Removed the extra 1 ms output-task delay after successful queued PCM writes, allowing the task to drain available audio chunks continuously.
- Stopped hot-path PCM peak/stat accumulation when periodic audio stats are disabled.
- Clarified local build and release documentation so VS Code links and OTA firmware paths are portable across Windows, macOS, and Linux.
- Ignored macOS `.DS_Store` metadata files so local VS Code work does not add release-noise files.

## [1.1.1] - 2026-05-09

- Added a low-battery warning LED on GPIO14 for the red RGB LED channel.
- Changed status LED behavior so battery-powered devices at or below 20% alternate once per second between the active mode LED and the red low-battery LED.
- Kept the low-battery LED disabled when the saved power source is `mains` or battery sensing is unavailable.

## [1.1.0] - 2026-05-08

- Added a persisted `battery` / `mains` power-source setting for Snapclient mode.
- Added `power_source` and `capabilities.power_source` to `/api/status`.
- Added `POST /api/power-source` so companion apps can change the saved power source after deployment.
- Suppressed battery voltage/percentage reporting when the saved power source is `mains`.

## [1.0.3] - 2026-05-08

- Changed Snapclient/Wi-Fi LED behavior so it blinks while connecting to Wi-Fi and stays solid once connected.

## [1.0.2] - 2026-05-07

- Removed Snapclient playback-idle restarts so the device no longer reboots when powered on with no audio playing.
- Changed the reported project name from `ESP32 Audio Client v9.30` to `ESP32 Audio Client`; firmware build numbers remain in `version` and `firmwareVersion`.
- Changed release notes so the "Changes Since" section only lists changes from the immediately previous firmware version.

## [1.0.1] - 2026-05-07

- Fixed Snapclient startup behavior so an initial Wi-Fi connection failure no longer causes a reboot loop.
- Added Wi-Fi startup retry handling after connection failure.
- Added serial Wi-Fi failure diagnostics showing the configured SSID, connection status, scan count, whether the target SSID is visible, and best RSSI when found.
- Changed release firmware builds to require Wi-Fi credentials from GitHub Actions secrets instead of silently compiling the placeholder example credentials.
- Removed the changelog file from GitHub release artifacts; release summaries now live in `RELEASE-NOTES.md`.

## [1.0.0] - 2026-05-07

- Promoted the firmware release line to `v1.0.0`.
- Added a repo-root `VERSION` file as the canonical firmware version source.
- Injected the firmware version into PlatformIO builds so serial boot logs, `/api/status`, and the Snapserver hello version stay aligned.
- Added a release workflow check that requires `v*` tags to match `VERSION`.
- Added agent release guidance for semantic version bumps and release documentation updates.

## [0.14.0] - 2026-05-07

- Added Snapclient-mode OTA firmware update support through `POST /api/firmware`.
- Added OTA capability, update state, board, flash size, and OTA partition size fields to `/api/status`.
- Clarified the OTA firmware update contract for SnapControl app implementation and firmware endpoint behavior.
- Removed historical change-summary sections from the README so version notes live only in this changelog.

## [0.13.4] - 2026-05-07

- Added Snapclient restart diagnostics for heap, Wi-Fi, task state, and PCM queue counters before automatic recovery restarts.
- Checked Snapclient loop task creation and fail cleanly with diagnostics if the task cannot be started.
- Stopped the Snapclient loop and processor before restart muting so mode/recovery restarts quiesce audio writes first.

## [0.13.3] - 2026-05-06

- Added automatic Snapclient-mode restart after a sustained decoded-output idle timeout so stalled Spotify/Snapserver playback can recover without manual mode switching.

## [0.13.2] - 2026-05-06

- Changed the Wi-Fi and Bluetooth status LED defaults to active-low for common-anode RGB LED wiring.

## [0.13.1] - 2026-05-06

- Changed Bluetooth LED behavior so it blinks while waiting for a source and stays solid when a Bluetooth client is connected.

## [0.13.0] - 2026-05-06

- Added a Snapclient-mode control API endpoint for saving the Bluetooth device name used on later Bluetooth boots.

## [0.12.3] - 2026-05-06

- Added short software audio fade-in on I2S startup and fade-to-mute before mode-switch restarts to reduce clicks and pops.

## [0.12.2] - 2026-05-06

- Reduced the Bluetooth-mode I2S DMA buffer footprint so I2S can start after the Bluetooth stack has allocated its internal task.

## [0.12.1] - 2026-05-06

- Started the Bluetooth A2DP stack before opening I2S so Bluetooth mode can allocate its internal task before the audio DMA buffers.

## [0.12.0] - 2026-05-06

- Updated the board pinout for SENSE on GPIO34, the mode button on GPIO23, and dedicated active-high Wi-Fi and Bluetooth LEDs on GPIO32/GPIO33.

## [0.11.0] - 2026-05-06

- Added configurable 4S battery monitoring for Snapclient mode using an ADC1 battery divider input.
- Added pack battery voltage and estimated percentage to the Snapclient control API status response.
- Added `API.md` as a compact companion-app API reference.

## [0.10.1] - 2026-05-06

- Added `firmwareVersion` to the Snapclient control API status response and aligned the Snapserver-reported Snapclient version with the firmware version.

## [0.10.0]

- Added a Snapclient-mode HTTP control API for companion apps to read firmware capabilities and set local channel routing.
- Added persistent Snapclient channel routing modes: stereo, left-to-both-DAC-channels, and right-to-both-DAC-channels.
- Documented the SnapApp control API and clarified that Bluetooth mode ignores local channel routing.

## [0.9.29]

- Moved private Wi-Fi credentials out of the committed configuration and into a local `include/secrets.h` file.
- Added `include/secrets.example.h` as the copyable template for local builds.

## [0.9.28]

- Replaced the fixed `1.0x` Snapclient timing path with a tightly clamped dynamic sync so small long-run clock drift can be corrected without audible pause-and-refill behavior.
- Re-enabled the Snapclient resampler for gentle drift correction while limiting it to a very narrow range around unity.
- Disabled the hard Snapclient rebuffer intervention by default, since the repeated stop-and-refill cycle itself was becoming audible.

## [0.9.27]

- Kept the deeper queue and rebuffer behavior from `0.9.26`.
- Stopped printing repeated `rebuffer-start` and `rebuffer-end` lines while periodic stats are disabled.
- Left warning and fault logging intact so real failures still show up in the serial monitor.

## [0.9.26]

- Increased the Snapclient queue depth and raised the startup/resume cushion so the WROVER keeps a healthier PCM reserve during Wi-Fi jitter.
- Made the low-buffer rebuffer thresholds explicit and more conservative so playback recovers before the live queue is nearly empty.
- Reduced repeated `sync-wait` startup chatter so the serial monitor stays quieter during bring-up.

## [0.9.25]

- Added Snapclient queue re-buffering when the live PCM cushion falls below a low-water mark.
- Added explicit `rebuffer-start` and `rebuffer-end` log lines for the low-buffer recovery path.

## [0.9.24]

- Added a one-shot Snapclient PCM first-write probe so early decoded output can be verified without re-enabling continuous log spam.
- Kept the log volume low so live playback testing still avoids the old serial flood.

## [0.9.23]

- Fixed a Snapclient boot crash caused by re-opening the shared I2S stream during PCM codec-header setup.
- Kept the shared Bluetooth/I2S output ownership unchanged and limited the fix to the Snapclient wrapper path.

## [0.9.22]

- Disabled periodic Snapclient PCM and queue-stat heartbeat logs by default during live playback testing.
- Kept startup, format-change, and warning/error logs active so failure states still show up clearly.

## [0.9.21]

- Reduced the ESP32 core log level so verbose per-packet Snapclient library info logs no longer run during normal playback.
- Moved the Snapclient processing loop back onto its own RTOS task to match the earlier stable bench profile more closely.
- Added a startup log for the dedicated Snapclient task configuration.

## [0.9.20]

- Switched the Snapclient PCM path to fixed Snapcast timing so the ESP32 no longer chases dynamic playback-factor updates during normal PCM playback.
- Disabled the Snapclient-only resampler/boost stage in the project-local Snap output path to prioritize clean PCM output over elastic clock correction.
- Added startup logs for the fixed-sync factor and intentionally disabled Snapclient resampler.

## [0.9.19]

- Added a final Snapclient-only PCM gain stage immediately before I2S so the actual outgoing samples have guaranteed headroom.
- Added a startup log for the final Snapclient PCM gain.
- Left Bluetooth mode, mode switching, and the shared I2S output path unchanged.

## [0.9.18 and earlier]

- Added a Snapclient-only gain trim before the shared resampler/output path to reduce distortion from full-scale PCM material.
- Fixed the Snapclient PCM header handoff so the parsed WAV header format is pushed into the active downstream stream even when the library exposes the target as `Print`.
- Added clearer Snapclient PCM header and I2S format-update logging.
- Reverted Snapclient mode from Opus back to the project-local PCM decoder after isolating distortion and stop behavior to the Opus path.
- Added focused Snapclient queue, PCM-format, playback-idle, and PCM activity logging so the serial monitor shows where playback stalls.
- Retargeted the firmware to the ESP32-WROVER-IE-N16R8 with a project-local PlatformIO board definition and 16 MB flash partitioning.
- Added centralized board pin mapping, including external DAC MCLK output and a boot-time mode select button.
- Split the firmware into boot-selected Snapclient and Bluetooth receiver modes while keeping the external I2S DAC path shared.
- Enabled PSRAM-aware buffering for more stable playback on the WROVER hardware.
- Updated the README and Snapserver notes for the `0.4.0` prototype revision.
- Changed the boot selector to a momentary startup button with Snapclient as the default mode.
- Added a dedicated mode-status LED pin with steady Snapclient indication and blinking Bluetooth indication.
- Changed the mode button so cold boot always starts in Snapclient and runtime button presses reboot into the opposite mode.
- Made I2S MCLK optional through a central board configuration flag, with MCLK disabled by default for PCM5102-style builds.
- Fixed Snapclient PCM playback by replacing the generic WAV decoder with a local decoder that handles the truncated Snapcast PCM wrapper correctly.
- Added serial commands to switch between Snapclient and Bluetooth modes without the physical mode button during bring-up.
- Fixed the reboot handoff for requested mode changes so the next mode survives reset reliably.
- Changed Snapclient mode to use a fixed playback sync factor for more stable PCM bring-up on the ESP32 target.
- Switched Snapclient mode over to the upstream Opus decoder path and updated the Snapserver documentation to use `codec=opus`.
- Switched the Opus Snapclient path back to dynamic clock synchronization and restored a positive `172 ms` processing lag for better playback timing.
- Reverted the experimental dynamic Opus timing change after it caused silence, returning Snapclient mode to the earlier fixed-sync Opus behavior.
- Increased the Snapclient Opus queue and I2S DMA buffering to improve playback stability on the WROVER hardware.
- Fixed the boot loop from the oversize I2S DMA buffer setting and clamped the DMA size to the ESP32 driver's valid range.
- Lowered the Snapclient queue activation threshold so the larger Opus queue starts playback earlier instead of waiting for a near-full buffer.
- Enabled info-level runtime logging to expose the Snapclient queue and synchronization behavior during silent Opus playback debugging.
- Increased the Snapclient RTOS queue entry slot count after logs showed `size_queue full` while plenty of byte-buffer space was still available.
- Added a project-local Snapclient output wrapper so Opus decoder startup failures are logged clearly instead of silently returning zero-byte writes.
- Added a safe fallback to the configured 48 kHz, 16-bit, stereo format when Snapclient Opus audio info arrives invalid.
- Reduced the Snapclient Opus queue size and activation threshold after logs showed the RTOS output task was waiting too long to start playback.
