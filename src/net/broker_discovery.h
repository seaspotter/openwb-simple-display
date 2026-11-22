// Finds a real openWB MQTT broker on the local network instead of requiring the user to
// already know its IP — mirrors the approach confirmed working in a sibling project
// (kevinwieland/openWB-rotary-knob): an mDNS _mqtt._tcp query first (fast, if the
// broker's host advertises it), falling back to a local-subnet TCP scan on port 1883,
// with every candidate verified via a real MQTT handshake (subscribing to the retained
// openWB/system/version topic) rather than trusting "something answered on 1883".
#pragma once

#include <cstdint>

namespace net {

struct DiscoveredBroker {
    char host[16] = ""; // dotted-decimal IP
    uint16_t port = 1883;
};

constexpr uint8_t kMaxDiscoveredBrokers = 4;

// Starts the scan on its own dedicated task (the same reasoning as ota_manager.cpp's
// InstallTaskBody: a local-subnet scan can take tens of seconds, too long to block
// whichever task calls this — the UI task or the web server's request-handling task).
// No-op if a scan is already running.
void StartBrokerDiscovery();

bool IsBrokerDiscoveryRunning();

// 0-100 while scanning; -1 once finished or if none has ever run.
int BrokerDiscoveryProgressPercent();

// Only meaningful once IsBrokerDiscoveryRunning() is false and a scan has actually run.
uint8_t DiscoveredBrokerCount();
DiscoveredBroker GetDiscoveredBroker(uint8_t index);

// Non-empty if the most recent scan couldn't even attempt a meaningful search (e.g.
// WiFi not connected) — distinguishes "didn't look" from "looked and found nothing".
const char* BrokerDiscoveryError();

} // namespace net
