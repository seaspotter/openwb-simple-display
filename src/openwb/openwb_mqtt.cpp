#include "openwb_mqtt.h"

#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <cstdio>
#include <cstring>

#include "config/config_store.h"
#include "openwb_mqtt_topics.h"
#include "state/app_state.h"
#include "util/logging.h"

namespace openwb {
namespace mqtt {
namespace {

constexpr char kTag[] = "openwb_mqtt";
constexpr char kSimpleApiChargepointPrefix[] = "openWB/simpleAPI/chargepoint/";
constexpr char kRawChargepointPrefix[] = "openWB/chargepoint/";
constexpr uint32_t kReconnectIntervalMs = 5000;
constexpr uint8_t kMaxMessagesPerLoop = 20; // drain a backlog instead of lagging behind it
// openWB/chargepoint/<id>/set/log's JSON payload (charging session totals) runs well
// past 256 bytes on a real install with charging history — measured ~800 bytes live.
// Must match the PubSubClient buffer set via setBufferSize() below, or the real MQTT
// packet arrives intact but gets truncated again by our own local copy of it.
constexpr size_t kMqttBufferSize = 1200;

WiFiClient g_plain_client;
WiFiClientSecure g_secure_client;
PubSubClient g_mqtt;

// Auto-discovers any extra sub-meter ("Zähler") beyond the grid root (counter id 0) via
// openWB's own counter hierarchy topic, instead of requiring the user to configure an
// id: openWB/simpleAPI/counter/get/power only ever mirrors the root counter (confirmed
// live), so a true "sum of every counter" needs to walk the tree like openWB's own
// frontend presumably does. Only direct children of the root are considered — on the
// install this was built against, extra counters appear there, not nested deeper.
constexpr char kHierarchyChildPrefix[] = "openWB/simpleAPI/counter/get/hierarchy/1/children/";
constexpr uint8_t kMaxHierarchyChildren = 16;
constexpr uint8_t kMaxExtraCounters = 4;

struct HierarchyChildSlot {
    uint8_t id = 0;
    bool id_known = false;
    bool is_counter = false;
};
HierarchyChildSlot g_hierarchy_children[kMaxHierarchyChildren];

uint8_t g_extra_counter_ids[kMaxExtraCounters] = {};
float g_extra_counter_power[kMaxExtraCounters] = {};
uint8_t g_extra_counter_count = 0;

void SubscribeExtraCounterPower(uint8_t id) {
    char topic[48];
    snprintf(topic, sizeof(topic), "openWB/simpleAPI/counter/%u/get/power", id);
    g_mqtt.subscribe(topic);
}

// Adds `id` to the discovered set and subscribes to its power topic, unless already known.
void EnsureExtraCounterDiscovered(uint8_t id) {
    for (uint8_t i = 0; i < g_extra_counter_count; ++i) {
        if (g_extra_counter_ids[i] == id) return;
    }
    if (g_extra_counter_count >= kMaxExtraCounters) return;
    g_extra_counter_ids[g_extra_counter_count] = id;
    g_extra_counter_power[g_extra_counter_count] = 0;
    ++g_extra_counter_count;
    SubscribeExtraCounterPower(id);
    util::LogInfo(kTag, "discovered an extra counter via the hierarchy topic");
}

void RecomputeExtraCounterTotal() {
    float total = 0;
    for (uint8_t i = 0; i < g_extra_counter_count; ++i) total += g_extra_counter_power[i];
    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    global.meter_power_w = total;
    global.valid = true;
    global.last_update_ms = millis();
    state::State().SetGlobalStatus(global);
}

// Handles one discovered extra counter's own get/power update. Returns false if `topic`
// isn't one of them (the common case for every other global message).
bool HandleExtraCounterPower(const char* topic, const char* payload) {
    for (uint8_t i = 0; i < g_extra_counter_count; ++i) {
        char expected[48];
        snprintf(expected, sizeof(expected), "openWB/simpleAPI/counter/%u/get/power", g_extra_counter_ids[i]);
        if (strcmp(topic, expected) == 0) {
            g_extra_counter_power[i] = static_cast<float>(atof(payload));
            RecomputeExtraCounterTotal();
            return true;
        }
    }
    return false;
}

// Matches "openWB/simpleAPI/counter/get/hierarchy/1/children/<n>/id" or ".../<n>/type".
// Returns false if `topic` isn't under that prefix at all.
bool HandleHierarchyChildMessage(const char* topic, const char* payload) {
    size_t prefix_len = strlen(kHierarchyChildPrefix);
    if (strncmp(topic, kHierarchyChildPrefix, prefix_len) != 0) return false;

    const char* rest = topic + prefix_len; // "<n>/id", "<n>/type", or deeper nesting
    const char* slash = strchr(rest, '/');
    if (slash == nullptr) return true;
    uint8_t slot = static_cast<uint8_t>(atoi(rest));
    if (slot == 0 || slot > kMaxHierarchyChildren) return true; // out of the small pool we track

    HierarchyChildSlot& child = g_hierarchy_children[slot - 1];
    const char* field = slash + 1;
    if (strcmp(field, "id") == 0) {
        child.id = static_cast<uint8_t>(atoi(payload));
        child.id_known = true;
    } else if (strcmp(field, "type") == 0) {
        child.is_counter = (strcmp(payload, "counter") == 0);
    } else {
        return true; // a deeper field, e.g. children/<n>/children/<m>/id — not tracked
    }

    if (child.id_known && child.is_counter && child.id != 0) EnsureExtraCounterDiscovered(child.id);
    return true;
}

// Maps an openWB-side chargepoint id to this project's fixed-size AppState slot.
int SlotForChargepointId(uint8_t id) {
    const config::AppConfig& cfg = config::Store().Get();
    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
        if (cfg.chargepoint_ids[i] == id) return i;
    }
    return -1;
}

void HandleChargepointMessage(uint8_t id, const char* suffix, const char* payload) {
    int slot = SlotForChargepointId(id);
    if (slot < 0) return;

    openwb::ChargepointStatus status = state::State().GetChargepointStatus(static_cast<uint8_t>(slot));

    // openWB only republishes a topic when its value changes, so when a vehicle unplugs and
    // a different one plugs in, fields the new vehicle hasn't republished yet (name/SoC/range/
    // session energy) would otherwise keep showing the previous vehicle's last known values.
    if (strcmp(suffix, "plug_state") == 0) {
        bool now_plugged = ParseBool(payload);
        if (status.plugged && !now_plugged) {
            status.connected_vehicle_name[0] = '\0';
            status.soc_percent = -1;
            status.range_km = -1;
            status.energy_charged_wh = 0;
        }
    }

    if (!ParseChargepointField(suffix, payload, status)) return; // unrecognized suffix
    status.id = id;
    status.valid = true;
    status.last_update_ms = millis();
    state::State().SetChargepointStatus(static_cast<uint8_t>(slot), status);
}

void HandleLogMessage(uint8_t id, const char* payload) {
    int slot = SlotForChargepointId(id);
    if (slot < 0) return;

    openwb::ChargepointStatus status = state::State().GetChargepointStatus(static_cast<uint8_t>(slot));
    ParseLogPayload(payload, status);
    state::State().SetChargepointStatus(static_cast<uint8_t>(slot), status);
}

// Read-only: openWB/chargepoint/<id>/set/charge_template is what simpleAPI_mqtt.py itself
// writes to when it applies an external command — reading from here directly mirrors in
// milliseconds (confirmed live: ~5ms for chargemode), instead of waiting for openWB's own
// control loop to next reconcile the simpleAPI GET-mirror topics (confirmed 3+ seconds
// behind, twice, for chargemode). Same applies to every per-mode setting nested in this
// same blob, so all of them are extracted here too for the settings popup. We never
// publish to this topic ourselves — doing a read-merge-write against the full blob from
// this device would risk corrupting unrelated settings (time_charging plans, other
// modes' parameters, etc.) if anything failed to round-trip exactly; writes stay on
// simpleAPI (openwb_mqtt.h's SetInstantLimit*/SetPvLimit*/SetMaxPriceEcoCt).
void HandleChargeTemplateMessage(uint8_t id, const char* payload) {
    int slot = SlotForChargepointId(id);
    if (slot < 0) return;

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return;
    JsonObject chargemode = doc["chargemode"].as<JsonObject>();
    if (chargemode.isNull()) return;

    openwb::ChargepointStatus status = state::State().GetChargepointStatus(static_cast<uint8_t>(slot));

    const char* selected = chargemode["selected"] | "";
    if (selected[0] != '\0') strlcpy(status.chargemode, selected, sizeof(status.chargemode));

    JsonObject instant = chargemode["instant_charging"].as<JsonObject>();
    if (!instant.isNull()) {
        status.instant_current_a = instant["current"] | status.instant_current_a;
        const char* limit_type = instant["limit"]["selected"] | "";
        if (limit_type[0] != '\0') {
            strlcpy(status.instant_limit_type, limit_type, sizeof(status.instant_limit_type));
        }
        status.instant_limit_soc = instant["limit"]["soc"] | status.instant_limit_soc;
        // charge_template stores the amount in Wh; the popup and the simpleAPI write
        // topic both work in kWh.
        float amount_wh = instant["limit"]["amount"] | (status.instant_limit_amount_kwh * 1000.0f);
        status.instant_limit_amount_kwh = amount_wh / 1000.0f;
    }

    JsonObject pv = chargemode["pv_charging"].as<JsonObject>();
    if (!pv.isNull()) {
        const char* limit_type = pv["limit"]["selected"] | "";
        if (limit_type[0] != '\0') strlcpy(status.pv_limit_type, limit_type, sizeof(status.pv_limit_type));
        status.pv_limit_soc = pv["limit"]["soc"] | status.pv_limit_soc;
        float amount_wh = pv["limit"]["amount"] | (status.pv_limit_amount_kwh * 1000.0f);
        status.pv_limit_amount_kwh = amount_wh / 1000.0f;
    }

    JsonObject eco = chargemode["eco_charging"].as<JsonObject>();
    if (!eco.isNull()) {
        // charge_template stores this in EUR/Wh; openWB's own GET mirror (and our popup)
        // use ct/kWh — confirmed live: 0.0002517 EUR/Wh == 25.17 ct/kWh, a factor of 100000.
        float price_eur_per_wh = eco["max_price"] | (status.max_price_eco_ct / 100000.0f);
        status.max_price_eco_ct = price_eur_per_wh * 100000.0f;
    }

    status.valid = true;
    status.last_update_ms = millis();
    state::State().SetChargepointStatus(static_cast<uint8_t>(slot), status);
}

void HandleGlobalMessage(const char* topic, const char* payload) {
    if (HandleHierarchyChildMessage(topic, payload)) return;
    if (HandleExtraCounterPower(topic, payload)) return;

    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    if (!ParseGlobalTopic(topic, payload, global)) return;
    global.valid = true;
    global.last_update_ms = millis();
    state::State().SetGlobalStatus(global);
}

// Matches "<prefix><id>/<rest>" and splits out the id and the part after the next '/'.
bool ParseIdAfterPrefix(const char* topic, const char* prefix, uint8_t* out_id, const char** out_rest) {
    size_t prefix_len = strlen(prefix);
    if (strncmp(topic, prefix, prefix_len) != 0) return false;
    const char* rest = topic + prefix_len;
    const char* slash = strchr(rest, '/');
    if (slash == nullptr) return false;
    *out_id = static_cast<uint8_t>(atoi(rest));
    *out_rest = slash + 1;
    return true;
}

void OnMessage(char* topic, uint8_t* payload_bytes, unsigned int length) {
    char payload[kMqttBufferSize];
    size_t copy_len = length < sizeof(payload) - 1 ? length : sizeof(payload) - 1;
    memcpy(payload, payload_bytes, copy_len);
    payload[copy_len] = '\0';

    uint8_t id;
    const char* rest;
    if (ParseIdAfterPrefix(topic, kSimpleApiChargepointPrefix, &id, &rest)) {
        HandleChargepointMessage(id, rest, payload);
        return;
    }
    if (ParseIdAfterPrefix(topic, kRawChargepointPrefix, &id, &rest)) {
        if (strcmp(rest, "set/log") == 0) HandleLogMessage(id, payload);
        else if (strcmp(rest, "set/charge_template") == 0) HandleChargeTemplateMessage(id, payload);
        return;
    }
    HandleGlobalMessage(topic, payload);
}

void Subscribe() {
    const config::AppConfig& cfg = config::Store().Get();

    g_mqtt.subscribe("openWB/pv/get/power");
    g_mqtt.subscribe("openWB/bat/get/power");
    g_mqtt.subscribe("openWB/bat/get/soc");
    g_mqtt.subscribe("openWB/consumer/get/power");
    g_mqtt.subscribe("openWB/simpleAPI/counter/get/power");
    g_mqtt.subscribe("openWB/simpleAPI/counter/set/home_consumption");
    g_mqtt.subscribe("openWB/simpleAPI/counter/get/hierarchy/1/children/+/id");
    g_mqtt.subscribe("openWB/simpleAPI/counter/get/hierarchy/1/children/+/type");
    // Already-discovered extra counters (from a prior connection) need re-subscribing
    // explicitly on every fresh connect — a new MQTT session has no memory of them, and
    // they won't be rediscovered until/unless the hierarchy topic happens to republish.
    for (uint8_t i = 0; i < g_extra_counter_count; ++i) SubscribeExtraCounterPower(g_extra_counter_ids[i]);

    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
        char topic[48];
        snprintf(topic, sizeof(topic), "openWB/simpleAPI/chargepoint/%u/#", cfg.chargepoint_ids[i]);
        g_mqtt.subscribe(topic);
        snprintf(topic, sizeof(topic), "openWB/chargepoint/%u/set/log", cfg.chargepoint_ids[i]);
        g_mqtt.subscribe(topic);
        snprintf(topic, sizeof(topic), "openWB/chargepoint/%u/set/charge_template", cfg.chargepoint_ids[i]);
        g_mqtt.subscribe(topic);
    }
}

bool Connect() {
    const config::AppConfig& cfg = config::Store().Get();

    Client* net_client = &g_plain_client;
    if (cfg.mqtt_use_tls) {
        if (!cfg.mqtt_validate_cert) g_secure_client.setInsecure(); // broker cert is self-signed
        net_client = &g_secure_client;
    }
    g_mqtt.setClient(*net_client);
    g_mqtt.setServer(cfg.mqtt_host, cfg.mqtt_port);
    g_mqtt.setCallback(OnMessage);
    g_mqtt.setBufferSize(kMqttBufferSize);

    char client_id[40];
    snprintf(client_id, sizeof(client_id), "%s-%08x", cfg.device_name,
              static_cast<unsigned>(ESP.getEfuseMac() & 0xFFFFFFFF));

    bool connected = cfg.mqtt_username[0] != '\0'
                          ? g_mqtt.connect(client_id, cfg.mqtt_username, cfg.mqtt_password)
                          : g_mqtt.connect(client_id);

    if (connected) {
        util::LogInfo(kTag, "connected to broker");
        Subscribe();
    } else {
        util::LogWarn(kTag, "broker connect failed");
    }
    return connected;
}

void Publish(uint8_t chargepoint_id, const char* param, const char* value) {
    // "openWB/simpleAPI/set/chargepoint/<id>/instant_charging_limit_amount" alone is 65
    // bytes including the null terminator — a 64-byte buffer silently truncated it to
    // ".../instant_charging_limit_amoun", a topic openWB never recognized, so the write
    // was dropped with no error. Sized with real headroom instead of the next-exact fit.
    char topic[96];
    snprintf(topic, sizeof(topic), "openWB/simpleAPI/set/chargepoint/%u/%s", chargepoint_id, param);
    g_mqtt.publish(topic, value, /*retained=*/true);
}

} // namespace

