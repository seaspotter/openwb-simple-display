#include "ota_manager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include "app_config_build.h"
#include "config/config_store.h"
#include "ota_gate.h"
#include "ota_manifest.h"
#include "ota_signing_pubkey.h"
#include "third_party/ed25519/ed25519.h"
#include "util/logging.h"

// Mozilla CA root bundle, linked in via CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y.
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm(
    "_binary_x509_crt_bundle_start");

namespace ota {
namespace {
constexpr char kTag[] = "ota_manager";

Manifest g_last_manifest;
bool g_have_manifest = false;
int g_install_progress = -1;
bool g_update_available = false;

void ApplyCaCertBundle(WiFiClientSecure& client) {
    client.setCACertBundle(x509_crt_imported_bundle_bin_start);
}

// Lexicographic/numeric compare of "x.y.z" version strings. Good enough for this
// project's own tagged releases; not a general semver range comparator.
int CompareVersions(const char* a, const char* b) {
    int a_parts[3] = {0, 0, 0};
    int b_parts[3] = {0, 0, 0};
    sscanf(a, "%d.%d.%d", &a_parts[0], &a_parts[1], &a_parts[2]);
    sscanf(b, "%d.%d.%d", &b_parts[0], &b_parts[1], &b_parts[2]);
    for (int i = 0; i < 3; ++i) {
        if (a_parts[i] != b_parts[i]) return a_parts[i] < b_parts[i] ? -1 : 1;
    }
    return 0;
}

bool FetchManifest(const char* url, Manifest& out) {
    WiFiClientSecure client;
    ApplyCaCertBundle(client);

    HTTPClient http;
    if (!http.begin(client, url)) return false;

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        util::LogWarn(kTag, "manifest fetch failed");
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        util::LogWarn(kTag, "manifest JSON parse error");
        return false;
    }

    return ParseManifestJson(doc, out);
}

bool HexToBytes(const char* hex, uint8_t* out, size_t out_len) {
    if (strlen(hex) != out_len * 2) return false;
    for (size_t i = 0; i < out_len; ++i) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return false;
        out[i] = static_cast<uint8_t>(byte);
    }
    return true;
}

} // namespace

UpdateCheckResult CheckForUpdate(bool auto_triggered) {
    UpdateCheckResult result;
    strlcpy(result.current_version, FW_VERSION, sizeof(result.current_version));

    if (auto_triggered && !ShouldAttemptAutomaticInstall()) {
        strlcpy(result.message, "übersprungen: Ladevorgang aktiv oder kürzlich berührt", sizeof(result.message));
        return result;
    }

    const config::AppConfig& cfg = config::Store().Get();
    Manifest manifest;
    if (!FetchManifest(cfg.ota_manifest_url, manifest)) {
        strlcpy(result.message, "Manifest konnte nicht abgerufen werden", sizeof(result.message));
        return result;
    }

    g_last_manifest = manifest;
    g_have_manifest = true;

    result.success = true;
    strlcpy(result.latest_version, manifest.latest_version, sizeof(result.latest_version));
    result.update_available = CompareVersions(manifest.latest_version, FW_VERSION) > 0;
    g_update_available = result.update_available;
    return result;
}

