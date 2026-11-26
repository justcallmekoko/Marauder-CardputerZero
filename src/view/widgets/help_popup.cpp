/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "help_popup.h"

#include "asset_manager.h"
#include "base_viewmodel.h"
#include "theme.h"

#include <array>
#include <cstddef>

namespace view::widgets {
namespace {

struct HelpRow {
    const char* key;
    const char* action;
};

constexpr std::array<HelpRow, 5> kDashboardRows = {{
    {"F / X", "Move up / down"},
    {"Z / C", "Move left / right"},
    {"ENTER", "Open selected tile"},
    {"HELP", "Context help"},
    {"ESC", "Back / exit at root"},
}};

constexpr std::array<HelpRow, 5> kDeviceRows = {{
    {"F / X", "Browse settings"},
    {"ENTER", "Open or change highlighted item"},
    {"ESC", "Back one menu"},
    {"HELP", "Close help"},
    {"", ""},
}};

constexpr std::array<HelpRow, 5> kRadioRows = {{
    {"F / X", "Browse devices"},
    {"ENTER", "Toggle selected device"},
    {"D", "Refresh inventory"},
    {"ESC", "Return to menu"},
    {"HELP", "Context help"},
}};

constexpr std::array<HelpRow, 5> kAdapterRows = {{
    {"Z / ESC", "Back to originating menu"},
    {"F / X", "Previous / next adapter"},
    {"C / ENTER", "Select adapter"},
    {"D", "More / summary"},
    {"M", "Toggle selected monitor mode"},
}};

constexpr std::array<HelpRow, 5> kWifiScanRows = {{
    {"Z / ESC", "Back to originating menu"},
    {"F / X", "Previous / next result"},
    {"C / ENTER", "Open AP, station, or Fox hunt"},
    {"D", "Restart / AP security pages"},
    {"HELP", "Context help"},
}};
constexpr std::array<HelpRow, 5> kRawCaptureRows = {{
    {"Z / C", "Previous / next supported channel"},
    {"H", "Toggle automatic channel hopping"},
    {"B", "Build AP and station lists"},
    {"T", "Frame-type filters"},
    {"ENTER", "Show observed access points"},
}};
constexpr std::array<HelpRow, 5> kRemoteIdRows = {{
    {"F / X", "Select aircraft, action, or tab"},
    {"ENTER", "Open aircraft actions"},
    {"ESC", "Back to actions or map"},
    {"P", "Start / stop PCAP"},
    {"HELP", "Close help"},
}};

constexpr std::array<HelpRow, 5> kTrafficRows = {{
    {"Z / C", "Tune monitor channel"}, {"ESC", "Back to Activity"}, {"HELP", "Close help"}, {"", ""}, {"", ""}
}};

constexpr std::array<HelpRow, 5> kDiagnosticsRows = {{
    {"Z / ESC", "Back to Device menu"},
    {"F / X", "Previous / next page"},
    {"C / ENTER", "Refresh diagnostics"},
    {"D", "Details / summary"},
    {"HELP", "Context help"},
}};

constexpr std::array<HelpRow, 5> kPcapRows = {{
    {"F / X", "Browse captures"},
    {"ENTER", "File information"},
    {"D", "Refresh file list"},
    {"P", "Start / stop PCAP"},
    {"ESC", "Back to Device"},
}};

} // namespace

HelpPopup::HelpPopup(lv_obj_t* parent,
                     viewmodel::BaseViewModel& view_model,
                     app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

void HelpPopup::build() {
    if (core_obj_ || !parent_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(core_obj_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(core_obj_, LV_OPA_50, 0);
    lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(core_obj_, backdrop_clicked_cb, LV_EVENT_CLICKED, this);

    panel_ = lv_obj_create(core_obj_);
    lv_obj_remove_style_all(panel_);
    lv_obj_set_size(panel_, 292, 146);
    lv_obj_center(panel_);
    lv_obj_set_style_radius(panel_, 6, 0);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel_, 1, 0);
    lv_obj_set_style_shadow_width(panel_, 14, 0);
    lv_obj_set_style_shadow_opa(panel_, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(panel_, 0, 0);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_CLICKABLE);

    title_ = lv_label_create(panel_);
    lv_label_set_text(title_, "Keyboard help");
    auto* title_font = assets_.load_standard_font(15);
    lv_obj_set_style_text_font(title_, title_font ? title_font : &lv_font_montserrat_14, 0);
    lv_obj_align(title_, LV_ALIGN_TOP_LEFT, 12, 9);

    page_badge_ = lv_label_create(panel_);
    auto* badge_font = assets_.load_standard_font(11);
    lv_obj_set_style_text_font(page_badge_, badge_font ? badge_font : &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(page_badge_, 3, 0);
    lv_obj_set_style_pad_hor(page_badge_, 6, 0);
    lv_obj_set_style_pad_ver(page_badge_, 2, 0);
    lv_obj_align(page_badge_, LV_ALIGN_TOP_RIGHT, -10, 7);

    auto* rows = lv_obj_create(panel_);
    lv_obj_remove_style_all(rows);
    lv_obj_set_size(rows, 268, 88);
    lv_obj_align(rows, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rows, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(rows, 1, 0);
    lv_obj_clear_flag(rows, LV_OBJ_FLAG_SCROLLABLE);

    auto* key_font = assets_.load_standard_font(11);
    auto* action_font = assets_.load_standard_font(12, app::StandardFontWeight::Regular);
    for (auto& widgets : rows_) {
        widgets.row = lv_obj_create(rows);
        lv_obj_remove_style_all(widgets.row);
        lv_obj_set_size(widgets.row, LV_PCT(100), 16);
        lv_obj_clear_flag(widgets.row, LV_OBJ_FLAG_SCROLLABLE);

        widgets.key = lv_label_create(widgets.row);
        lv_obj_set_width(widgets.key, 82);
        lv_obj_set_style_text_font(widgets.key, key_font ? key_font : &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(widgets.key, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(widgets.key, LV_ALIGN_LEFT_MID, 0, 0);

        widgets.action = lv_label_create(widgets.row);
        lv_obj_set_width(widgets.action, 176);
        lv_obj_set_style_text_font(widgets.action,
                                   action_font ? action_font : &lv_font_montserrat_12,
                                   0);
        lv_obj_align(widgets.action, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    footer_ = lv_label_create(panel_);
    lv_label_set_text(footer_, "ESC Close   P PCAP   PRSC Screenshot");
    lv_label_set_long_mode(footer_, LV_LABEL_LONG_CLIP);
    auto* footer_font = assets_.load_standard_font(10);
    lv_obj_set_style_text_font(footer_, footer_font ? footer_font : &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(footer_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_size(footer_, LV_SIZE_CONTENT, 12);
    lv_obj_align(footer_, LV_ALIGN_BOTTOM_LEFT, 12, -7);

    lv_subject_add_observer_obj(view_model_.dark_mode_subject(),
                                theme_observer_cb,
                                core_obj_,
                                this);
    apply_theme();
    hide();
}

void HelpPopup::show(model::AppPage page) {
    if (!core_obj_) {
        build();
    }
    if (!core_obj_) {
        return;
    }
    update_rows(page);
    apply_theme();
    lv_obj_move_foreground(core_obj_);
    lv_obj_remove_flag(core_obj_, LV_OBJ_FLAG_HIDDEN);
}

void HelpPopup::hide() {
    if (core_obj_ && lv_obj_is_valid(core_obj_)) {
        lv_obj_add_flag(core_obj_, LV_OBJ_FLAG_HIDDEN);
    }
}

bool HelpPopup::visible() const {
    return core_obj_ && lv_obj_is_valid(core_obj_) &&
           !lv_obj_has_flag(core_obj_, LV_OBJ_FLAG_HIDDEN);
}

void HelpPopup::update_rows(model::AppPage page) {
    const bool list = page == model::AppPage::Dashboard && view_model_.menu_columns() == 1;
    const bool radios = page == model::AppPage::RadioSetup;
    const bool adapters = page == model::AppPage::Adapters;
    const bool traffic = page == model::AppPage::Traffic;
    const bool wifi_scan = page == model::AppPage::WifiScan;
    const bool raw_capture = wifi_scan && view_model_.raw_session();
    const bool diagnostics = page == model::AppPage::Diagnostics;
    const bool pcap = page == model::AppPage::PcapFiles;
    const bool remote = page == model::AppPage::RemoteId;
    lv_label_set_text(page_badge_, remote ? "REMOTE ID" : pcap ? "CAPTURES" : traffic ? "ACTIVITY" : list ? "DEVICE" : radios ? "RADIO SETUP" : adapters ? "ADAPTERS" : raw_capture ? "RAW CAPTURE" : wifi_scan ? "WIFI SCAN" : diagnostics ? "SYSTEM INFO" : "MENU");
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        lv_obj_remove_flag(rows_[i].row, LV_OBJ_FLAG_HIDDEN);
        const auto& row = remote ? kRemoteIdRows[i] : pcap ? kPcapRows[i] : traffic ? kTrafficRows[i] : list ? kDeviceRows[i] : radios ? kRadioRows[i] : adapters ? kAdapterRows[i] : raw_capture ? kRawCaptureRows[i] : wifi_scan ? kWifiScanRows[i] : diagnostics ? kDiagnosticsRows[i] : kDashboardRows[i];
        lv_label_set_text(rows_[i].key, row.key);
        lv_label_set_text(rows_[i].action, row.action);
        if (list && !*row.key) lv_obj_add_flag(rows_[i].row, LV_OBJ_FLAG_HIDDEN);
    }
}

void HelpPopup::apply_theme() {
    if (!panel_) {
        return;
    }
    const auto colors = view::palette(view_model_.is_dark_mode());
    lv_obj_set_style_bg_color(panel_, colors.surface, 0);
    lv_obj_set_style_border_color(panel_, colors.border, 0);
    lv_obj_set_style_shadow_color(panel_, lv_color_black(), 0);
    lv_obj_set_style_text_color(title_, colors.text, 0);
    lv_obj_set_style_bg_color(page_badge_, colors.primary, 0);
    lv_obj_set_style_bg_opa(page_badge_, LV_OPA_20, 0);
    lv_obj_set_style_text_color(page_badge_, colors.primary, 0);
    lv_obj_set_style_text_color(footer_, colors.text_disabled, 0);
    for (auto& row : rows_) {
        lv_obj_set_style_text_color(row.key, colors.primary, 0);
        lv_obj_set_style_text_color(row.action, colors.text, 0);
    }
}

void HelpPopup::backdrop_clicked_cb(lv_event_t* event) {
    auto* popup = static_cast<HelpPopup*>(lv_event_get_user_data(event));
    if (popup && lv_event_get_target(event) == popup->core_obj_) {
        popup->hide();
    }
}

void HelpPopup::theme_observer_cb(lv_observer_t* observer, lv_subject_t*) {
    auto* popup = static_cast<HelpPopup*>(lv_observer_get_user_data(observer));
    if (popup) {
        popup->apply_theme();
    }
}

} // namespace view::widgets
