// Typed results decoded from openWB's MQTT topics — see openwb_mqtt_topics.h.
#pragma once

#include <cstdint>

namespace openwb {

struct ChargepointStatus {
    uint8_t id = 0;
    bool valid = false;

    float power_w = 0;
    bool plugged = false;
    bool charging = false;
    char state_str[96] = ""; // real messages can include a live countdown, e.g. "...in 19 Sek."
    char fault_str[64] = "";
    uint8_t fault_state = 0;

    char chargemode[24] = ""; // "instant_charging" | "pv_charging" | "eco_charging" |
                               // "scheduled_charging" | "stop"
    float soc_percent = -1;   // -1 = unknown
    float range_km = -1;      // "Reichweite"; -1 = unknown
    char connected_vehicle_name[48] = "";

    uint8_t phases_in_use = 0;
    float evse_current_a = 0;
    bool locked = false;

    char name[48] = "";          // the chargepoint's own name, not the vehicle's
    float energy_charged_wh = 0; // "Geladen" — MQTT-only, not in simpleAPI

    // Per-mode settings editable from the settings popup — see
    // https://wiki.openwb.de/doku.php?id=openwb:vc:2.3.x:simpleapi#schreiboperationen_set-topics
    char instant_limit_type[8] = "none"; // "none" | "soc" | "amount"
    float instant_limit_soc = 0;         // percent, 0-100
    float instant_limit_amount_kwh = 0;  // kWh, 1-50
    char pv_limit_type[8] = "none";      // "none" | "soc" | "amount"
    float pv_limit_soc = 0;
    float pv_limit_amount_kwh = 0;
    float max_price_eco_ct = 0; // ct/kWh — openWB reports this in ct, but the write topic
                                 // expects EUR (so ÷100 on write; see SetMaxPriceEcoCt)

    uint32_t last_update_ms = 0;
};

struct GlobalStatus {
    bool valid = false;

    float pv_power_w = 0;
    float grid_power_w = 0;
    float battery_power_w = 0;
    float battery_soc_percent = -1;   // -1 = no battery configured
    float consumer_power_w = 0;       // "Verbraucher" (other switchable consumer devices)
    float home_consumption_w = 0;     // "Hausverbrauch" (unmeasured household base load)
    float meter_power_w = 0;          // "Zähler" — sum of any extra sub-meters beyond the
                                       // grid root, auto-discovered via openWB's own
                                       // counter hierarchy topic (see openwb_mqtt.cpp)

    uint32_t last_update_ms = 0;
};

// kTarget only sets the mode via simpleAPI's `target` alias; the target SoC/time
// scheduling UI itself stays web-UI-only.
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

// Only "instant" (Sofort) takes a user-set fixed current; others compute it automatically.
inline bool ChargeModeHasManualCurrent(ChargeMode mode) { return mode == ChargeMode::kInstant; }

} // namespace openwb