void Init() { Connect(); }

void Loop() {
    if (!g_mqtt.connected()) {
        static uint32_t last_attempt_ms = 0;
        uint32_t now = millis();
        if (now - last_attempt_ms < kReconnectIntervalMs) return;
        last_attempt_ms = now;
        Connect();
        return;
    }
    // PubSubClient::loop() reads at most one queued MQTT packet per call (confirmed in
    // its source: `if (_client->available())`, not a `while`). The net task only calls
    // Loop() once per 50ms tick, so a burst of topics changing together (e.g. several
    // fields after a chargemode switch on openWB) could take many ticks to fully drain
    // — seconds of real lag behind what the broker already has. Draining a bounded
    // batch per call catches up immediately; it's cheap when there's nothing queued,
    // since loop() just checks the keepalive timer and returns.
    for (uint8_t i = 0; i < kMaxMessagesPerLoop && g_mqtt.connected(); ++i) {
        g_mqtt.loop();
    }
}

bool IsConnected() { return g_mqtt.connected(); }

void SetChargemode(uint8_t chargepoint_id, ChargeMode mode) {
    Publish(chargepoint_id, "chargemode", ChargeModeToApiValue(mode));
}

void SetChargecurrent(uint8_t chargepoint_id, float amps) {
    char value[8];
    snprintf(value, sizeof(value), "%d", static_cast<int>(amps));
    Publish(chargepoint_id, "chargecurrent", value);
}

