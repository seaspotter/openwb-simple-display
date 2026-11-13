#!/usr/bin/env python3
"""Packages a PlatformIO build into docs/firmware/ and writes the ESP Web Tools install
manifest (docs/manifest.json) and the device-facing OTA manifest
(docs/firmware/ota/manifest.json), signing the firmware's sha256 digest with the
project's Ed25519 release key via `openssl` (see include/ota_signing_pubkey.h).

Usage:
    python3 merge_firmware.py --build-dir .pio/build/guition_4848s040c \\
        --project-dir . --version 1.2.0 --base-url https://<user>.github.io/openwb-display \\
        --private-key-file /path/to/ota_signing_private.pem
"""

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
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


def sign_digest_hex(digest_hex: str, private_key_file: Path) -> str:
    """Ed25519-signs the raw sha256 digest bytes via `openssl`, returns hex."""
    digest_bytes = bytes.fromhex(digest_hex)
    with tempfile.TemporaryDirectory() as tmp:
        digest_path = Path(tmp) / "digest.bin"
        sig_path = Path(tmp) / "sig.bin"
        digest_path.write_bytes(digest_bytes)
        subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-inkey", str(private_key_file),
             "-rawin", "-in", str(digest_path), "-out", str(sig_path)],
            check=True, capture_output=True,
        )
        return sig_path.read_bytes().hex()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--project-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True,
                         help="e.g. https://<user>.github.io/openwb-display")
    parser.add_argument("--board-slug", default="guition_4848s040c")
    parser.add_argument("--private-key-file", type=Path, default=None,
                         help="Ed25519 private key (PEM) to sign the release with. "
                              "Required unless --allow-unsigned is passed.")
    parser.add_argument("--allow-unsigned", action="store_true",
                         help="Skip signing, for local testing only — no device will "
                              "accept the resulting manifest as an OTA update.")
    parser.add_argument("--min-supported-version", default="0.0.0",
                         help="Devices on an older firmware version refuse this OTA "
                              "(see ota_manager.cpp) and must be reflashed manually. "
                              "Default allows any version to update.")
    args = parser.parse_args()

    if args.private_key_file is None and not args.allow_unsigned:
        sys.exit("error: --private-key-file is required (or pass --allow-unsigned)")

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

    # Bootloader/partition-table offsets are fixed by convention; app/littlefs come
    # from partitions.csv.
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

    firmware_url = f"{args.base_url}/firmware/{app_name}"
    app_sha256 = sha256_of(firmware_dir / app_name)
    firmware_field = {
        "url": firmware_url,
        "sha256": app_sha256,
        "size_bytes": (firmware_dir / app_name).stat().st_size,
    }
    if args.private_key_file is not None:
        firmware_field["signature"] = sign_digest_hex(app_sha256, args.private_key_file)
    else:
        print("WARNING: --allow-unsigned set — manifest has no signature; "
              "no device will accept this as an OTA update", file=sys.stderr)

    ota_manifest = {
        "latest_version": args.version,
        "min_supported_version": args.min_supported_version,
        "release_notes_url": f"{args.base_url}/releases/{args.version}.html",
        "firmware": firmware_field,
    }
    (firmware_dir / "ota" / "manifest.json").write_text(json.dumps(ota_manifest, indent=2))

    print(f"wrote {docs_dir / 'manifest.json'} and {firmware_dir / 'ota' / 'manifest.json'}")


if __name__ == "__main__":
    main()
