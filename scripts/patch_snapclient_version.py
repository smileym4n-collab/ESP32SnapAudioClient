from pathlib import Path
import re

Import("env")


def firmware_version():
    version_path = Path(env.subst("$PROJECT_DIR")) / "VERSION"
    version = version_path.read_text(encoding="utf-8").strip()
    return version[1:] if version.startswith("v") else version


def patch_snapclient_version(source=None, target=None, env=None):
    # The upstream Snapclient library hard-codes the hello version Snapserver sees.
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

    if patched == text:
        if f'hello_message.version = "{version}";' in text:
            return
        raise RuntimeError("Snapclient hello_message.version assignment not found")

    processor_path.write_text(patched, encoding="utf-8")
    print(f"[snapclient-version] patched Snapserver client version={version}")


patch_snapclient_version(env=env)
env.AddPreAction("$BUILD_DIR/src/snapclient_mode.cpp.o", patch_snapclient_version)
