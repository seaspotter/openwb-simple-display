// Typed results decoded from openWB's MQTT topics (plan §5). Decouples the rest of the
// app from the raw topic/payload shapes, which is what openwb_mqtt_topics.h parses into
// these.
#pragma once

#include <cstdint>

namespace openwb {

// Mirrors the fields this project reads from `openWB/simpleAPI/chargepoint/<id>/#` (plus
// `name`/`energy_charged_wh`, sourced from two raw, non-simpleAPI topics — see
// openwb_mqtt.h). `chargemode` holds the full internal name openWB reports (e.g.
// "instant_charging"), not the short write-side alias.
struct ChargepointStatus {
    uint8_t id = 0;
    bool valid = false; // false until the first update is received for this slot

    float power_w = 0;
    bool plugged = false;
    bool charging = false;
    char state_str[48] = "";
    char fault_str[64] = "";
    uint8_t fault_state = 0;

    char chargemode[24] = ""; // "instant_charging" | "pv_charging" | "eco_charging" |
                               // "scheduled_charging" | "stop"
    float soc_percent = -1;   // -1 = unknown / no vehicle SoC reported
    char connected_vehicle_name[48] = "";

    uint8_t phases_in_use = 0;
    float evse_current_a = 0;
    bool locked = false;

    char name[48] = "";          // config_name: the chargepoint's own display name (e.g.
                                  // "MQTT-Ladepunkt 3"), not the connected vehicle's name
    float energy_charged_wh = 0; // "Geladen": imported_since_plugged from the raw
                                  // openWB/chargepoint/<id>/set/log topic — not available
                                  // via simpleAPI at all, MQTT-only (see plan §5)

    uint32_t last_update_ms = 0;
};

// Global (non-chargepoint) status: PV, grid, battery, other consumers ("Verbraucher").
// Deliberately doesn't model openWB's "Zähler" (additional sub-metering counters,
// `type: "counter"` entries in `openWB/simpleAPI/counter/get/hierarchy` beyond the root
// grid meter) — that's an install-specific, optionally-configured feature, not something
// every openWB setup has. If this gets added later: quartz's own JS bundle colors Zähler
// entries with `colorVar: '--grid'`, i.e. no dedicated palette token, just reuse
// `Palette::grid` (confirmed from the live theme's JS, not guessed).
struct GlobalStatus {
    bool valid = false;

    float pv_power_w = 0;
    float grid_power_w = 0;
    float battery_power_w = 0;
    float battery_soc_percent = -1; // -1 = no battery configured
    float consumer_power_w = 0;     // "Verbraucher": other household consumers, not
                                     // counting chargepoints or the battery

    uint32_t last_update_ms = 0;
};

// Charge modes exposed to the touch UI's mode_switch widget (plan §5/§6). All five
// modes openWB's own UI shows ("Sofort"/"PV"/"Ziel"/"Eco"/"Stop") — kTarget only sets
// the mode via simpleAPI's `target` alias; it does not expose the target SoC/time
// scheduling UI itself, which stays web-UI-only (plan §3's settings-scope decision).
enum class ChargeMode { kInstant, kPv, kTarget, kEco, kStop };

inline const char* ChargeModeToApiValue(ChargeMode mode) {
    switch (mode) {
        case ChargeMode::kInstant: return "instant";
        case ChargeMode::kPv: return "pv";
        case ChargeMode::kTarget: return "target";
        case ChargeMode::kEco: return "eco";
        case ChargeMode::kStop: return "stop";
    }
    return "stop";
}

// Only "instant" (Sofort/manual) mode takes a user-set fixed current — PV/Eco/Ziel
// compute it automatically and Stop charges at none, so the +/- current stepper only
// makes sense to show for this mode (confirmed by the user against openWB's own UI
// behavior, not just a styling choice).
inline bool ChargeModeHasManualCurrent(ChargeMode mode) { return mode == ChargeMode::kInstant; }

} // namespace openwb
