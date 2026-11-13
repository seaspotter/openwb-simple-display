// MQTT transport — the sole backend transport (plan §5, Context section). Replaces the
// retired simpleAPI HTTP client/poller/commands modules: connects to the broker
// configured in AppConfig, subscribes to the openWB-maintained `simpleAPI` MQTT mirror
// (`openWB/simpleAPI/chargepoint/<id>/#`) plus a handful of raw topics for fields that
// mirror doesn't carry, and updates state::AppState on every message. Writes publish a
// plain scalar to `openWB/simpleAPI/set/chargepoint/<id>/<param>` and let openWB's own
// `simpleAPI_mqtt.py` daemon do the charge_template read-merge-republish server-side —
// this project never touches that raw blob.
#pragma once

#include <cstdint>

#include "openwb_models.h"

namespace openwb {
namespace mqtt {

// Connects to the broker and subscribes. Call once from the net task after WiFi is up.
void Init();

// Pumps the underlying MQTT client's socket I/O and reconnect logic — non-blocking, call
// every net-task tick (replaces the old poll-tick timer; this is push-based, so there's
// no interval to gate on).
void Loop();

// True once Init() has connected and the connection is still alive — used to surface a
// "nicht verbunden" result for touch-triggered commands rather than silently dropping
// them (plan §5: writes are otherwise fire-and-forget, with no per-command ack).
bool IsConnected();

void SetChargemode(uint8_t chargepoint_id, ChargeMode mode);
void SetChargecurrent(uint8_t chargepoint_id, float amps);
void SetChargepointLock(uint8_t chargepoint_id, bool locked);

} // namespace mqtt
} // namespace openwb
