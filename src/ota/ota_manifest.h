// Mirrors the device-facing OTA manifest JSON (plan §9), hosted at
// AppConfig::ota_manifest_url (default: the project's GitHub Pages site). Distinct from
// the ESP Web Tools install-time manifest (docs/manifest.json), which is a different
// shape fixed by that library's spec.
//
// {
//   "latest_version": "1.2.0",
//   "min_supported_version": "1.0.0",
//   "release_notes_url": "https://.../releases/1.2.0.html",
//   "firmware": {
//     "url": "https://.../firmware/openwb-simple-display-1.2.0-guition_4848s040c.bin",
//     "sha256": "<hex digest>",
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
    char min_supported_version[16] = "";
    char release_notes_url[160] = "";
    char firmware_url[160] = "";
    char firmware_sha256[65] = ""; // hex-encoded, 64 chars + NUL
    uint32_t firmware_size_bytes = 0;
};

namespace detail {
inline void CopyField(char* dest, size_t dest_size, const char* src) {
    // snprintf rather than strlcpy: portable to the native_test host build, which
    // doesn't reliably provide the BSD strlcpy that the ESP32 Arduino core does.
    snprintf(dest, dest_size, "%s", src);
}
} // namespace detail

// Header-only (like openwb_mqtt_topics.h) so test/test_ota_manifest can link against it
// under the native_test env without pulling in any ESP-only dependency.
// Returns false if the document is missing the fields that make a manifest usable
// (latest_version, firmware.url).
inline bool ParseManifestJson(JsonVariantConst doc, Manifest& out) {
    detail::CopyField(out.latest_version, sizeof(out.latest_version), doc["latest_version"] | "");
    detail::CopyField(out.min_supported_version, sizeof(out.min_supported_version),
                       doc["min_supported_version"] | "");
    detail::CopyField(out.release_notes_url, sizeof(out.release_notes_url),
                       doc["release_notes_url"] | "");
    detail::CopyField(out.firmware_url, sizeof(out.firmware_url), doc["firmware"]["url"] | "");
    detail::CopyField(out.firmware_sha256, sizeof(out.firmware_sha256),
                       doc["firmware"]["sha256"] | "");
    out.firmware_size_bytes = doc["firmware"]["size_bytes"] | 0;
    return out.latest_version[0] != '\0' && out.firmware_url[0] != '\0';
}

} // namespace ota
