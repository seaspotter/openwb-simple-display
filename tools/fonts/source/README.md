# Font source

Place `PublicSans-Regular.ttf` (OFL license, from
https://github.com/googlefonts/public-sans) here, plus that project's `OFL.txt` as
`LICENSE` in this directory for attribution. Not fetched automatically by
`generate_fonts.sh` — add it manually before running that script.

`PublicSans-Bold.woff` is the same OFL-licensed Public Sans family, repackaged by
`@fontsource/public-sans` (the upstream googlefonts/public-sans repo doesn't serve a
bare `.ttf` for the bold weight at a stable URL; this `.woff` — not `.woff2`, which
lv_font_conv's fontkit dependency can't decode — is:
```
curl -fsSL -o PublicSans-Bold.woff \
  https://cdn.jsdelivr.net/npm/@fontsource/public-sans@5.3.0/files/public-sans-latin-700-normal.woff
```
