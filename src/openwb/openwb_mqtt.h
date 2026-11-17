// MQTT transport to openWB: subscribes to its `simpleAPI` mirror topics and updates
// state::AppState on every message. Writes publish a plain scalar to
// `openWB/simpleAPI/set/chargepoint/<id>/<param>`; openWB's own daemon does the
// charge_template merge server-side.
#pragma once

#include <cstdint>

#include "openwb_models.h"

namespace openwb {
namespace mqtt {

// Connects to the broker and subscribes. Call once from the net task after WiFi is up.
void Init();

// Pumps socket I/O and reconnect logic. Non-blocking; call every net-task tick.
void Loop();

// True once connected. Writes are otherwise fire-and-forget with no per-command ack.
bool IsConnected();

void SetChargemode(uint8_t chargepoint_id, ChargeMode mode);
void SetChargecurrent(uint8_t chargepoint_id, float amps);
void SetChargepointLock(uint8_t chargepoint_id, bool locked);

// Per-mode settings popup — type is "none" | "soc" | "amount"; soc is 0-100 (percent),
// amount is 1-50 (kWh). See
// https://wiki.openwb.de/doku.php?id=openwb:vc:2.3.x:simpleapi#schreiboperationen_set-topics
void SetInstantLimitType(uint8_t chargepoint_id, const char* type);
void SetInstantLimitSoc(uint8_t chargepoint_id, float soc_percent);
void SetInstantLimitAmount(uint8_t chargepoint_id, float amount_kwh);
void SetPvLimitType(uint8_t chargepoint_id, const char* type);
void SetPvLimitSoc(uint8_t chargepoint_id, float soc_percent);
void SetPvLimitAmount(uint8_t chargepoint_id, float amount_kwh);
// openWB reports max_price_eco in ct/kWh but the write topic expects EUR/kWh.
void SetMaxPriceEcoCt(uint8_t chargepoint_id, float price_ct);

} // namespace mqtt
} // namespace openwb
