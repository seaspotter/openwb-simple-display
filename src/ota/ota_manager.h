// Fetches the OTA manifest, compares versions, and performs a signature- and
// sha256-verified download+flash to the inactive OTA partition.
#pragma once

namespace ota {

struct UpdateCheckResult {
    bool success = false;
    bool update_available = false;
    char current_version[16] = "";
    char latest_version[16] = "";
    char message[96] = "";
};

// If auto_triggered is true and ShouldAttemptAutomaticInstall() is false, skips the
// fetch entirely.
UpdateCheckResult CheckForUpdate(bool auto_triggered);

// Result of the most recent CheckForUpdate() call, or false if none has run yet.
bool IsUpdateAvailable();

struct InstallResult {
    bool started = false;
    char reason[64] = "";
};

// Downloads and flashes the firmware from the manifest last fetched by
// CheckForUpdate(). force=false refuses while a chargepoint is charging.
InstallResult StartInstall(bool force);

// 0-100 during an install; -1 if none is running.
int InstallProgressPercent();

// Cancels the bootloader's pending rollback once the boot is confirmed healthy.
// Idempotent. If never called, an unexpected reboot reverts to the previous OTA slot.
void MarkAppValidAfterSuccessfulBoot();

} // namespace ota
