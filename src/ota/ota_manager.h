// Fetches the device-facing OTA manifest, compares versions, and performs a
// sha256-verified download+flash to the inactive OTA partition (plan §9). Automatic
// background checks only ever check-and-notify; installing is always an explicit call
// from the web UI or the on-device settings screen, gated by ota::ShouldAttemptAutomaticInstall()
// only when it's the *automatic* path (auto_triggered=true).
#pragma once

namespace ota {

struct UpdateCheckResult {
    bool success = false;
    bool update_available = false;
    char current_version[16] = "";
    char latest_version[16] = "";
    char message[96] = "";
};

// Fetches AppConfig::ota_manifest_url and compares `latest_version` against FW_VERSION.
// If auto_triggered is true and ota::ShouldAttemptAutomaticInstall() is false, returns
// immediately without even fetching the manifest (no point checking if we wouldn't
// install anyway) — callers that want a check regardless (e.g. the web UI's
// "check now" button) must call with auto_triggered=false.
UpdateCheckResult CheckForUpdate(bool auto_triggered);

struct InstallResult {
    bool started = false;
    char reason[64] = "";
};

// Downloads and flashes the firmware from the manifest most recently fetched by
// CheckForUpdate(). `force=false` still refuses if a chargepoint is actively charging
// (surfacing that in `reason` so the caller can ask the user to confirm/force);
// `force=true` skips that check entirely.
InstallResult StartInstall(bool force);

// 0-100 while an install is in progress; -1 if none is running.
int InstallProgressPercent();

// Confirms this boot is healthy and cancels the bootloader's pending rollback (plan §8).
// Idempotent — safe to call every tick until it succeeds. Call once a successful first
// openWB MQTT connection confirms the boot is actually healthy, not just "didn't crash
// before reaching this line" — a successful first render is implied by reaching the net
// task at all, since ui::Init() runs first in main.cpp and would have crashed before this
// otherwise. If this is never called and the device reboots unexpectedly while still
// "pending verify", CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE (sdkconfig.defaults) reverts to
// the previous OTA slot automatically.
void MarkAppValidAfterSuccessfulBoot();

} // namespace ota
