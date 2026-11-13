"""PlatformIO pre-build step: deletes LVGL's ARM-only NEON/Helium blend assembly files
from the installed dependency, since the LDF compiles them unconditionally and they
fail to assemble on this ESP32-S3 (Xtensa) target. Only touches the gitignored
.pio/libdeps cache; a no-op once the files are gone."""

from pathlib import Path

Import("env")  # noqa: F821 - provided by the PlatformIO build environment

PROJECT_DIR = Path(env["PROJECT_DIR"])  # noqa: F821

ARM_ONLY_ASM_FILES = [
    "src/draw/sw/blend/neon/lv_blend_neon.S",
    "src/draw/sw/blend/helium/lv_blend_helium.S",
]


def strip():
    libdeps_dir = PROJECT_DIR / ".pio" / "libdeps"
    if not libdeps_dir.is_dir():
        return

    for env_dir in libdeps_dir.iterdir():
        for lvgl_dir in env_dir.glob("lvgl*"):
            for rel_path in ARM_ONLY_ASM_FILES:
                path = lvgl_dir / rel_path
                if path.exists():
                    path.unlink()
                    print(f"[strip_lvgl_arm_asm] removed {path}")


strip()
