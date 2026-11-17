#include "config_store.h"

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
        if (stored_size == sizeof(AppConfig)) {
            prefs.getBytes(kBlobKey, &loaded, sizeof(AppConfig));
        } else if (stored_size > 0) {
            util::LogWarn(kTag, "stored config size mismatch, using defaults");
        }
        prefs.end();
    }
#endif

    if (loaded.schema_version != kCurrentSchemaVersion) {
        Migrate(loaded, loaded.schema_version);
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
    // strcpy, not strlcpy: both sides are fixed-size AppConfig arrays of the same type,
    // already null-terminated by construction.
    strcpy(fresh.wifi_ssid, current.wifi_ssid);
    strcpy(fresh.wifi_password, current.wifi_password);
    strcpy(fresh.web_ui_pin, current.web_ui_pin);
    strcpy(fresh.device_name, current.device_name);
    Save(fresh);
}

ConfigStore& Store() {
    static ConfigStore instance;
    return instance;
}

} // namespace config
