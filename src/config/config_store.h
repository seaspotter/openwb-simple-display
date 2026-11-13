// NVS-backed persistence for AppConfig (plan §3). Single writer module shared by the
// captive portal, the on-device settings screen, and the web settings UI's /api/config
// handler — none of them parse/validate the config shape independently.
#pragma once

#include "app_config.h"

namespace config {

class ConfigStore {
public:
    // Loads from NVS on first call (applying Migrate() if a stale schema_version is
    // found), then serves the cached copy. Safe to call from any task; callers must not
    // mutate the returned reference directly — go through Update()/Save().
    const AppConfig& Get();

    // Replaces the in-memory config and persists it to NVS immediately.
    void Save(const AppConfig& config);

    // Convenience for a single-field change: copies Get(), applies `mutator`, saves.
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

// Process-wide instance. Deliberately not a function-local static singleton: config is
// read from both the UI task and the net task, and this makes that sharing explicit at
// every call site (`config::Store().Get()`) rather than hidden behind a bare function.
ConfigStore& Store();

} // namespace config
