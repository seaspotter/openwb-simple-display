# Vendored source

This is a subset of [orlp/ed25519](https://github.com/orlp/ed25519) (zlib license, see
`LICENSE` alongside this file) — a small, dependency-free, public-domain-style Ed25519
implementation in plain C, chosen because this project's pinned ESP-IDF/mbedtls version
(`espressif32@6.11.0`) doesn't have Ed25519 support compiled in (`PSA_WANT_ALG_PURE_EDDSA`
is not enabled in that prebuilt framework's sdkconfig — confirmed by checking its bundled
headers directly, not guessed), so mbedtls's own PK API isn't usable for this.

Only the files needed for **verification** are vendored (`ed25519_verify()` and its
dependencies: `verify.c`, `ge.c`, `sc.c`, `fe.c`, `sha512.c` + headers) — this project
never signs anything on-device, only verifies a signature produced by
`tools/release/merge_firmware.py` in CI, so the signing/keypair-generation/key-exchange
files (`sign.c`, `keypair.c`, `seed.c`, `add_scalar.c`, `key_exchange.c`) were
deliberately left out.

Fetched from the `master` branch; pin a specific commit if regenerating, since this
upstream has no tagged releases.
