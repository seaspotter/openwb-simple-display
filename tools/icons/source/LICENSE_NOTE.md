# Icon source

`tabler-icons.ttf` (and `LICENSE` alongside it) is the Tabler Icons webfont, MIT
licensed, from https://github.com/tabler/tabler-icons — downloaded from
`@tabler/icons-webfont@3.51.0` on jsDelivr:

```
curl -fsSL -o tabler-icons.ttf \
  https://cdn.jsdelivr.net/npm/@tabler/icons-webfont@3.51.0/dist/fonts/tabler-icons.ttf
curl -fsSL -o LICENSE \
  https://raw.githubusercontent.com/tabler/tabler-icons/main/LICENSE
```

`tools/icons/generate_icons.sh` extracts a handful of specific icons (by their
Private-Use-Area codepoint, confirmed against that version's `dist/tabler-icons.css`)
into `src/ui/theme/fonts/font_icons_16.c` — icons as font glyphs, not rasterized images.
