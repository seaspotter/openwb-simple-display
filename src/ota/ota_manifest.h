// Mirrors the device-facing OTA manifest JSON, distinct from the ESP Web Tools
// install-time manifest (docs/manifest.json).
//
// {
//   "latest_version": "1.2.0",
//   "min_supported_version": "1.0.0",
//   "release_notes_url": "https://.../changelog/#v1-2-0",
//   "firmware": {
//     "url": "https://.../firmware/openwb-simple-display-1.2.0-guition_4848s040c.bin",
//     "sha256": "<hex digest>",
//     "signature": "<hex Ed25519 signature, 64 bytes, see include/ota_signing_pubkey.h>",
//     "size_bytes": 1234567
//   },
//   "filesystem": {
//     "url": "https://.../firmware/littlefs.bin",
//     "sha256": "<hex digest>",
//     "signature": "<hex Ed25519 signature>",
//     "size_bytes": 1234567
//   }
// }
// "filesystem" is optional — an OTA install proceeds without it (app-only), logging a
// warning, rather than refusing the whole update over a missing/stale web UI. Without
// this field (and the matching flash step in ota_manager.cpp), an OTA update would
// silently never update the web UI's static assets at all: they live on a separate
// LittleFS partition that only the ESP Web Tools / USB install path (docs/manifest.json)
// ever wrote to — confirmed as the actual cause of a real device staying on old web UI
// JS/HTML indefinitely across several working OTA updates.
#pragma once

#include <cstdint>
#include <cstdio>

#include <ArduinoJson.h>

namespace ota {

struct Manifest {
    char latest_version[16] = "";
    char min_supported_version[16] = ""; // StartInstall() refuses if FW_VERSION < this
    char release_notes_url[160] = "";
    char firmware_url[160] = "";
    char firmware_sha256[65] = "";    // hex-encoded, 64 chars + NUL
    char firmware_signature[129] = ""; // hex-encoded Ed25519 signature, 128 chars + NUL
    uint32_t firmware_size_bytes = 0;

    // All empty/zero if the manifest has no "filesystem" field — ota_manager.cpp treats
    // that as "skip the filesystem update", not an error.
    char filesystem_url[160] = "";
    char filesystem_sha256[65] = "";
    char filesystem_signature[129] = "";
    uint32_t filesystem_size_bytes = 0;
};

namespace detail {
inline void CopyField(char* dest, size_t dest_size, const char* src) {
    snprintf(dest, dest_size, "%s", src); // portable to native_test, unlike strlcpy
}
} // namespace detail

// Returns false if the manifest is missing latest_version or firmware.url.
inline bool ParseManifestJson(JsonVariantConst doc, Manifest& out) {
    detail::CopyField(out.latest_version, sizeof(out.latest_version), doc["latest_version"] | "");
    detail::CopyField(out.min_supported_version, sizeof(out.min_supported_version),
                       doc["min_supported_version"] | "");
    detail::CopyField(out.release_notes_url, sizeof(out.release_notes_url),
                       doc["release_notes_url"] | "");
    detail::CopyField(out.firmware_url, sizeof(out.firmware_url), doc["firmware"]["url"] | "");
    detail::CopyField(out.firmware_sha256, sizeof(out.firmware_sha256),
                       doc["firmware"]["sha256"] | "");
    detail::CopyField(out.firmware_signature, sizeof(out.firmware_signature),
                       doc["firmware"]["signature"] | "");
    out.firmware_size_bytes = doc["firmware"]["size_bytes"] | 0;
    detail::CopyField(out.filesystem_url, sizeof(out.filesystem_url), doc["filesystem"]["url"] | "");
    detail::CopyField(out.filesystem_sha256, sizeof(out.filesystem_sha256),
                       doc["filesystem"]["sha256"] | "");
    detail::CopyField(out.filesystem_signature, sizeof(out.filesystem_signature),
                       doc["filesystem"]["signature"] | "");
    out.filesystem_size_bytes = doc["filesystem"]["size_bytes"] | 0;
    return out.latest_version[0] != '\0' && out.firmware_url[0] != '\0';
}

} // namespace ota
