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

// Auto-discovers any extra sub-counter ("Zähler") or consumer ("Verbraucher") beyond the
// grid root (counter id 1) via openWB's own counter hierarchy topic, instead of requiring
// the user to configure an id. Only direct children of the root are considered — on the
// install this was built against, these appear there, not nested deeper; a child with
// further children of its own (so that this project would need to subtract THEIR power
// out too, the way openWB's own "local_power" does server-side) isn't handled — a known
// gap, not attempted here given the added complexity of walking an arbitrarily deep tree
// on-device for a case not seen in practice.
//
// Each discovered child's power is only ever added to Verbraucher/Zähler if its effective
// home-consumption flag resolves to NOT home consumption — confirmed by reading openWB's
// own calculation (control/counter_all/counter_all.py::_calc_home_consumption_from_counter)
// and its web frontend's independent re-derivation of the same thing
// (web_themes/koala/source/src/stores/mqtt-store.ts::homeConsumptionAssignment): a child
// flagged (explicitly, or via "auto" inheriting its parent's — here, the grid root's —
// effective flag, the default) AS home consumption is already included inside
// home_consumption_w by openWB itself, so adding it again here would double-count it.
constexpr char kHierarchyChildPrefix[] = "openWB/simpleAPI/counter/get/hierarchy/1/children/";
constexpr uint8_t kMaxHierarchyChildren = 16;
constexpr uint8_t kMaxExtraCounters = 4;
constexpr uint8_t kMaxExtraConsumers = 4;
constexpr uint8_t kRootCounterId = 1;

struct HierarchyChildSlot {
    uint8_t id = 0;
    bool id_known = false;
    bool is_counter = false;
    bool is_consumer = false;    // ComponentType.CONSUMER.value == "consumer" in openWB
    bool is_chargepoint = false; // ComponentType.CHARGEPOINT.value == "cp" in openWB
};
HierarchyChildSlot g_hierarchy_children[kMaxHierarchyChildren];

// The grid root's own flag — needed to resolve any child's "auto" value (see
// ExcludedFromHomeConsumption in openwb_mqtt_topics.h).
HcFlag g_root_flag = HcFlag::kAuto;

// Shared shape for both discovered-extra-counter and discovered-extra-consumer tracking —
// kept as one type (rather than two identical structs) specifically so the subscribe/
// match logic below can be one parameterized set of functions instead of two near-
// identical copies that could silently drift apart (exactly how an earlier draft of this
// fix ended up with a buffer-size bug on only the consumer side).
struct ExtraComponent {
    uint8_t id = 0;
    float power = 0;
    HcFlag flag = HcFlag::kAuto;
};
ExtraComponent g_extra_counters[kMaxExtraCounters];
uint8_t g_extra_counter_count = 0;

ExtraComponent g_extra_consumers[kMaxExtraConsumers];
uint8_t g_extra_consumer_count = 0;

// Every chargepoint id openWB actually reports via the hierarchy topic, independent of
// which ones are configured — feeds the web UI's pick-list (see api_routes.cpp), so a
// stale/typo'd id can be caught before it's ever saved instead of silently showing no
// data (the exact bug this was built to prevent).
constexpr uint8_t kMaxDiscoveredChargepoints = 8;
openwb::DiscoveredChargepoint g_discovered_chargepoints[kMaxDiscoveredChargepoints];
uint8_t g_discovered_chargepoint_count = 0;

// `component` is "counter" or "consumer", `flag_field` the matching
// is_home_consumption_{counter,consumer} config key — see ExtraComponent's comment for why
// this is one parameterized function instead of two copies. Sized for the longest real
// topic this builds: "openWB/simpleAPI/consumer/255/config/is_home_consumption_consumer"
// is 65 chars + NUL; a tighter buffer previously truncated it for 2+ digit ids, silently
// subscribing to a topic the broker never publishes.
//
// Raw openWB never gives is_home_consumption_consumer its own topic (it's one key in the
// consumer's whole config JSON blob) — but simpleAPI's generic mirror expands any JSON
// object value into per-key sub-topics, so it IS individually addressable here even though
// it isn't on the raw, non-simpleAPI broker (confirmed by reading simpleAPI_mqtt.py's
// `_expand_value_to_topics` and consumer_data.py's `ConsumerConfig`, which carries no
// per-field topic metadata of its own, unlike the counter equivalent).
void SubscribeExtraComponent(const char* component, const char* flag_field, uint8_t id) {
    char topic[80];
    snprintf(topic, sizeof(topic), "openWB/simpleAPI/%s/%u/get/power", component, id);
    g_mqtt.subscribe(topic);
    snprintf(topic, sizeof(topic), "openWB/simpleAPI/%s/%u/config/%s", component, id, flag_field);
    g_mqtt.subscribe(topic);
}

