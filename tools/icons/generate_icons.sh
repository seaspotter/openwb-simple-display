#!/usr/bin/env bash
# Generates a small LVGL icon font from the Tabler Icons (MIT) webfont — icons as font
# glyphs, exactly the same lv_font_conv tool and workflow as
# tools/fonts/generate_fonts.sh, not a separate SVG-rasterization pipeline (an earlier,
# abandoned plan for this file). Requires Node and lv_font_conv:
# `npm install -g lv_font_conv`.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_FONT="${SCRIPT_DIR}/source/tabler-icons.ttf"
OUT_DIR="${SCRIPT_DIR}/../../src/ui/theme/fonts"

if [ ! -f "${SOURCE_FONT}" ]; then
  echo "Missing ${SOURCE_FONT} — download it (and its LICENSE) into" >&2
  echo "tools/icons/source/ before running this script. Not fetched automatically:" >&2
  echo "this script doesn't pull external files on its own." >&2
  echo "  curl -fsSL -o ${SOURCE_FONT} \\" >&2
  echo "    https://cdn.jsdelivr.net/npm/@tabler/icons-webfont@3.51.0/dist/fonts/tabler-icons.ttf" >&2
  exit 1
fi

if ! command -v lv_font_conv >/dev/null 2>&1; then
  echo "lv_font_conv not found. Install with: npm install -g lv_font_conv" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

# Codepoints confirmed against @tabler/icons-webfont@3.51.0's dist/tabler-icons.css
# (the canonical codepoint -> icon-name map that ships with the font) — pin that
# version if regenerating, since codepoints can shift between releases.
#   0xEB30 sun            -> PV (Erzeugung)
#   0xF00A plug-connected -> Netz (grid)
#   0xEA38 bolt           -> Ladepunkte (chargepoints)
#   0xEA34 battery        -> Speicher (battery)
#   0xF336 home-bolt      -> Verbraucher (other consumers)
#   0xEAE1 lock-open      -> chargepoint lock button (unlocked state)
#   0xEAE2 lock           -> chargepoint lock button (locked state)
#   0xEB20 settings       -> future settings-gear use
#   0xEBBB car            -> connected vehicle row
lv_font_conv \
  --font "${SOURCE_FONT}" \
  --size 16 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --range 0xEA34,0xEA38,0xEAE1,0xEAE2,0xEB20,0xEB30,0xEBBB,0xF00A,0xF336 \
  --lv-font-name font_icons_16 \
  -o "${OUT_DIR}/font_icons_16.c"
echo "generated font_icons_16.c"

# 24px variant of just the lock/lock-open glyphs — used for the chargepoint lock toggle
# specifically, which is a real touch control (not a decorative/informational icon like
# the others), so it needs a bigger glyph + tap target than the 16px set above.
lv_font_conv \
  --font "${SOURCE_FONT}" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --range 0xEAE1,0xEAE2 \
  --lv-font-name font_icons_24 \
  -o "${OUT_DIR}/font_icons_24.c"
echo "generated font_icons_24.c"
