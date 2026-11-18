#include "pin_prompt.h"

#include <cstring>

#include <lvgl.h>

#include "config/config_store.h"
#include "ui/theme/fonts/fonts.h"
#include "ui/theme/theme_colors.h"
#include "ui/theme/theme_quartz.h"

namespace ui {
namespace {

constexpr size_t kMaxPinLen = 16; // matches AppConfig::web_ui_pin[17]

// Plain text, not LV_SYMBOL_BACKSPACE — the project's generated fonts (see
// ui/theme/fonts/fonts.h) only cover Latin-1 for German umlauts, not LVGL's built-in
// symbol private-use-area glyphs, so that symbol would render as a tofu box.
constexpr const char* kBackspaceLabel = "DEL";
constexpr const char* kKeypadMap[] = {
    "1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", kBackspaceLabel, "0", "OK", "",
};

struct PinPromptState {
    char entered[kMaxPinLen + 1] = "";
    size_t len = 0;
    lv_obj_t* mbox = nullptr;
    lv_obj_t* dots_label = nullptr;
    lv_obj_t* error_label = nullptr;
    PinSuccessCb on_success = nullptr;
    void* user_data = nullptr;
};

void UpdateDots(PinPromptState* state) {
    char buf[kMaxPinLen + 1];
    for (size_t i = 0; i < state->len; ++i) buf[i] = '*';
    buf[state->len] = '\0';
    lv_label_set_text(state->dots_label, state->len > 0 ? buf : " ");
}

void OnKeypadPressed(lv_event_t* e) {
    auto* state = static_cast<PinPromptState*>(lv_event_get_user_data(e));
    lv_obj_t* matrix = static_cast<lv_obj_t*>(lv_event_get_target(e));
    uint32_t id = lv_buttonmatrix_get_selected_button(matrix);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) return;
    const char* txt = lv_buttonmatrix_get_button_text(matrix, id);
    if (txt == nullptr) return;

    if (strcmp(txt, kBackspaceLabel) == 0) {
        if (state->len > 0) state->entered[--state->len] = '\0';
        lv_obj_add_flag(state->error_label, LV_OBJ_FLAG_HIDDEN);
        UpdateDots(state);
        return;
    }

    if (strcmp(txt, "OK") == 0) {
        const config::AppConfig& cfg = config::Store().Get();
        if (strcmp(state->entered, cfg.web_ui_pin) == 0) {
            PinSuccessCb on_success = state->on_success;
            void* user_data = state->user_data;
            lv_obj_t* mbox = state->mbox;
            lv_msgbox_close(mbox); // fires LV_EVENT_DELETE, which frees `state`
            on_success(user_data);
        } else {
            state->len = 0;
            state->entered[0] = '\0';
            UpdateDots(state);
            lv_obj_remove_flag(state->error_label, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (state->len < kMaxPinLen) {
        state->entered[state->len++] = txt[0];
        state->entered[state->len] = '\0';
        lv_obj_add_flag(state->error_label, LV_OBJ_FLAG_HIDDEN);
        UpdateDots(state);
    }
}

void OnCancel(lv_event_t* e) {
    auto* state = static_cast<PinPromptState*>(lv_event_get_user_data(e));
    lv_msgbox_close(state->mbox);
}

void OnPromptDeleted(lv_event_t* e) {
    delete static_cast<PinPromptState*>(lv_event_get_user_data(e));
}

} // namespace

void RequirePin(PinSuccessCb on_success, void* user_data) {
    const config::AppConfig& cfg = config::Store().Get();
    if (cfg.web_ui_pin[0] == '\0') {
        on_success(user_data);
        return;
    }

    auto* state = new PinPromptState();
    state->on_success = on_success;
    state->user_data = user_data;

    lv_obj_t* mbox = lv_msgbox_create(nullptr);
    state->mbox = mbox;
    lv_obj_set_width(mbox, lv_pct(88));

    lv_obj_t* title = lv_msgbox_add_title(mbox, "PIN eingeben");
    theme::ApplyMsgboxFont(title);
    lv_msgbox_add_close_button(mbox);

    lv_obj_t* content = lv_msgbox_get_content(mbox);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    state->dots_label = lv_label_create(content);
    lv_obj_set_style_text_font(state->dots_label, &font_sans_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(state->dots_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(state->dots_label, lv_pct(100));
    lv_label_set_text(state->dots_label, " ");

    state->error_label = lv_label_create(content);
    lv_label_set_text(state->error_label, "Falscher PIN");
    theme::ApplyMsgboxFont(state->error_label);
    lv_obj_set_style_text_color(state->error_label, theme::Colors(cfg.dark_mode).error, LV_PART_MAIN);
    lv_obj_add_flag(state->error_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* matrix = lv_buttonmatrix_create(content);
    // Same lv_buttonmatrix SCROLLABLE gotcha as the chargepoint popup's limit-type matrix
    // (see screen_chargepoint.cpp) — without this, a jittery tap can be swallowed as a
    // self-scroll attempt instead of registering the digit.
    lv_obj_remove_flag(matrix, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(matrix, lv_pct(100), 180);
    lv_obj_set_style_text_font(matrix, &font_sans_20, LV_PART_ITEMS);
    lv_buttonmatrix_set_map(matrix, kKeypadMap);
    lv_obj_add_event_cb(matrix, OnKeypadPressed, LV_EVENT_VALUE_CHANGED, state);

    lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(mbox, "Abbrechen");
    theme::ApplyMsgboxFont(cancel_btn);
    lv_obj_add_event_cb(cancel_btn, OnCancel, LV_EVENT_CLICKED, state);

    lv_obj_add_event_cb(mbox, OnPromptDeleted, LV_EVENT_DELETE, state);
}

} // namespace ui
