// Pure topic/payload parsing for the MQTT backend — no PubSubClient/WiFi dependency, so
// it's unit-testable on the host (test/test_openwb_mqtt).
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ArduinoJson.h>

#include "openwb_models.h"

namespace openwb {
namespace mqtt {

inline void CopyString(char* dst, size_t dst_size, const char* src) { snprintf(dst, dst_size, "%s", src); }

// openWB writes Python's str(bool) ("True"/"False"), not JSON/lowercase.
inline bool ParseBool(const char* payload) {
    return strcmp(payload, "True") == 0 || strcmp(payload, "true") == 0 || strcmp(payload, "1") == 0;
}

// openWB's CounterMode enum (control/counter.py), as published in
// config/is_home_consumption_{counter,consumer} — confirmed against that source directly.
// "auto_home_consumption" inherits the parent's effective flag; for a direct child of the
// grid root (the only depth this project resolves — see openwb_mqtt.cpp), that parent is
// always the root itself.
enum class HcFlag : uint8_t { kAuto, kHome, kNotHome };

inline HcFlag ParseHcFlag(const char* payload) {
    if (strcmp(payload, "home_consumption") == 0) return HcFlag::kHome;
    if (strcmp(payload, "no_home_consumption") == 0) return HcFlag::kNotHome;
    return HcFlag::kAuto; // "auto_home_consumption", or not yet received
}

// Resolves a child's own flag against the grid root's effective flag (root_flag's own
// "auto" hardcodes to home consumption, matching openWB's own calculation — there's no
// grandparent for the root to inherit from) and reports whether the result is NOT home
// consumption — i.e. whether this child's power belongs in Verbraucher/Zähler rather than
// already being counted inside openWB's own home_consumption total.
inline bool ExcludedFromHomeConsumption(HcFlag own_flag, HcFlag root_flag) {
    bool root_is_home_consumption = root_flag != HcFlag::kNotHome;
    if (own_flag == HcFlag::kAuto) return !root_is_home_consumption;
    return own_flag == HcFlag::kNotHome;
}

// Parses one field of `openWB/simpleAPI/chargepoint/<id>/<suffix>`. Returns false for
// an unrecognized suffix (expected — the caller subscribes with a wildcard).
inline bool ParseChargepointField(const char* suffix, const char* payload, ChargepointStatus& out) {
    if (strcmp(suffix, "power") == 0) {
        out.power_w = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "plug_state") == 0) {
        out.plugged = ParseBool(payload);
    } else if (strcmp(suffix, "charge_state") == 0) {
        out.charging = ParseBool(payload);
    } else if (strcmp(suffix, "state_str") == 0) {
        CopyString(out.state_str, sizeof(out.state_str), payload);
    } else if (strcmp(suffix, "fault_str") == 0) {
        CopyString(out.fault_str, sizeof(out.fault_str), payload);
    } else if (strcmp(suffix, "fault_state") == 0) {
        out.fault_state = static_cast<uint8_t>(atoi(payload));
    } else if (strcmp(suffix, "chargemode") == 0) {
        CopyString(out.chargemode, sizeof(out.chargemode), payload);
    } else if (strcmp(suffix, "soc/soc") == 0) {
        out.soc_percent = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "soc/range_charged") == 0) {
        // Not "soc/range" (the vehicle's absolute total range, e.g. 153 km) — quartz's
        // "+X km" is the range gained since plugged in, confirmed live via
        // openWB/chargepoint/<id>/set/log's own "range_charged" field (same shape as
        // "imported_since_plugged" for energy).
        out.range_km = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "vehicle_name") == 0) {
        CopyString(out.connected_vehicle_name, sizeof(out.connected_vehicle_name), payload);
    } else if (strcmp(suffix, "phases_in_use") == 0) {
        out.phases_in_use = static_cast<uint8_t>(atoi(payload));
    } else if (strcmp(suffix, "evse_current") == 0) {
        out.evse_current_a = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "manual_lock") == 0) {
        out.locked = ParseBool(payload);
    } else if (strcmp(suffix, "config/name") == 0) {
        CopyString(out.name, sizeof(out.name), payload);
    } else if (strcmp(suffix, "instant_charging_limit") == 0) {
        CopyString(out.instant_limit_type, sizeof(out.instant_limit_type), payload);
    } else if (strcmp(suffix, "instant_charging_limit_soc") == 0) {
        out.instant_limit_soc = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "instant_charging_limit_amount") == 0) {
        out.instant_limit_amount_kwh = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "pv_charging_limit") == 0) {
        CopyString(out.pv_limit_type, sizeof(out.pv_limit_type), payload);
    } else if (strcmp(suffix, "pv_charging_limit_soc") == 0) {
        out.pv_limit_soc = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "pv_charging_limit_amount") == 0) {
        out.pv_limit_amount_kwh = static_cast<float>(atof(payload));
    } else if (strcmp(suffix, "max_price_eco") == 0) {
        out.max_price_eco_ct = static_cast<float>(atof(payload));
    } else {
        return false;
    }
    return true;
}

// `openWB/chargepoint/<id>/set/log`'s payload is a JSON object, unlike every other
// topic here. Only `imported_since_plugged` ("Geladen") is extracted.
inline void ParseLogPayload(const char* payload, ChargepointStatus& out) {
    JsonDocument doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        out.energy_charged_wh = doc["imported_since_plugged"] | 0.0f;
    }
}

// Parses one of the global (non-chargepoint) topics this project subscribes to.
inline bool ParseGlobalTopic(const char* topic, const char* payload, GlobalStatus& out) {
    if (strcmp(topic, "openWB/pv/get/power") == 0) {
        out.pv_power_w = -static_cast<float>(atof(payload)); // negative while generating
    } else if (strcmp(topic, "openWB/bat/get/power") == 0) {
        out.battery_power_w = static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/bat/get/soc") == 0) {
        out.battery_soc_percent = static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/simpleAPI/counter/set/home_consumption") == 0) {
        out.home_consumption_w = static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/simpleAPI/counter/get/power") == 0) {
        out.grid_power_w = static_cast<float>(atof(payload)); // assumes lowest id is the grid root
    } else {
        return false;
    }
    return true;
}

} // namespace mqtt
} // namespace openwb
