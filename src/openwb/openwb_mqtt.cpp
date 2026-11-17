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
// openWB/chargepoint/<id>/set/log's JSON payload (charging session totals) runs well
// past 256 bytes on a real install with charging history — measured ~800 bytes live.
// Must match the PubSubClient buffer set via setBufferSize() below, or the real MQTT
// packet arrives intact but gets truncated again by our own local copy of it.
constexpr size_t kMqttBufferSize = 1200;

WiFiClient g_plain_client;
WiFiClientSecure g_secure_client;
PubSubClient g_mqtt;

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

    // TEMP DIAGNOSTIC — remove once the name cross-contamination bug is found.
    if (strcmp(suffix, "config/name") == 0) {
        Serial.printf("[name-debug] topic-id=%u slot=%d payload=\"%s\"\n", id, slot, payload);
    }

    openwb::ChargepointStatus status = state::State().GetChargepointStatus(static_cast<uint8_t>(slot));
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

void HandleGlobalMessage(const char* topic, const char* payload) {
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

    for (uint8_t i = 0; i < cfg.chargepoint_count && i < config::AppConfig::kMaxChargepoints; ++i) {
        char topic[48];
        snprintf(topic, sizeof(topic), "openWB/simpleAPI/chargepoint/%u/#", cfg.chargepoint_ids[i]);
        g_mqtt.subscribe(topic);
        snprintf(topic, sizeof(topic), "openWB/chargepoint/%u/set/log", cfg.chargepoint_ids[i]);
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
    char topic[64];
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
    g_mqtt.loop();
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

} // namespace mqtt
} // namespace openwb
