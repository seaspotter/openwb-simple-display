#include "broker_discovery.h"

#include <cstdio>
#include <cstring>

#include <Arduino.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "util/logging.h"

namespace net {
namespace {

constexpr char kTag[] = "broker_discovery";
constexpr uint32_t kVerifyTimeoutMs = 1000;   // wait for the retained version topic
constexpr int32_t kTcpProbeTimeoutMs = 200;   // per-host connect attempt during the scan
constexpr uint8_t kMaxScanAddresses = 254;    // a /24's usable host range

DiscoveredBroker g_discovered[kMaxDiscoveredBrokers];
uint8_t g_discovered_count = 0;
volatile bool g_running = false;
volatile int g_progress_percent = -1;

// PubSubClient's callback is a plain function pointer (no user-data parameter in this
// version), so a result can't be captured by a lambda — a single static flag is fine
// here since verification only ever runs one candidate at a time, never concurrently.
volatile bool g_verify_got_version = false;

void OnVerifyMessage(char* /*topic*/, uint8_t* /*payload*/, unsigned int /*length*/) {
    g_verify_got_version = true;
}

bool AlreadyFound(IPAddress ip) {
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), "%s", ip.toString().c_str());
    for (uint8_t i = 0; i < g_discovered_count; ++i) {
        if (strcmp(g_discovered[i].host, ip_str) == 0) return true;
    }
    return false;
}

void AddDiscovered(IPAddress ip, uint16_t port) {
    if (g_discovered_count >= kMaxDiscoveredBrokers || AlreadyFound(ip)) return;
    snprintf(g_discovered[g_discovered_count].host, sizeof(g_discovered[g_discovered_count].host), "%s",
              ip.toString().c_str());
    g_discovered[g_discovered_count].port = port;
    ++g_discovered_count;
    util::LogInfo(kTag, "verified an openWB broker");
}

// Connects, subscribes to the one retained topic only a real openWB install publishes,
// and waits briefly for it — confirmed live against a real broker (not guessed) that
// "openWB/system/version" is retained and present. Rejects anything that merely has
// port 1883 open (a different MQTT broker, or nothing openWB-related at all).
bool VerifyOpenwbBroker(IPAddress ip, uint16_t port) {
    WiFiClient client;
    PubSubClient test_mqtt(client);
    test_mqtt.setServer(ip, port);
    test_mqtt.setCallback(OnVerifyMessage);
    g_verify_got_version = false;

    char client_id[24];
    snprintf(client_id, sizeof(client_id), "discovery-%08x", static_cast<unsigned>(millis()));
    if (!test_mqtt.connect(client_id)) return false;

    test_mqtt.subscribe("openWB/system/version");
    uint32_t start = millis();
    while (millis() - start < kVerifyTimeoutMs && !g_verify_got_version) {
        test_mqtt.loop();
        delay(10);
    }
    test_mqtt.disconnect();
    return g_verify_got_version;
}

void ScanViaMdns() {
    int count = MDNS.queryService("mqtt", "tcp");
    for (int i = 0; i < count && g_discovered_count < kMaxDiscoveredBrokers; ++i) {
        IPAddress ip = MDNS.IP(i);
        uint16_t port = MDNS.port(i);
        if (port == 0) port = 1883;
        if (VerifyOpenwbBroker(ip, port)) AddDiscovered(ip, port);
    }
}

// Only the common /24 (255.255.255.0) case — covers the vast majority of home networks;
// anything larger would make a sequential per-host scan impractically slow on this
// hardware anyway, and anything smaller has too few hosts to be worth scanning.
void ScanLocalSubnet() {
    IPAddress local = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    if (mask[0] != 255 || mask[1] != 255 || mask[2] != 255) {
        util::LogWarn(kTag, "subnet isn't a /24, skipping the local scan fallback");
        return;
    }

    for (uint16_t host = 1; host <= kMaxScanAddresses && g_discovered_count < kMaxDiscoveredBrokers; ++host) {
        g_progress_percent = static_cast<int>((host * 100) / kMaxScanAddresses);
        if (static_cast<uint8_t>(host) == local[3]) continue; // skip our own address

        IPAddress candidate(local[0], local[1], local[2], static_cast<uint8_t>(host));
        if (AlreadyFound(candidate)) continue;

        WiFiClient probe;
        if (probe.connect(candidate, 1883, kTcpProbeTimeoutMs)) {
            probe.stop();
            if (VerifyOpenwbBroker(candidate, 1883)) AddDiscovered(candidate, 1883);
        }
        taskYIELD();
    }
}

void DiscoveryTaskBody(void* /*param*/) {
    g_discovered_count = 0;
    g_progress_percent = 0;

    ScanViaMdns();
    // The subnet scan is the slow part (seconds to tens of seconds) — skipped entirely
    // if mDNS already found something, since that's both faster and more precise.
    if (g_discovered_count == 0) ScanLocalSubnet();

    g_progress_percent = -1;
    g_running = false;
    vTaskDelete(nullptr);
}

} // namespace

void StartBrokerDiscovery() {
    if (g_running) return;
    g_running = true;
    g_progress_percent = 0;
    xTaskCreatePinnedToCore(DiscoveryTaskBody, "broker_discovery", 8192, nullptr, 1, nullptr, 0);
}

bool IsBrokerDiscoveryRunning() { return g_running; }

int BrokerDiscoveryProgressPercent() { return g_progress_percent; }

uint8_t DiscoveredBrokerCount() { return g_discovered_count; }

DiscoveredBroker GetDiscoveredBroker(uint8_t index) {
    if (index >= g_discovered_count) return DiscoveredBroker{};
    return g_discovered[index];
}

} // namespace net
