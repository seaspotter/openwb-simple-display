// Pure topic/payload parsing for the MQTT backend (plan §5), split out from
// openwb_mqtt.h/.cpp so it has zero dependency on PubSubClient/WiFi and can be
// unit-tested on the host (test/test_openwb_mqtt) under the native_test PlatformIO env,
// which has no ESP toolchain available — same split as the old (now removed)
// openwb_response_parser.h used for the simpleAPI HTTP client.
//
// Every topic name and payload shape below was confirmed live against the user's openWB
// instance (`mosquitto_sub -h <host> -p 1883 -t 'openWB/#'`) and by reading
// core/simpleAPI/simpleAPI_mqtt.py in their fork — not guessed (plan §5).
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ArduinoJson.h>

#include "openwb_models.h"

namespace openwb {
namespace mqtt {

// snprintf rather than strlcpy: portable to the native_test host build, which doesn't
// reliably provide the BSD strlcpy that the ESP32 Arduino core does (same reasoning as
// ota_manifest.h).
inline void CopyString(char* dst, size_t dst_size, const char* src) { snprintf(dst, dst_size, "%s", src); }

inline bool ParseBool(const char* payload) {
    // openWB's simpleAPI MQTT mirror writes Python's str(bool) ("True"/"False",
    // confirmed live), not JSON/lowercase — both forms accepted defensively.
    return strcmp(payload, "True") == 0 || strcmp(payload, "true") == 0 || strcmp(payload, "1") == 0;
}

// Parses one field of `openWB/simpleAPI/chargepoint/<id>/<suffix>` into `out`. `suffix`
// is everything after the chargepoint id (e.g. "power", "soc/soc", "config/name").
// Returns false for an unrecognized suffix — the caller's wildcard subscribe is
// forward-compatible with fields this project doesn't parse, so an unmatched suffix is
// expected and silently ignored, not an error.
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
    } else {
        return false;
    }
    return true;
}

// `openWB/chargepoint/<id>/set/log`'s payload is a JSON object (unlike every other topic
// this project reads, which are plain scalars) — confirmed live, e.g.
// `{"imported_since_plugged": 0, "soc_at_start": null, ...}`. Only the one field this
// project shows ("Geladen") is extracted; everything else in that object is ignored.
inline void ParseLogPayload(const char* payload, ChargepointStatus& out) {
    JsonDocument doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        out.energy_charged_wh = doc["imported_since_plugged"] | 0.0f;
    }
}

// Parses one of the small set of global (non-chargepoint) topics this project
// subscribes to. Returns false for any other topic (the caller only calls this for
// topics it knows aren't chargepoint ones, but an exhaustive match here is still
// defensive against a stray topic reaching this function).
inline bool ParseGlobalTopic(const char* topic, const char* payload, GlobalStatus& out) {
    if (strcmp(topic, "openWB/pv/get/power") == 0) {
        // Negative while generating in this raw topic too (confirmed live, same
        // convention as the retired HTTP client's "pv-all" field) — negated back to
        // this project's "pv_power_w is always a positive generation magnitude"
        // convention, used elsewhere (e.g. the flow bars).
        out.pv_power_w = -static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/bat/get/power") == 0) {
        out.battery_power_w = static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/bat/get/soc") == 0) {
        out.battery_soc_percent = static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/consumer/get/power") == 0) {
        out.consumer_power_w = static_cast<float>(atof(payload));
    } else if (strcmp(topic, "openWB/simpleAPI/counter/get/power") == 0) {
        // The simpleAPI daemon's lowest-id/root counter mirror — confirmed live as the
        // actual grid meter for the user's install; same "assumes lowest id is the grid
        // root" caveat the retired HTTP client's `grid` field had internally already,
        // not a regression introduced by this module.
        out.grid_power_w = static_cast<float>(atof(payload));
    } else {
        return false;
    }
    return true;
}

} // namespace mqtt
} // namespace openwb