void SubscribeExtraCounter(uint8_t id) { SubscribeExtraComponent("counter", "is_home_consumption_counter", id); }
void SubscribeExtraConsumer(uint8_t id) { SubscribeExtraComponent("consumer", "is_home_consumption_consumer", id); }

// Scans `items[0..*count)` for an existing `id`; if it's new and there's room, appends a
// default-constructed slot with that `id` set, increments `*count`, and returns true. Bails
// out (false) if `id` is already known or the array is full. This is the one "find-or-add,
// bounded" shape behind every Ensure*Discovered function below (extra counters, extra
// consumers, chargepoints) — each only differs in what runs once something new is actually
// added, which stays in the caller rather than being copy-pasted here three times.
template <typename T, uint8_t N>
bool FindOrAdd(T (&items)[N], uint8_t* count, uint8_t id) {
    for (uint8_t i = 0; i < *count; ++i) {
        if (items[i].id == id) return false;
    }
    if (*count >= N) return false;
    items[*count] = T{};
    items[*count].id = id;
    ++*count;
    return true;
}

// Adds `id` to the discovered set and subscribes to its power + home-consumption-flag
// topics, unless already known.
void EnsureExtraCounterDiscovered(uint8_t id) {
    if (!FindOrAdd(g_extra_counters, &g_extra_counter_count, id)) return;
    SubscribeExtraCounter(id);
    util::LogInfo(kTag, "discovered an extra counter via the hierarchy topic");
}

// Adds `id` to the discovered set and subscribes to its power + home-consumption-flag
// topics, unless already known.
void EnsureExtraConsumerDiscovered(uint8_t id) {
    if (!FindOrAdd(g_extra_consumers, &g_extra_consumer_count, id)) return;
    SubscribeExtraConsumer(id);
    util::LogInfo(kTag, "discovered an extra consumer via the hierarchy topic");
}

void PublishDiscoveredChargepoints() {
    state::State().SetDiscoveredChargepoints(g_discovered_chargepoints, g_discovered_chargepoint_count);
}

void SubscribeChargepointName(uint8_t id) {
    char topic[64];
    snprintf(topic, sizeof(topic), "openWB/simpleAPI/chargepoint/%u/config/name", id);
    g_mqtt.subscribe(topic);
}

// Adds `id` to the discovered set, unless already known. Already-configured chargepoints
// are already subscribed to their full `#` wildcard (see Subscribe()), which includes
// config/name — subscribing again here is a harmless duplicate. For a discovered-but-
// not-configured one, this is the only topic fetched for it, keeping discovery cheap.
void EnsureChargepointDiscovered(uint8_t id) {
    if (!FindOrAdd(g_discovered_chargepoints, &g_discovered_chargepoint_count, id)) return;
    SubscribeChargepointName(id);
    PublishDiscoveredChargepoints();
    util::LogInfo(kTag, "discovered a chargepoint via the hierarchy topic");
}

// Records a discovered (not necessarily configured) chargepoint's name. Returns false if
// `id` isn't a known discovered chargepoint — the common case being one that IS
// configured, where the name instead flows through HandleChargepointMessage as normal.
bool HandleDiscoveredChargepointName(uint8_t id, const char* name) {
    for (uint8_t i = 0; i < g_discovered_chargepoint_count; ++i) {
        if (g_discovered_chargepoints[i].id == id) {
            strlcpy(g_discovered_chargepoints[i].name, name, sizeof(g_discovered_chargepoints[i].name));
            PublishDiscoveredChargepoints();
            return true;
        }
    }
    return false;
}

void RecomputeExtraCounterTotal() {
    float total = 0;
    for (uint8_t i = 0; i < g_extra_counter_count; ++i) {
        if (ExcludedFromHomeConsumption(g_extra_counters[i].flag, g_root_flag)) total += g_extra_counters[i].power;
    }
    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    global.meter_power_w = total;
    global.valid = true;
    global.last_update_ms = millis();
    state::State().SetGlobalStatus(global);
}

void RecomputeExtraConsumerTotal() {
    float total = 0;
    for (uint8_t i = 0; i < g_extra_consumer_count; ++i) {
        if (ExcludedFromHomeConsumption(g_extra_consumers[i].flag, g_root_flag)) total += g_extra_consumers[i].power;
    }
    openwb::GlobalStatus global = state::State().GetGlobalStatus();
    global.consumer_power_w = total;
    global.valid = true;
    global.last_update_ms = millis();
    state::State().SetGlobalStatus(global);
}

