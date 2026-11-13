"""PlatformIO pre-build step: removes LVGL's ARM-only NEON/Helium assembly blend
backends from the installed dependency copy.

The `lvgl/lvgl` PlatformIO registry package declares no `srcFilter` in its
library.json, so PlatformIO's LDF compiles its entire src/ tree unconditionally,
including these two assembly files that are irrelevant on this ESP32-S3 (Xtensa)
target. Assembling them fails here specifically because the assembler's preprocessing
include path doesn't carry the ESP-IDF sdkconfig.h/esp_attr.h paths that normal C/C++
translation units get, which lv_conf_kconfig.h's `#ifdef ESP_PLATFORM` branch requires.

Touches only the disposable `.pio/libdeps` cache (gitignored, rebuilt per machine), not
the project's own source — safe to run on every build; a no-op once the files are gone.
"""

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
