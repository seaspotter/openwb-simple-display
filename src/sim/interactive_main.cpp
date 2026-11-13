// Interactive LVGL desktop simulator — a real window via LVGL's SDL driver, mouse
// acting as the touchscreen. Needs libsdl2-dev installed.
#include <cstdio>
#include <cstring>

#include <lvgl.h>

#include <SDL2/SDL.h>

#include "fake_state.h"
#include "ui/screens/screen_status.h"
#include "ui/theme/theme_quartz.h"
#include "ui/ui_events.h"

namespace {

constexpr int32_t kWidth = 480;
constexpr int32_t kHeight = 480;

} // namespace

int main(int argc, char** argv) {
    bool multi = argc > 1 && std::strcmp(argv[1], "--multi") == 0;

    lv_init();

    lv_display_t* display = lv_sdl_window_create(kWidth, kHeight);
    lv_sdl_window_set_title(display, "openwb-simple-display (sim)");
    lv_indev_t* mouse = lv_sdl_mouse_create();
    (void)mouse;

    sim::SeedFakeState();
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