void SetChargepointLock(uint8_t chargepoint_id, bool locked) {
    Publish(chargepoint_id, "chargepoint_lock", locked ? "true" : "false");
}

void SetInstantLimitType(uint8_t chargepoint_id, const char* type) {
    Publish(chargepoint_id, "instant_charging_limit", type);
}

void SetInstantLimitSoc(uint8_t chargepoint_id, float soc_percent) {
    char value[8];
    snprintf(value, sizeof(value), "%d", static_cast<int>(soc_percent));
    Publish(chargepoint_id, "instant_charging_limit_soc", value);
}

void SetInstantLimitAmount(uint8_t chargepoint_id, float amount_kwh) {
    char value[8];
    snprintf(value, sizeof(value), "%d", static_cast<int>(amount_kwh));
    Publish(chargepoint_id, "instant_charging_limit_amount", value);
}

void SetPvLimitType(uint8_t chargepoint_id, const char* type) {
    Publish(chargepoint_id, "pv_charging_limit", type);
}

void SetPvLimitSoc(uint8_t chargepoint_id, float soc_percent) {
    char value[8];
    snprintf(value, sizeof(value), "%d", static_cast<int>(soc_percent));
    Publish(chargepoint_id, "pv_charging_limit_soc", value);
}

void SetPvLimitAmount(uint8_t chargepoint_id, float amount_kwh) {
    char value[8];
    snprintf(value, sizeof(value), "%d", static_cast<int>(amount_kwh));
    Publish(chargepoint_id, "pv_charging_limit_amount", value);
}

void SetMaxPriceEcoCt(uint8_t chargepoint_id, float price_ct) {
    // Despite the wiki calling this topic's unit "EUR" (example "0.25"), empirically
    // openWB expects the plain ct/kWh number and does its own ct->EUR/Wh conversion
    // (confirmed live: sending price_ct/100 here, i.e. pre-converting to EUR, produced
    // a stored price 100x too small — exactly the gap closed by not pre-dividing).
    char value[16];
    snprintf(value, sizeof(value), "%.1f", price_ct);
    Publish(chargepoint_id, "max_price_eco", value);
}

} // namespace mqtt
} // namespace openwb
