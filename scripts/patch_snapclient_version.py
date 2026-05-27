from pathlib import Path
import re

Import("env")


def firmware_version():
    version_path = Path(env.subst("$PROJECT_DIR")) / "VERSION"
    version = version_path.read_text(encoding="utf-8").strip()
    return version[1:] if version.startswith("v") else version


def patch_snapclient_version(source=None, target=None, env=None):
    # The upstream Snapclient library hard-codes the hello version Snapserver sees.
    # It also writes a burst of silence on every server-settings update, because
    # settings handling calls setMute() even when only volume changed.
    libdeps_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    processor_path = (
        libdeps_dir
        / env.subst("$PIOENV")
        / "snapclient"
        / "src"
        / "api"
        / "SnapProcessor.h"
    )

    if not processor_path.exists():
        print(f"[snapclient-version] {processor_path} not found yet")
        return

    version = firmware_version()
    text = processor_path.read_text(encoding="utf-8")
    patched = re.sub(
        r'hello_message\.version\s*=\s*"[^"]+";',
        f'hello_message.version = "{version}";',
        text,
        count=1,
    )

    if patched == text and f'hello_message.version = "{version}";' not in text:
        raise RuntimeError("Snapclient hello_message.version assignment not found")

    old_settings = """    // set volume
    if (header_received) {
      setMute(server_settings_message.muted);
    }
    setVolume(0.01f * server_settings_message.volume);

    if (server_settings_message.muted != client_state_muted) {
      client_state_muted = server_settings_message.muted;
    }
"""
    new_settings = """    // set mute only when it changes; setMute() writes silence in this
    // library, so calling it for volume-only updates causes an audible dropout.
    if (header_received && server_settings_message.muted != client_state_muted) {
      setMute(server_settings_message.muted);
      client_state_muted = server_settings_message.muted;
    }
    setVolume(0.01f * server_settings_message.volume);
"""
    if old_settings in patched:
        patched = patched.replace(old_settings, new_settings, 1)
    elif new_settings not in patched:
        raise RuntimeError("Snapclient server settings volume block not found")

    if patched == text:
        return

    processor_path.write_text(patched, encoding="utf-8")
    print(
        f"[snapclient-version] patched Snapserver client version={version} "
        "and volume-only mute handling"
    )


patch_snapclient_version(env=env)
env.AddPreAction("$BUILD_DIR/src/snapclient_mode.cpp.o", patch_snapclient_version)
