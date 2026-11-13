// Headless LVGL screenshot tool: renders each real screen into an off-screen
// framebuffer and dumps it to src/sim/out/*.bmp (gitignored, regenerate on demand).
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <lvgl.h>

#include "config/config_store.h"
#include "fake_state.h"
#include "ui/screens/screen_boot.h"
#include "ui/screens/screen_provisioning.h"
#include "ui/screens/screen_settings.h"
#include "ui/screens/screen_status.h"
#include "ui/theme/theme_quartz.h"
#include "ui/wifi_reset_prompt.h"

namespace {

constexpr int32_t kWidth = 480;
constexpr int32_t kHeight = 480;

uint16_t g_framebuffer[kWidth * kHeight]; // RGB565, matches lv_conf.h's LV_COLOR_DEPTH

void FlushCb(lv_display_t* disp, const lv_area_t* /*area*/, uint8_t* /*px_map*/) {
    // DIRECT render mode writes straight into g_framebuffer; nothing to copy here.
    lv_display_flush_ready(disp);
}

// 24bpp BMP from the RGB565 framebuffer — no third-party dependency to decode it.
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

// 640ms, past screen_status.cpp's 500ms refresh interval, so real data is applied
// before capture instead of each widget's just-created placeholder text.
void Settle(int ticks = 40) {
    for (int i = 0; i < ticks; ++i) {
        lv_tick_inc(16);
        lv_timer_handler();
    }
}

} // namespace

int main() {
    lv_init();

    lv_display_t* display = lv_display_create(kWidth, kHeight);
    lv_display_set_buffers(display, g_framebuffer, nullptr, sizeof(g_framebuffer),
                            LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, FlushCb);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    sim::SeedFakeState();

    const std::string out_dir = "src/sim/out";
    std::string cmd = "mkdir -p " + out_dir;
    std::system(cmd.c_str());

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

    screens::ShowSettingsScreen();
    Settle();
    WriteBmp((out_dir + "/05_settings_light.bmp").c_str());

    screens::ShowStatusScreen(1);
    Settle();
    ui::ShowWifiResetPrompt();
    Settle();
    WriteBmp((out_dir + "/06_wifi_reset_prompt_light.bmp").c_str());
    // A msgbox created with a null parent attaches to LVGL's top layer, which survives
    // lv_screen_load() — without this it'd still show up in every screenshot below.
    lv_obj_clean(lv_layer_top());

    // Keep dark_mode in sync with the theme — some widgets read it directly.
    config::Store().Update([](config::AppConfig& c) { c.dark_mode = true; });
    theme::ApplyQuartzTheme(display, /*dark_mode=*/true);
    screens::ShowStatusScreen(1);
    Settle();
    WriteBmp((out_dir + "/07_status_single_dark.bmp").c_str());

    screens::ShowStatusScreen(2);
    Settle();
    WriteBmp((out_dir + "/08_status_multi_dark.bmp").c_str());

    return 0;
}