InstallResult StartInstall(bool force) {
    InstallResult result;

    if (!g_have_manifest) {
        strlcpy(result.reason, "noch kein Manifest abgerufen; zuerst auf Update prüfen", sizeof(result.reason));
        return result;
    }
    if (CompareVersions(FW_VERSION, g_last_manifest.min_supported_version) < 0) {
        strlcpy(result.reason, "Firmware zu alt für dieses Update; bitte manuell per USB neu flashen",
                sizeof(result.reason));
        return result;
    }
    if (!force && !ShouldAttemptAutomaticInstall()) {
        strlcpy(result.reason, "charging_active", sizeof(result.reason));
        return result;
    }

    WiFiClientSecure client;
    ApplyCaCertBundle(client);

    HTTPClient http;
    if (!http.begin(client, g_last_manifest.firmware_url)) {
        strlcpy(result.reason, "Firmware-Download konnte nicht gestartet werden", sizeof(result.reason));
        return result;
    }

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        strlcpy(result.reason, "HTTP-Fehler beim Firmware-Download", sizeof(result.reason));
        http.end();
        return result;
    }

    int content_length = http.getSize();
    if (!Update.begin(content_length > 0 ? content_length : UPDATE_SIZE_UNKNOWN)) {
        strlcpy(result.reason, "nicht genug Platz auf der inaktiven OTA-Partition", sizeof(result.reason));
        http.end();
        return result;
    }

    uint8_t expected_sha256[32];
    bool have_expected_hash = HexToBytes(g_last_manifest.firmware_sha256, expected_sha256, 32);
    if (!have_expected_hash) {
        util::LogWarn(kTag, "manifest sha256 missing/malformed — signature check below still applies");
    }

    uint8_t expected_signature[64];
    bool have_expected_signature =
        HexToBytes(g_last_manifest.firmware_signature, expected_signature, 64);
    if (!have_expected_signature) {
        // Refused outright, not just logged — otherwise signing could be bypassed by
        // simply omitting the field.
        util::LogError(kTag, "manifest has no valid firmware signature, refusing to install");
        http.end();
        strlcpy(result.reason, "Manifest enthält keine gültige Signatur", sizeof(result.reason));
        return result;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, /*is224=*/0);

    auto* stream = http.getStreamPtr(); // type name differs across arduino-esp32 versions
    uint8_t buf[4096];
    int written_total = 0;
    g_install_progress = 0;

    while (http.connected() && (content_length < 0 || written_total < content_length)) {
        size_t available = stream->available();
        if (available == 0) {
            delay(1);
            continue;
        }
        size_t chunk = std::min(available, sizeof(buf));
        size_t read_bytes = stream->readBytes(buf, chunk);
        if (read_bytes == 0) break;

        Update.write(buf, read_bytes);
        mbedtls_sha256_update(&sha_ctx, buf, read_bytes);
        written_total += read_bytes;

        if (content_length > 0) {
            g_install_progress = (written_total * 100) / content_length;
        }
    }
    http.end();

    uint8_t actual_sha256[32];
    mbedtls_sha256_finish(&sha_ctx, actual_sha256);
    mbedtls_sha256_free(&sha_ctx);

    // Checked separately from the signature below for a clearer error on plain corruption.
    bool hash_ok = !have_expected_hash || (memcmp(actual_sha256, expected_sha256, 32) == 0);
    if (!hash_ok) {
        util::LogError(kTag, "downloaded firmware sha256 mismatch, aborting install");
        Update.abort();
        strlcpy(result.reason, "sha256 stimmt nicht überein", sizeof(result.reason));
        g_install_progress = -1;
        return result;
    }

    // Verified against the actual downloaded digest, not the manifest's own claimed
    // sha256 — catches a tampered binary even if that field was forged to match it.
    bool signature_ok =
        ed25519_verify(expected_signature, actual_sha256, sizeof(actual_sha256), kOtaSigningPublicKey) != 0;
    if (!signature_ok) {
        util::LogError(kTag, "firmware signature verification failed, aborting install");
        Update.abort();
        strlcpy(result.reason, "Signaturprüfung fehlgeschlagen", sizeof(result.reason));
        g_install_progress = -1;
        return result;
    }

    if (!Update.end(true)) {
        strlcpy(result.reason, "Flash-Schreibvorgang wurde nicht korrekt abgeschlossen", sizeof(result.reason));
        g_install_progress = -1;
        return result;
    }

    g_install_progress = 100;
    result.started = true;
    util::LogInfo(kTag, "install verified and flashed, restarting");
    delay(500);
    ESP.restart();
    return result; // unreachable
}

int InstallProgressPercent() { return g_install_progress; }

bool IsUpdateAvailable() { return g_update_available; }

void MarkAppValidAfterSuccessfulBoot() {
    static bool already_marked = false;
    if (already_marked) return;

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        already_marked = true;
        util::LogInfo(kTag, "app marked valid, rollback cancelled");
    } else {
        util::LogWarn(kTag, "failed to mark app valid");
    }
}

} // namespace ota
