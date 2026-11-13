// Mirrors the device-facing OTA manifest JSON, distinct from the ESP Web Tools
// install-time manifest (docs/manifest.json).
//
// {
//   "latest_version": "1.2.0",
//   "min_supported_version": "1.0.0",
//   "release_notes_url": "https://.../releases/1.2.0.html",
//   "firmware": {
//     "url": "https://.../firmware/openwb-simple-display-1.2.0-guition_4848s040c.bin",
//     "sha256": "<hex digest>",
//     "signature": "<hex Ed25519 signature, 64 bytes, see include/ota_signing_pubkey.h>",
//     "size_bytes": 1234567
//   }
// }
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
    return out.latest_version[0] != '\0' && out.firmware_url[0] != '\0';
}

} // namespace ota
