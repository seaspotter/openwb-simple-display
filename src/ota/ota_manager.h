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
    // true once the download+flash has been handed off to its own task — NOT that the
    // install finished. The caller must poll InstallProgressPercent()/
    // InstallFailureReason() for the outcome, since the real work runs asynchronously
    // (a full firmware download+flash blocking the calling task — the UI task, or the
    // web server's request-handling task — for tens of seconds was the actual cause of
    // a report where an on-device "install" appeared to do nothing: the task either
    // froze long enough to be mistaken for a hang, or a reset during the still-pending
    // download left the OTA slot never committed, so the device just kept booting the
    // old firmware).
    bool started = false;
    char reason[64] = ""; // only set when started == false (a precondition failed)
};

// Validates preconditions synchronously (fast, no network) and, if they pass, hands the
// actual download+verify+flash+restart off to a dedicated task. force=false refuses
// while a chargepoint is charging.
InstallResult StartInstall(bool force);

// 0-100 while the async install is running; -1 if none is running.
int InstallProgressPercent();

// Non-empty if the most recent async install attempt failed (cleared at the start of
// the next StartInstall() call). Empty while one is still running or none has failed.
const char* InstallFailureReason();

// Cancels the bootloader's pending rollback once the boot is confirmed healthy.
// Idempotent. If never called, an unexpected reboot reverts to the previous OTA slot.
void MarkAppValidAfterSuccessfulBoot();

} // namespace ota
