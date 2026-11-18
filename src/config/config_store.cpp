#include "config_store.h"

#include <algorithm>
#include <cstring>

#include "config_defaults.h"
#include "util/logging.h"

#ifndef UNIT_TEST
#include <Preferences.h>
#endif

namespace config {
namespace {
constexpr char kTag[] = "config_store";
constexpr char kNamespace[] = "appcfg";
constexpr char kBlobKey[] = "cfg";
} // namespace

void ConfigStore::Load() {
    AppConfig loaded = DefaultConfig();

#ifndef UNIT_TEST
    Preferences prefs;
    if (prefs.begin(kNamespace, /*readOnly=*/true)) {
        size_t stored_size = prefs.getBytesLength(kBlobKey);
        if (stored_size > 0) {
            // AppConfig schema changes are append-only (new fields added at the end —
            // see kCurrentSchemaVersion's comment in app_config.h); reading only the
            // overlapping byte prefix preserves every existing field's value across a
            // firmware update that adds one, leaving just the new trailing field(s) at
            // their struct-default values. Previously any size change at all (i.e. any
            // OTA update that added a single field) silently discarded the entire saved
            // config — WiFi, MQTT, chargepoint IDs, PIN, everything — back to defaults.
            size_t copy_size = std::min(stored_size, sizeof(AppConfig));
            prefs.getBytes(kBlobKey, &loaded, copy_size);
            if (stored_size != sizeof(AppConfig)) {
                util::LogWarn(kTag, "stored config size differs from current schema, migrated by prefix");
            }
        }
        prefs.end();
    }
#endif

    if (loaded.schema_version != kCurrentSchemaVersion) {
        Migrate(loaded, loaded.schema_version);
    }

    // Defend against a corrupted/partially-written NVS blob that still happens to match
    // sizeof(AppConfig): clamp chargepoint_count so every consumer's fixed-size
    // chargepoint_ids[kMaxChargepoints] loop stays in bounds.
    if (loaded.chargepoint_count > AppConfig::kMaxChargepoints) {
        util::LogWarn(kTag, "stored chargepoint_count out of range, clamping");
        loaded.chargepoint_count = AppConfig::kMaxChargepoints;
    }

    cached_ = loaded;
    loaded_ = true;
}

void ConfigStore::Migrate(AppConfig& loaded_config, uint8_t from_version) {
    // No prior schema versions exist yet; add a `case N:` here when one does.
    util::LogWarn(kTag, "unknown config schema version, resetting to defaults");
    (void)from_version;
    loaded_config = DefaultConfig();
}

const AppConfig& ConfigStore::Get() {
    if (!loaded_) Load();
    return cached_;
}

void ConfigStore::Save(const AppConfig& new_config) {
    cached_ = new_config;
    cached_.schema_version = kCurrentSchemaVersion;
    loaded_ = true;

#ifndef UNIT_TEST
    Preferences prefs;
    if (prefs.begin(kNamespace, /*readOnly=*/false)) {
        prefs.putBytes(kBlobKey, &cached_, sizeof(AppConfig));
        prefs.end();
    } else {
        util::LogError(kTag, "failed to open NVS namespace for write");
    }
#endif
}

void ConfigStore::ResetToDefaults() { Save(DefaultConfig()); }

void ConfigStore::ResetSettingsPreservingConnection() {
    AppConfig current = Get();
    AppConfig fresh = DefaultConfig();
    // strlcpy, not strcpy: `current` may have come from a corrupted/stale NVS blob that
    // Load() couldn't fully validate (see the chargepoint_count clamp above) and isn't
    // guaranteed to be null-terminated within its fixed size.
    strlcpy(fresh.wifi_ssid, current.wifi_ssid, sizeof(fresh.wifi_ssid));
    strlcpy(fresh.wifi_password, current.wifi_password, sizeof(fresh.wifi_password));
    strlcpy(fresh.web_ui_pin, current.web_ui_pin, sizeof(fresh.web_ui_pin));
    strlcpy(fresh.device_name, current.device_name, sizeof(fresh.device_name));
    Save(fresh);
}

ConfigStore& Store() {
    static ConfigStore instance;
    return instance;
}

} // namespace config
