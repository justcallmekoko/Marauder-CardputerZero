/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "wifi_scan_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {

WifiScanScreen::WifiScanScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void WifiScanScreen::build_content(lv_obj_t* content) {
    reactive::observe_obj(content, view_model().wifi_scan_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        auto* screen = static_cast<WifiScanScreen*>(lv_observer_get_user_data(observer));
        screen->set_key_guide(screen->view_model().wifi_details_open() ?
            "F/X  Item    Enter  Rescan    D  Summary    Esc  Back" :
            "F/X  Page    Enter  Rescan    D  Details    Esc  Back");
    }, this);
    auto* panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_hor(panel, 8, 0);
    lv_obj_set_style_pad_ver(panel, 3, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_ACTIVE);

    auto* results = lv_label_create(panel);
    lv_obj_set_width(results, LV_PCT(100));
    lv_label_set_long_mode(results, LV_LABEL_LONG_WRAP);
    lv_label_bind_text(results, view_model().wifi_scan_subject(), nullptr);
    auto* body_font = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(results, body_font ? body_font : &lv_font_montserrat_10, 0);
    reactive::bind_theme(results, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
