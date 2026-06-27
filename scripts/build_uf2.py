from pathlib import Path
import subprocess

Import("env")


def build_uf2(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    bin_path = build_dir / "firmware.bin"
    uf2_path = build_dir / "firmware.uf2"

    if not bin_path.exists():
        print(f"UF2: missing {bin_path}")
        return

    packages_dir = Path(env.subst("$PROJECT_PACKAGES_DIR"))
    tools = list(packages_dir.rglob("uf2conv.py"))
    if not tools:
        print("UF2: uf2conv.py was not found in the PlatformIO packages")
        return

    cmd = [
        env.subst("$PYTHONEXE"),
        str(tools[0]),
        str(bin_path),
        "-c",
        "-b",
        "0x26000",
        "-f",
        "0xADA52840",
        "-o",
        str(uf2_path),
    ]
    subprocess.check_call(cmd)
    print(f"UF2: wrote {uf2_path}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_uf2)
