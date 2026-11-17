#pragma once

#include <cstring>

#include "openwb/openwb_models.h"
#include "state/app_state.h"

namespace sim {

inline void SeedFakeState() {
    openwb::GlobalStatus global{};
    global.valid = true;
    global.pv_power_w = 3240;
    global.grid_power_w = -850;
    global.battery_power_w = 410;
    global.consumer_power_w = 620;
    global.home_consumption_w = 870;
    global.meter_power_w = 43;
    global.battery_soc_percent = 72;
    state::State().SetGlobalStatus(global);

    openwb::ChargepointStatus cp0{};
    cp0.id = 1;
    cp0.valid = true;
    cp0.plugged = true;
    cp0.charging = true;
    cp0.power_w = 7360;
    std::strncpy(cp0.chargemode, "pv_charging", sizeof(cp0.chargemode) - 1);
    cp0.soc_percent = 58;
    cp0.range_km = 180;
    std::strncpy(cp0.connected_vehicle_name, "Model 3", sizeof(cp0.connected_vehicle_name) - 1);
    cp0.phases_in_use = 3;
    cp0.evse_current_a = 16;
    std::strncpy(cp0.name, "MQTT-Ladepunkt 3", sizeof(cp0.name) - 1);
    cp0.energy_charged_wh = 2400;
    std::strncpy(cp0.pv_limit_type, "soc", sizeof(cp0.pv_limit_type) - 1);
    cp0.pv_limit_soc = 80;
    state::State().SetChargepointStatus(0, cp0);

    openwb::ChargepointStatus cp1{};
    cp1.id = 2;
    cp1.valid = true;
    std::strncpy(cp1.state_str, "Keine Ladung, da kein Auto angesteckt ist.", sizeof(cp1.state_str) - 1);
    std::strncpy(cp1.chargemode, "stop", sizeof(cp1.chargemode) - 1);
    std::strncpy(cp1.name, "MQTT-Ladepunkt 4", sizeof(cp1.name) - 1);
    state::State().SetChargepointStatus(1, cp1);
}

} // namespace sim
