from pathlib import Path
import os
import re
import subprocess

Import("env")


VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")


def tagged_version():
    try:
        project_dir = Path(env.subst("$PROJECT_DIR"))
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--exact-match", "HEAD"],
            cwd=project_dir,
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return None

    if tag.startswith("v"):
        tag = tag[1:]
    return tag if VERSION_RE.fullmatch(tag) else None


version = os.environ.get("OGN_VERSION") or tagged_version()
if version and version.startswith("v"):
    version = version[1:]

if version:
    if not VERSION_RE.fullmatch(version):
        raise RuntimeError(
            "OGN_VERSION must have the form MAJOR.MINOR.PATCH, "
            f"got {version!r}"
        )
    env.Append(CPPDEFINES=[("VERSION", env.StringifyMacro(version))])
    print(f"Firmware version: {version}")
else:
    print("Firmware version: using the untagged build fallback from src/main.h")
