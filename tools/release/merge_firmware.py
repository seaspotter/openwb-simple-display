#!/usr/bin/env python3
"""Packages a PlatformIO build's output artifacts (plan §9): copies the per-part
binaries into docs/firmware/, and regenerates both manifest.json shapes —
the ESP Web Tools install-time manifest (docs/manifest.json) and the device-facing
OTA-check manifest (docs/firmware/ota/manifest.json) — computing part offsets from the
actual partitions.csv rather than hand-maintaining them, so the two never drift apart.

Usage:
    python3 merge_firmware.py --build-dir .pio/build/guition_4848s040c \\
        --project-dir . --version 1.2.0 --base-url https://<user>.github.io/openwb-display

Run from CI (.github/workflows/pages.yml) after `pio run` and `pio run -t buildfs`.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path


def parse_partitions_csv(path: Path):
    """Returns {name: (offset:int, size:int)} from a PlatformIO partitions.csv."""
    partitions = {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = [f.strip() for f in line.split(",")]
        if len(fields) < 5:
            continue
        name, offset, size = fields[0], fields[3], fields[4]
        partitions[name] = (int(offset, 0), int(size, 0))
    return partitions


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--project-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True,
                         help="e.g. https://<user>.github.io/openwb-display")
    parser.add_argument("--board-slug", default="guition_4848s040c")
    args = parser.parse_args()

    project_dir = args.project_dir.resolve()
    build_dir = args.build_dir.resolve()
    docs_dir = project_dir / "docs"
    firmware_dir = docs_dir / "firmware"
    firmware_dir.mkdir(parents=True, exist_ok=True)
    (firmware_dir / "ota").mkdir(parents=True, exist_ok=True)

    partitions = parse_partitions_csv(project_dir / "partitions.csv")

    bootloader_src = build_dir / "bootloader.bin"
    partitions_bin_src = build_dir / "partitions.bin"
    app_src = build_dir / "firmware.bin"
    littlefs_src = build_dir / "littlefs.bin"

    app_name = f"openwb-simple-display-{args.version}-{args.board_slug}.bin"
    for src, dest_name in (
        (bootloader_src, "bootloader.bin"),
        (partitions_bin_src, "partitions.bin"),
        (app_src, app_name),
        (littlefs_src, "littlefs.bin"),
    ):
        if not src.exists():
            raise SystemExit(f"expected build artifact missing: {src}")
        (firmware_dir / dest_name).write_bytes(src.read_bytes())

    # ESP Web Tools install manifest. Bootloader offset is fixed by the ESP32 boot ROM
    # (0x0 for esp32/esp32s3 with this flash mode); partition table offset is also
    # fixed at 0x8000 by convention — only the app and littlefs offsets come from this
    # project's own partitions.csv.
    install_manifest = {
        "name": "openwb-simple-display",
        "version": args.version,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": [
                    {"path": f"firmware/bootloader.bin", "offset": 0x0},
                    {"path": f"firmware/partitions.bin", "offset": 0x8000},
                    {"path": f"firmware/{app_name}", "offset": partitions["app0"][0]},
                    {"path": f"firmware/littlefs.bin", "offset": partitions["littlefs"][0]},
                ],
            }
        ],
    }
    (docs_dir / "manifest.json").write_text(json.dumps(install_manifest, indent=2))

    # Device-facing OTA-check manifest (ota_manifest.h's shape).
    firmware_url = f"{args.base_url}/firmware/{app_name}"
    ota_manifest = {
        "latest_version": args.version,
        "min_supported_version": args.version,  # bump explicitly when a breaking change
                                                 # in AppConfig::schema_version ships
        "release_notes_url": f"{args.base_url}/releases/{args.version}.html",
        "firmware": {
            "url": firmware_url,
            "sha256": sha256_of(firmware_dir / app_name),
            "size_bytes": (firmware_dir / app_name).stat().st_size,
        },
    }
    (firmware_dir / "ota" / "manifest.json").write_text(json.dumps(ota_manifest, indent=2))

    print(f"wrote {docs_dir / 'manifest.json'} and {firmware_dir / 'ota' / 'manifest.json'}")


if __name__ == "__main__":
    main()
