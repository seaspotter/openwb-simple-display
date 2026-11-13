#!/usr/bin/env bash
# Generates LVGL font C files from the OFL sans-serif font in tools/fonts/source/ at the
# sizes this project's screens actually use (plan §6). Requires Node and lv_font_conv
# (https://github.com/lvgl/lv_font_conv): `npm install -g lv_font_conv`.
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

# Sizes used by the UI (plan §6): 11px small legends/labels (matches quartz's own
# `.badge` CSS: font-size 11px, confirmed against the live theme), 14px labels,
# 20px values, 32px hero numbers.
# Range covers Basic Latin (0x20-0x7F) plus Latin-1 Supplement (0xA0-0xFF), the latter
# needed for German UI text (ä/ö/ü/Ä/Ö/Ü/ß) — the UI is German, not just ASCII.
# --no-compress: lv_font_conv's default RLE-compressed bitmap format
# (bitmap_format=1) rendered as all-tofu against the pinned lvgl@9.2.2 — confirmed by
# comparing against LVGL's own built-in fonts, which all ship uncompressed
# (bitmap_format=0) and render correctly. Possible version drift between
# lv_font_conv@1.5.3's RLE scheme and this LVGL release's decoder; plain bitmaps sidestep
# it entirely and cost only a modest flash-size increase for this project's small glyph set.
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

# Bold weight, 14px only so far — used where a label needs to stand out (e.g. the
# chargepoint name) without bumping its size. Add more sizes here if a future screen
# needs them; LVGL has no synthetic/faux-bold, so an actual bold-weight font file is the
# only way to get this, not a style property.
lv_font_conv \
  --font "${SOURCE_FONT_BOLD}" \
  --size 14 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --range 0x20-0x7F,0xA0-0xFF \
  -o "${OUT_DIR}/font_sans_14_bold.c"
echo "generated font_sans_14_bold.c"
