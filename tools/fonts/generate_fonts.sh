#!/usr/bin/env bash
# Generates LVGL font C files from tools/fonts/source/. Requires lv_font_conv:
# `npm install -g lv_font_conv`.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_FONT="${SCRIPT_DIR}/source/PublicSans-Regular.ttf"
SOURCE_FONT_BOLD="${SCRIPT_DIR}/source/PublicSans-Bold.woff"
OUT_DIR="${SCRIPT_DIR}/../../src/ui/theme/fonts"

if [ ! -f "${SOURCE_FONT}" ] || [ ! -f "${SOURCE_FONT_BOLD}" ]; then
  echo "Missing PublicSans-Regular.ttf and/or PublicSans-Bold.woff — see" >&2
  echo "tools/fonts/source/README.md for exactly where to get each one. Not fetched" >&2
  echo "automatically: this script doesn't pull external files on its own." >&2
  exit 1
fi

if ! command -v lv_font_conv >/dev/null 2>&1; then
  echo "lv_font_conv not found. Install with: npm install -g lv_font_conv" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

# Range: Basic Latin + Latin-1 Supplement, for German umlauts/ß.
# --no-compress: lv_font_conv's default RLE bitmap format renders as tofu on lvgl@9.2.2.
for size in 11 14 20 32; do
  lv_font_conv \
    --font "${SOURCE_FONT}" \
    --size "${size}" \
    --bpp 4 \
    --format lvgl \
    --no-compress \
    --range 0x20-0x7F,0xA0-0xFF \
    -o "${OUT_DIR}/font_sans_${size}.c"
  echo "generated font_sans_${size}.c"
done

# LVGL has no synthetic bold — a real bold-weight font file, size 14 only so far.
lv_font_conv \
  --font "${SOURCE_FONT_BOLD}" \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --range 0x20-0x7F,0xA0-0xFF \
  -o "${OUT_DIR}/font_sans_14_bold.c"
echo "generated font_sans_14_bold.c"
