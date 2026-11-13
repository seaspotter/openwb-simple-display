// Interactive LVGL desktop simulator: a real window via LVGL's bundled SDL driver,
// mouse acting as the touchscreen. Lets you actually click through mode switches, the
// +/- current stepper, the lock button, and swipe between chargepoints (click-drag)
// before any hardware exists. Uses the exact same src/ui/* code the real firmware runs
// — see src/sim/sim_stubs.cpp for what replaces the ESP-only board::/net:: backends.
//
// Needs libsdl2-dev installed. Build and run:
//   pio run -e sim_interactive
//   .pio/build/sim_interactive/program
#include <cstdio>
#include <cstring>

#include <lvgl.h>

#include <SDL2/SDL.h>

#include "state/app_state.h"
#include "ui/screens/screen_boot.h"
#include "ui/screens/screen_chargepoint.h"
#include "ui/screens/screen_provisioning.h"
#include "ui/screens/screen_status.h"
#include "ui/theme/theme_quartz.h"
#include "ui/ui_events.h"

namespace {

constexpr int32_t kWidth = 480;
constexpr int32_t kHeight = 480;

void SeedFakeState() {
    openwb::GlobalStatus global{};
    global.valid = true;
    global.pv_power_w = 3240;
    global.grid_power_w = -850;
    global.battery_power_w = 410;
    global.consumer_power_w = 620;
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
    std::strncpy(cp0.connected_vehicle_name, "Model 3", sizeof(cp0.connected_vehicle_name) - 1);
    cp0.phases_in_use = 3;
    cp0.evse_current_a = 16;
    std::strncpy(cp0.name, "MQTT-Ladepunkt 3", sizeof(cp0.name) - 1);
    cp0.energy_charged_wh = 2400;
    state::State().SetChargepointStatus(0, cp0);

    openwb::ChargepointStatus cp1{};
    cp1.id = 2;
    cp1.valid = true;
    std::strncpy(cp1.state_str, "Keine Ladung, da kein Auto angesteckt ist.", sizeof(cp1.state_str) - 1);
    std::strncpy(cp1.chargemode, "stop", sizeof(cp1.chargemode) - 1);
    std::strncpy(cp1.name, "MQTT-Ladepunkt 4", sizeof(cp1.name) - 1);
    state::State().SetChargepointStatus(1, cp1);
}

} // namespace

int main(int argc, char** argv) {
    bool multi = argc > 1 && std::strcmp(argv[1], "--multi") == 0;

    lv_init();

    lv_display_t* display = lv_sdl_window_create(kWidth, kHeight);
    lv_sdl_window_set_title(display, "openwb-simple-display (sim)");
    lv_indev_t* mouse = lv_sdl_mouse_create();
    (void)mouse;

    SeedFakeState();
    theme::ApplyQuartzTheme(display, /*dark_mode=*/false);
    ui::InitEvents();

    screens::ShowStatusScreen(multi ? 2 : 1);

    std::printf("Simulator running. Close the window to quit.\n");
    std::printf("Tip: run with --multi to see the swipeable multi-chargepoint pager.\n");

    uint32_t last_tick = SDL_GetTicks();
    for (;;) {
        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        lv_timer_handler();
        SDL_Delay(5);
    }

    return 0;
}
