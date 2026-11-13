"""PlatformIO pre-build step: mirrors src/web/www/ into data/ before the LittleFS image
is built, so the web UI's HTML/CSS/JS has exactly one source of truth (src/web/www/)
instead of two copies that can drift (plan §2's "data/ vs src/web/www/" note).

Registered via platformio.ini's `extra_scripts = pre:tools/sync_www_to_data.py`.
"""

import shutil
from pathlib import Path

Import("env")  # noqa: F821 - provided by the PlatformIO build environment

PROJECT_DIR = Path(env["PROJECT_DIR"])  # noqa: F821
SRC_WWW = PROJECT_DIR / "src" / "web" / "www"
DATA_DIR = PROJECT_DIR / "data"


def sync():
    if not SRC_WWW.is_dir():
        print(f"[sync_www_to_data] {SRC_WWW} does not exist, skipping")
        return

    if DATA_DIR.exists():
        shutil.rmtree(DATA_DIR)
    shutil.copytree(SRC_WWW, DATA_DIR)
    print(f"[sync_www_to_data] copied {SRC_WWW} -> {DATA_DIR}")


sync()
