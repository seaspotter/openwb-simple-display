// Headless LVGL "screenshot" tool: renders each of this project's real screens
// (src/ui/screens/*) into an off-screen software framebuffer and dumps each one to a
// .bmp file — no SDL, no hardware, no root access needed. Populates state::AppState
// with representative fake data first, so the screenshots look like a real session
// rather than all-zero placeholders.
//
// Output: src/sim/out/*.bmp (gitignored — regenerate on demand with
// `pio run -e sim_screenshot` then run the produced binary directly).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <lvgl.h>

#include "config/config_store.h"
#include "openwb/openwb_models.h"
#include "state/app_state.h"
#include "ui/screens/screen_boot.h"
#include "ui/screens/screen_chargepoint.h"
#include "ui/screens/screen_provisioning.h"
#include "ui/screens/screen_status.h"
#include "ui/theme/theme_quartz.h"
#include "ui/wifi_reset_prompt.h"

namespace {

constexpr int32_t kWidth = 480;
constexpr int32_t kHeight = 480;

uint16_t g_framebuffer[kWidth * kHeight]; // RGB565, matches lv_conf.h's LV_COLOR_DEPTH

void FlushCb(lv_display_t* disp, const lv_area_t* /*area*/, uint8_t* /*px_map*/) {
    // Rendering happens directly into g_framebuffer (it's the buffer handed to
    // lv_display_set_buffers below, used in DIRECT render mode), so there's nothing
    // left to copy here — just acknowledge the flush.
    lv_display_flush_ready(disp);
}

// Writes a 24bpp BMP from the RGB565 framebuffer (BMP's bottom-up row order handled
// here too) — picked over PNG so this tool has zero third-party dependencies; convert
// afterward with `convert *.bmp` (ImageMagick) or Python/PIL, both commonly available.
void WriteBmp(const char* path) {
    const int row_size = ((kWidth * 3 + 3) / 4) * 4; // BMP rows are 4-byte aligned
    const int pixel_data_size = row_size * kHeight;
    const int file_size = 54 + pixel_data_size;

    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "failed to open %s for writing\n", path);
        return;
    }

    uint8_t header[54] = {0};
    header[0] = 'B';
    header[1] = 'M';
    *reinterpret_cast<uint32_t*>(&header[2]) = file_size;
    *reinterpret_cast<uint32_t*>(&header[10]) = 54; // pixel data offset
    *reinterpret_cast<uint32_t*>(&header[14]) = 40; // DIB header size
    *reinterpret_cast<int32_t*>(&header[18]) = kWidth;
    *reinterpret_cast<int32_t*>(&header[22]) = kHeight;
    *reinterpret_cast<uint16_t*>(&header[26]) = 1;  // planes
    *reinterpret_cast<uint16_t*>(&header[28]) = 24; // bpp
    *reinterpret_cast<uint32_t*>(&header[34]) = pixel_data_size;
    std::fwrite(header, 1, 54, f);

    std::vector<uint8_t> row(row_size, 0);
    for (int y = kHeight - 1; y >= 0; --y) { // BMP stores rows bottom-up
        for (int x = 0; x < kWidth; ++x) {
            uint16_t px = g_framebuffer[y * kWidth + x];
            uint8_t r = ((px >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((px >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (px & 0x1F) * 255 / 31;
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        std::fwrite(row.data(), 1, row_size, f);
    }
    std::fclose(f);
    std::printf("wrote %s\n", path);
}

// 40 ticks * 16ms = 640ms — comfortably past screen_status.cpp's 500ms refresh timer
// interval, so chargepoint/global data actually gets applied before capture instead of
// showing each widget's just-created default text.
void Settle(int ticks = 40) {
    for (int i = 0; i < ticks; ++i) {
        lv_tick_inc(16);
        lv_timer_handler();
    }
}

void SeedFakeState() {
    openwb::GlobalStatus global{};
    global.valid = true;
    global.pv_power_w = 3240;
    global.grid_power_w = -850; // negative = feeding into the grid
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
    // state_str deliberately left empty here to exercise the locally-derived fallback
    // text (RefreshChargepointCard) rather than only ever testing the server-message path.
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
    cp1.plugged = false;
    cp1.charging = false;
    // Confirmed verbatim from the user's live openWB instance (state_str for an unplugged
    // chargepoint) — exercises the real server-message path, not the fallback.
    std::strncpy(cp1.state_str, "Keine Ladung, da kein Auto angesteckt ist.", sizeof(cp1.state_str) - 1);
    std::strncpy(cp1.chargemode, "stop", sizeof(cp1.chargemode) - 1);
    std::strncpy(cp1.name, "MQTT-Ladepunkt 4", sizeof(cp1.name) - 1);
    state::State().SetChargepointStatus(1, cp1);
}

} // namespace

int main() {
    lv_init();

    lv_display_t* display = lv_display_create(kWidth, kHeight);
    lv_display_set_buffers(display, g_framebuffer, nullptr, sizeof(g_framebuffer),
                            LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, FlushCb);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    SeedFakeState();

    const std::string out_dir = "src/sim/out";
    std::string cmd = "mkdir -p " + out_dir;
    std::system(cmd.c_str());

    // Light theme.
    theme::ApplyQuartzTheme(display, /*dark_mode=*/false);
    screens::ShowBootScreen();
    Settle();
    WriteBmp((out_dir + "/01_boot_light.bmp").c_str());

    screens::ShowProvisioningScreen("openwb-simple-display-setup");
    Settle();
    WriteBmp((out_dir + "/02_provisioning_light.bmp").c_str());

    screens::ShowStatusScreen(1);
    Settle();
    WriteBmp((out_dir + "/03_status_single_light.bmp").c_str());

    screens::ShowStatusScreen(2);
    Settle();
    WriteBmp((out_dir + "/04_status_multi_light.bmp").c_str());

    // No on-device settings screen (settings are web-UI-only, reached by the device's
    // IP, per the user's preference) — the only on-device prompt left is this WiFi-reset
    // confirm, triggered by a long-press on the status screen's top panel.
    ui::ShowWifiResetPrompt();
    Settle();
    WriteBmp((out_dir + "/05_wifi_reset_prompt_light.bmp").c_str());

    // Dark theme — same screens again so both can be compared. Widgets that read
    // AppConfig::dark_mode directly for manually-set colors (mode_switch's track,
    // badges, ...) need this kept in sync with the theme, exactly as lvgl_port.cpp
    // does in the real app — forgetting this is what caused the mode-switch track to
    // render in the light palette against an otherwise-dark screen on the first pass.
    config::Store().Update([](config::AppConfig& c) { c.dark_mode = true; });
    theme::ApplyQuartzTheme(display, /*dark_mode=*/true);
    screens::ShowStatusScreen(1);
    Settle();
    WriteBmp((out_dir + "/06_status_single_dark.bmp").c_str());

    screens::ShowStatusScreen(2);
    Settle();
    WriteBmp((out_dir + "/07_status_multi_dark.bmp").c_str());

    return 0;
}
