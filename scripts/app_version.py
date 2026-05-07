from pathlib import Path
import re

Import("env")


VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")


def read_version():
    version_path = Path(env.subst("$PROJECT_DIR")) / "VERSION"
    version = version_path.read_text(encoding="utf-8").strip()
    if version.startswith("v"):
        version = version[1:]
    if not VERSION_RE.match(version):
        raise RuntimeError(
            f"Invalid firmware version '{version}'. Expected semantic version like 1.0.0."
        )
    return version


def stringify(value):
    if hasattr(env, "StringifyMacro"):
        return env.StringifyMacro(value)
    return f'\\"{value}\\"'


firmware_version = read_version()
env.Append(
    CPPDEFINES=[
        ("APP_FIRMWARE_VERSION", stringify(firmware_version)),
        ("APP_FIRMWARE_VERSION_TAG", stringify(f"v{firmware_version}")),
    ]
)
print(f"[app-version] firmware version={firmware_version}")
