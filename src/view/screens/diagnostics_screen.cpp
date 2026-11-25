/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "diagnostics_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {

DiagnosticsScreen::DiagnosticsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

DiagnosticsScreen::~DiagnosticsScreen() {
    if (diagnostics_observer_) lv_observer_remove(diagnostics_observer_);
}

void DiagnosticsScreen::build_content(lv_obj_t* content) {
    scroll_ = lv_obj_create(content);
    lv_obj_remove_style_all(scroll_);
    lv_obj_set_size(scroll_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_hor(scroll_, 8, 0);
    lv_obj_set_style_pad_ver(scroll_, 4, 0);
    lv_obj_set_scroll_dir(scroll_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_, LV_SCROLLBAR_MODE_ACTIVE);

    auto* details = lv_label_create(scroll_);
    lv_obj_set_width(details, LV_PCT(100));
    lv_label_set_long_mode(details, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(details, view_model().diagnostics_detail_subject(), nullptr);
    auto* body_font = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(details, body_font ? body_font : &lv_font_montserrat_10, 0);
    reactive::bind_theme(details, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    diagnostics_observer_ = reactive::observe_obj(scroll_,
                                                  view_model().diagnostics_generation_subject(),
                                                  diagnostics_changed_cb,
                                                  this);
}

void DiagnosticsScreen::diagnostics_changed_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* screen = static_cast<DiagnosticsScreen*>(lv_observer_get_user_data(observer));
    if (!screen || !screen->scroll_) return;
    lv_obj_scroll_to_y(screen->scroll_, 0, LV_ANIM_OFF);
    screen->set_key_guide(screen->view_model().system_details_open() ?
        "F/X  Page    Enter  Refresh    D  Less    Esc  Back" :
        "F/X  Page    Enter  Refresh    D  More    Esc  Back");
}

} // namespace screen
