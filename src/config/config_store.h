// NVS-backed persistence for AppConfig, shared by the captive portal and the web UI.
#pragma once

#include "app_config.h"

namespace config {

class ConfigStore {
public:
    // Loads from NVS on first call, migrating if the stored schema_version is stale.
    // Callers must not mutate the returned reference — go through Update()/Save().
    const AppConfig& Get();

    // Replaces the in-memory config and persists it to NVS immediately.
    void Save(const AppConfig& config);

    // Copies Get(), applies `mutator`, saves — for a single-field change.
    template <typename Mutator>
    void Update(Mutator mutator) {
        AppConfig updated = Get();
        mutator(updated);
        Save(updated);
    }

    void ResetToDefaults();

private:
    void Load();
    void Migrate(AppConfig& config, uint8_t from_version);

    bool loaded_ = false;
    AppConfig cached_{};
};

// Process-wide instance, shared by the UI task and the net task.
ConfigStore& Store();

} // namespace config
