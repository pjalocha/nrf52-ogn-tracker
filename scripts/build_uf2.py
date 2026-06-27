from pathlib import Path
import subprocess

Import("env")


def build_uf2(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    bin_path = build_dir / "firmware.bin"
    hex_path = build_dir / "firmware.hex"
    uf2_path = build_dir / "firmware.uf2"
    base_address = env.GetProjectOption("custom_uf2_base")

    if hex_path.exists():
        input_path = hex_path
    elif bin_path.exists():
        input_path = bin_path
    else:
        print(f"UF2: missing {hex_path} and {bin_path}")
        return

    packages_dir = Path(env.subst("$PROJECT_PACKAGES_DIR"))
    tools = list(packages_dir.rglob("uf2conv.py"))
    if not tools:
        print("UF2: uf2conv.py was not found in the PlatformIO packages")
        return

    cmd = [
        env.subst("$PYTHONEXE"),
        str(tools[0]),
        str(input_path),
        "-c",
        "-f",
        "0xADA52840",
        "-o",
        str(uf2_path),
    ]
    if input_path.suffix == ".bin":
        cmd[4:4] = ["-b", base_address]

    subprocess.check_call(cmd)
    print(f"UF2: wrote {uf2_path} from {input_path.name}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_uf2)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", build_uf2)
