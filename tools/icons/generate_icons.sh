#!/usr/bin/env bash
# Generates LVGL icon fonts from the Tabler Icons (MIT) webfont. Requires lv_font_conv:
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

# Codepoints per @tabler/icons-webfont@3.51.0's dist/tabler-icons.css; pin that version
# if regenerating, since codepoints can shift between releases.
#   0xEB30 sun            -> PV (Erzeugung)
#   0xEA38 bolt           -> Netz (grid) AND Ladepunkte (chargepoints) — same shape,
#                             different colors, matching openWB's own quartz web theme
#   0xEA34 battery        -> Speicher (battery)
#   0xEAC1 home           -> Hausverbrauch (home base load)
#   0xEA97 droplet        -> Verbraucher (other consumers)
#   0xEAB1 gauge          -> Zähler (auto-discovered extra sub-meter(s))
#   0xEAE1 lock-open      -> chargepoint lock button (unlocked state)
#   0xEAE2 lock           -> chargepoint lock button (locked state)
#   0xEB20 settings       -> future settings-gear use
#   0xEBBB car            -> connected vehicle row
#   0xEA06 alert-triangle -> status bar: a chargepoint has a fault
#   0xEA71 cloud-download -> status bar: firmware update available
#   0xEB52 wifi           -> status bar: MQTT connected
#   0xECFA wifi-off       -> status bar: MQTT disconnected
lv_font_conv \
  --font "${SOURCE_FONT}" \
  --size 16 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --range 0xEA06,0xEA34,0xEA38,0xEA71,0xEA97,0xEAB1,0xEAC1,0xEAE1,0xEAE2,0xEB20,0xEB30,0xEB52,0xEBBB,0xECFA \
  --lv-font-name font_icons_16 \
  -o "${OUT_DIR}/font_icons_16.c"
echo "generated font_icons_16.c"

# 24px: lock/lock-open (the lock toggle's tap target) plus the status bar's icons.
lv_font_conv \
  --font "${SOURCE_FONT}" \
  --size 24 \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  --range 0xEA06,0xEA71,0xEAE1,0xEAE2,0xEB20,0xEB52,0xECFA \
  --lv-font-name font_icons_24 \
  -o "${OUT_DIR}/font_icons_24.c"
echo "generated font_icons_24.c"