// Matches one discovered extra counter's/consumer's own get/power or home-consumption-flag
// update against `items[0..count)` and updates it in place. Returns false if `topic` isn't
// one of them (the common case for every other global message) — see
// SubscribeExtraComponent for why `component`/`flag_field` are parameters rather than two
// copies of this function.
bool HandleExtraComponentMessage(const char* component, const char* flag_field, ExtraComponent* items, uint8_t count,
                                   void (*recompute)(), const char* topic, const char* payload) {
    for (uint8_t i = 0; i < count; ++i) {
        char expected[80];
        snprintf(expected, sizeof(expected), "openWB/simpleAPI/%s/%u/get/power", component, items[i].id);
        if (strcmp(topic, expected) == 0) {
            items[i].power = static_cast<float>(atof(payload));
            recompute();
            return true;
        }
        snprintf(expected, sizeof(expected), "openWB/simpleAPI/%s/%u/config/%s", component, items[i].id, flag_field);
        if (strcmp(topic, expected) == 0) {
            items[i].flag = ParseHcFlag(payload);
            recompute();
            return true;
        }
    }
    return false;
}

bool HandleExtraCounterMessage(const char* topic, const char* payload) {
    return HandleExtraComponentMessage("counter", "is_home_consumption_counter", g_extra_counters,
                                        g_extra_counter_count, RecomputeExtraCounterTotal, topic, payload);
}

bool HandleExtraConsumerMessage(const char* topic, const char* payload) {
    return HandleExtraComponentMessage("consumer", "is_home_consumption_consumer", g_extra_consumers,
                                        g_extra_consumer_count, RecomputeExtraConsumerTotal, topic, payload);
}

// The grid root's own flag affects every discovered child whose own flag is "auto" — a
// change here can flip which children count as excluded, so both totals get recomputed.
bool HandleRootCounterFlag(const char* topic, const char* payload) {
    char expected[64];
    snprintf(expected, sizeof(expected), "openWB/simpleAPI/counter/%u/config/is_home_consumption_counter",
              kRootCounterId);
    if (strcmp(topic, expected) != 0) return false;
    g_root_flag = ParseHcFlag(payload);
    RecomputeExtraCounterTotal();
    RecomputeExtraConsumerTotal();
    return true;
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
        child.is_consumer = (strcmp(payload, "consumer") == 0);
        child.is_chargepoint = (strcmp(payload, "cp") == 0);
    } else {
        return true; // a deeper field, e.g. children/<n>/children/<m>/id — not tracked
    }

    if (child.id_known && child.is_counter && child.id != 0) EnsureExtraCounterDiscovered(child.id);
    if (child.id_known && child.is_consumer && child.id != 0) EnsureExtraConsumerDiscovered(child.id);
    if (child.id_known && child.is_chargepoint && child.id != 0) EnsureChargepointDiscovered(child.id);
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
    // Kept in sync regardless of whether `id` is currently configured — a chargepoint
    // the user already enabled is just as real a discovery-list entry as one they
    // haven't, so the web UI's pick-list can show a name for either.
    if (strcmp(suffix, "config/name") == 0) HandleDiscoveredChargepointName(id, payload);

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
    if (HandleRootCounterFlag(topic, payload)) return;
    if (HandleExtraCounterMessage(topic, payload)) return;
    if (HandleExtraConsumerMessage(topic, payload)) return;

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
    g_mqtt.subscribe("openWB/simpleAPI/counter/get/power");
    g_mqtt.subscribe("openWB/simpleAPI/counter/set/home_consumption");
    char root_flag_topic[64];
    snprintf(root_flag_topic, sizeof(root_flag_topic),
              "openWB/simpleAPI/counter/%u/config/is_home_consumption_counter", kRootCounterId);
    g_mqtt.subscribe(root_flag_topic);
    g_mqtt.subscribe("openWB/simpleAPI/counter/get/hierarchy/1/children/+/id");
    g_mqtt.subscribe("openWB/simpleAPI/counter/get/hierarchy/1/children/+/type");
    // Already-discovered extra counters/consumers (from a prior connection) need
    // re-subscribing explicitly on every fresh connect — a new MQTT session has no memory
    // of them, and they won't be rediscovered until/unless the hierarchy topic republishes.
    for (uint8_t i = 0; i < g_extra_counter_count; ++i) SubscribeExtraCounter(g_extra_counters[i].id);
    for (uint8_t i = 0; i < g_extra_consumer_count; ++i) SubscribeExtraConsumer(g_extra_consumers[i].id);

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

    char client_id[48]; // device_name (<=31 chars) + '-' + 8 hex chars + NUL
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
