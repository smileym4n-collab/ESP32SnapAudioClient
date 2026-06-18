Release and versioning rules for this repository:

- `VERSION` is the canonical firmware version source.
- Firmware versions use semantic versioning in `major.minor.patch` form.
- If a prompt changes firmware behavior, API output, build configuration, release process, or user-facing documentation and the work is intended to be pushed as a release-ready change, increment `VERSION` in the same change. Do not leave release-ready firmware changes on the previous released version.
- If a prompt makes code or documentation changes but is intentionally not release-ready, keep `VERSION` unchanged and put concise notes under `## [Unreleased]` in `CHANGELOG.md`.
- Small releases, bug fixes, documentation-only release corrections, and low-risk firmware tweaks increment the patch version: `v1.0.x`.
- Medium releases, new backward-compatible features, meaningful API additions, or notable behavior changes increment the minor version and reset patch to zero: `v1.x.0`.
- Large releases, breaking API changes, incompatible OTA/partition behavior, major hardware target changes, or substantial firmware rewrites increment the major version and reset minor and patch to zero: `vx.0.0`.
- Git tags must use the leading `v` form, for example `v1.0.1`.
- The value in `VERSION` must not include the leading `v`.
- Before creating or pushing a release tag, make sure the tag version exactly matches `VERSION`.
- Do not hard-code firmware versions in source files when the build can use `VERSION` instead.
- Before finishing release-ready work, run a version sweep for the old version number and update every visible firmware identity example, including `README.md`, `API.md`, `docs/control-api.md`, `ota-firmware-update-contract.md`, `RELEASE-NOTES.md`, source file version comments, and `APP_FIRMWARE_VERSION_TAG`.

Release documentation rules:

- Keep `CHANGELOG.md` updated for every prompt that changes repository behavior, release process, firmware behavior, API output, build configuration, or user-facing documentation.
- Keep `RELEASE-NOTES.md` updated for every release-oriented prompt, firmware version change, OTA-visible behavior change, API/status change, or build artifact change.
- Use `RELEASE-NOTES.md` as the release notes file. Do not create or reference `RELEASENOTES.md`.
- For unreleased work, add concise notes under `## [Unreleased]` unless the current task is explicitly preparing a named release.
- For a named release, move the relevant release notes into that version section and ensure `RELEASE-NOTES.md` title, visible version fields, and manual test checklist match the firmware version.
- When changing the version, update all visible examples that report `version` or `firmwareVersion`, including API docs and OTA contract docs.
- After versioning changes, run `pio run` when practical and note whether the build passed.
- If `pio run` changes the built firmware image size, update the image size in `RELEASE-NOTES.md`.
