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

#include <Arduino.h>

#include "app_config_build.h"
#include "config/config_store.h"
#include "ota_gate.h"
#include "ota_manifest.h"
#include "ota_signing_pubkey.h"
#include "state/app_state.h"
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
bool g_install_in_progress = false;
char g_install_failure_reason[64] = "";

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

// Runs on its own dedicated task (see StartInstall) — never on the caller's task.
// Downloading ~1.6MB and writing it to flash takes anywhere from several seconds to
// over a minute depending on WiFi throughput; running that inline on the calling task
// (the UI task for an on-device install, the web server's request-handling task for a
// web-triggered one) blocked rendering/networking for the whole operation and, worse,
// left a real window where a reset before Update.end() committed the new boot partition
// would silently leave the device on its old firmware with no error shown — the
// confirmed cause of a report where neither of two prior releases ever actually landed
// despite "Installation läuft" being shown.
void InstallTaskBody(void* /*param*/) {
    auto fail = [](const char* reason) {
        strlcpy(g_install_failure_reason, reason, sizeof(g_install_failure_reason));
        util::LogError(kTag, reason);
        g_install_progress = -1;
        g_install_in_progress = false;
        vTaskDelete(nullptr); // unreachable past this point
    };

    WiFiClientSecure client;
    ApplyCaCertBundle(client);

    HTTPClient http;
    if (!http.begin(client, g_last_manifest.firmware_url)) {
        fail("Firmware-Download konnte nicht gestartet werden");
        return;
    }

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        fail("HTTP-Fehler beim Firmware-Download");
        return;
    }

    int content_length = http.getSize();
    if (!Update.begin(content_length > 0 ? content_length : UPDATE_SIZE_UNKNOWN)) {
        http.end();
        fail("nicht genug Platz auf der inaktiven OTA-Partition");
        return;
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
        http.end();
        fail("Manifest enthält keine gültige Signatur");
        return;
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
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        size_t chunk = std::min(available, sizeof(buf));
        size_t read_bytes = stream->readBytes(buf, chunk);
        if (read_bytes == 0) break;

        size_t written = Update.write(buf, read_bytes);
        if (written != read_bytes) {
            Update.abort();
            http.end();
            fail("Flash-Schreibfehler beim Download");
            return;
        }
        mbedtls_sha256_update(&sha_ctx, buf, read_bytes);
        written_total += read_bytes;

        if (content_length > 0) {
            g_install_progress = (written_total * 100) / content_length;
        }
        // Running on a dedicated task now (not the UI/web-request task), so this isn't
        // needed to keep the rest of the system responsive — it's here so this task
        // itself always yields at least once per chunk instead of only when the
        // network stalls, which is what made a tight run of back-to-back available
        // chunks able to starve this task's own watchdog feeding.
        taskYIELD();
    }
    http.end();

    uint8_t actual_sha256[32];
    mbedtls_sha256_finish(&sha_ctx, actual_sha256);
    mbedtls_sha256_free(&sha_ctx);

    // Checked separately from the signature below for a clearer error on plain corruption.
    bool hash_ok = !have_expected_hash || (memcmp(actual_sha256, expected_sha256, 32) == 0);
    if (!hash_ok) {
        Update.abort();
        fail("sha256 stimmt nicht überein");
        return;
    }

    // Verified against the actual downloaded digest, not the manifest's own claimed
    // sha256 — catches a tampered binary even if that field was forged to match it.
    bool signature_ok =
        ed25519_verify(expected_signature, actual_sha256, sizeof(actual_sha256), kOtaSigningPublicKey) != 0;
    if (!signature_ok) {
        Update.abort();
        fail("Signaturprüfung fehlgeschlagen");
        return;
    }

    if (!Update.end(true)) {
        fail("Flash-Schreibvorgang wurde nicht korrekt abgeschlossen");
        return;
    }

    g_install_progress = 100;
    util::LogInfo(kTag, "install verified and flashed, restarting");
    delay(500);
    ESP.restart();
    // unreachable
}

InstallResult StartInstall(bool force) {
    InstallResult result;

    // StartInstall can be reached from the UI task, the web-config server, and an
    // automatic net_task check; without this guard two concurrent calls could both
    // reach Update.begin()/write()/end() against the same OTA partition.
    if (g_install_in_progress) {
        strlcpy(result.reason, "Installation läuft bereits", sizeof(result.reason));
        return result;
    }

    if (!g_have_manifest) {
        strlcpy(result.reason, "noch kein Manifest abgerufen; zuerst auf Update prüfen", sizeof(result.reason));
        return result;
    }
    if (CompareVersions(FW_VERSION, g_last_manifest.min_supported_version) < 0) {
        strlcpy(result.reason, "Firmware zu alt für dieses Update; bitte manuell per USB neu flashen",
                sizeof(result.reason));
        return result;
    }
    // Intentionally NOT ShouldAttemptAutomaticInstall() here — that also refuses within
    // ~5 minutes of any touch, which is exactly what just happened to reach this button
    // (tapping "Installieren" is itself a touch). That idle check only makes sense for
    // the automatic background check in net_task.cpp, not for an explicit manual
    // install — using it here meant a manual install always reported "charging_active"
    // on the first attempt even with nothing charging, forcing the force-confirm path
    // every single time.
    if (!force && state::State().AnyChargepointCharging()) {
        strlcpy(result.reason, "charging_active", sizeof(result.reason));
        return result;
    }

    g_install_failure_reason[0] = '\0';
    g_install_progress = 0;
    g_install_in_progress = true;

    BaseType_t created = xTaskCreatePinnedToCore(InstallTaskBody, "ota_install", 12288, nullptr,
                                                  1, nullptr, 0);
    if (created != pdPASS) {
        g_install_in_progress = false;
        g_install_progress = -1;
        strlcpy(result.reason, "Installations-Task konnte nicht gestartet werden", sizeof(result.reason));
        return result;
    }

    result.started = true;
    return result;
}

int InstallProgressPercent() { return g_install_progress; }

const char* InstallFailureReason() { return g_install_failure_reason; }

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
