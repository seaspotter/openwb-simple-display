// Thin strip at the top of screen_status: brand label, centered clock, connectivity
// icon, update/fault icons (shown only when relevant), and a settings-gear icon.
#pragma once

#include <lvgl.h>

namespace widgets {

lv_obj_t* CreateStatusBar(lv_obj_t* parent);

} // namespace widgets
