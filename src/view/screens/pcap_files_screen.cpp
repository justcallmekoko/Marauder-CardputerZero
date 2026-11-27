// SPDX-License-Identifier: AGPL-3.0-only
#include "pcap_files_screen.h"
#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace screen {
namespace {
std::string size_label(std::uintmax_t bytes) {
    if (bytes >= 1048576) {
        std::ostringstream text;
        text << std::fixed << std::setprecision(1) << static_cast<double>(bytes) / 1048576.0 << " MiB";
        return text.str();
    }
    if (bytes >= 1024) return std::to_string(bytes / 1024) + " KiB";
    return std::to_string(bytes) + " B";
}
}

PcapFilesScreen::PcapFilesScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets)
    : BaseScreen(model, assets) { init(); }

void PcapFilesScreen::build_content(lv_obj_t* content) {
    auto* small = assets().load_standard_font(10);
    auto* regular = assets().load_standard_font(11);
    title_ = lv_label_create(content);
    lv_obj_set_pos(title_, 8, 2);
    lv_obj_set_style_text_font(title_, regular ? regular : LV_FONT_DEFAULT, 0);
    count_ = lv_label_create(content);
    lv_obj_align(count_, LV_ALIGN_TOP_RIGHT, -8, 3);
    lv_obj_set_style_text_font(count_, small ? small : LV_FONT_DEFAULT, 0);
    message_ = lv_label_create(content);
    lv_obj_set_pos(message_, 9, 25);
    lv_obj_set_width(message_, 300);
    lv_label_set_long_mode(message_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(message_, regular ? regular : LV_FONT_DEFAULT, 0);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        row.box = lv_obj_create(content);
        lv_obj_remove_style_all(row.box);
        lv_obj_set_pos(row.box, 6, 21 + static_cast<int>(i) * 25);
        lv_obj_set_size(row.box, 308, 23);
        lv_obj_set_style_radius(row.box, 4, 0);
        lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row.box, 1, 0);
        lv_obj_clear_flag(row.box, LV_OBJ_FLAG_SCROLLABLE);
        row.name = lv_label_create(row.box);
        lv_obj_set_pos(row.name, 7, 1);
        lv_obj_set_width(row.name, 294);
        lv_label_set_long_mode(row.name, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(row.name, regular ? regular : LV_FONT_DEFAULT, 0);
        row.meta = lv_label_create(row.box);
        lv_obj_set_pos(row.meta, 7, 12);
        lv_obj_set_width(row.meta, 294);
        lv_obj_set_style_text_font(row.meta, small ? small : LV_FONT_DEFAULT, 0);
    }
    reactive::observe_obj(content,
        view_model().current_page() == model::AppPage::WardriveFiles ?
            view_model().wardrive_files_subject() : view_model().pcap_files_subject(),
        [](lv_observer_t* observer, lv_subject_t*) {
        static_cast<PcapFilesScreen*>(lv_observer_get_user_data(observer))->render();
    }, this);
}

void PcapFilesScreen::render() {
    const auto colors = view::palette(view_model().is_dark_mode());
    const bool wardrive = view_model().current_page() == model::AppPage::WardriveFiles;
    if (wardrive) {
        const auto& files = view_model().wardrive_files();
        const auto cursor = view_model().wardrive_cursor();
        const bool details = view_model().wardrive_details() && cursor < files.size();
        lv_label_set_text(title_, details ? "Wardrive details" : "Wardrive logs");
        const auto count = std::to_string(files.size()) + " logs";
        lv_label_set_text(count_, count.c_str());
        lv_obj_set_style_text_color(title_, colors.text, 0);
        lv_obj_set_style_text_color(count_, colors.success, 0);
        if (details || files.empty() || view_model().wardrive_uploading()) lv_obj_remove_flag(message_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
        if (details) {
            const auto& item = files[cursor];
            const auto detail = view_model().screen_capture_name(item.name) + "\n" +
                size_label(item.bytes) + "  " + item.modified + "\n" +
                "WiGLE  " + item.wigle + "\nWDGWars  " + item.wdg +
                "\n" + view_model().wardrive_message();
            lv_label_set_text(message_, detail.c_str());
        } else if (files.empty()) lv_label_set_text(message_, "No wardrive logs saved yet.");
        else lv_label_set_text(message_, view_model().wardrive_message().c_str());
        lv_obj_set_style_text_color(message_, colors.text_disabled, 0);
        if (cursor < first_) first_ = cursor;
        if (cursor >= first_ + rows_.size()) first_ = cursor + 1 - rows_.size();
        for (std::size_t i = 0; i < rows_.size(); ++i) {
            auto& row = rows_[i];
            const auto index = first_ + i;
            if (details || view_model().wardrive_uploading() || index >= files.size()) {
                lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue;
            }
            lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
            const bool focus = index == cursor;
            lv_obj_set_style_bg_color(row.box, focus ? colors.button : colors.surface, 0);
            lv_obj_set_style_border_color(row.box, focus ? colors.success : colors.border, 0);
            const auto name = view_model().screen_capture_name(files[index].name);
            lv_label_set_text(row.name, name.c_str());
            const auto meta = files[index].modified + "  " + size_label(files[index].bytes) +
                "  W:" + (files[index].wigle == "Uploaded" ? "OK" : files[index].wigle == "Accepted" ? "WAIT" : files[index].wigle == "Failed" ? "ERR" : "-") +
                " D:" + (files[index].wdg == "Uploaded" ? "OK" : files[index].wdg == "Accepted" ? "WAIT" : files[index].wdg == "Failed" ? "ERR" : "-");
            lv_label_set_text(row.meta, meta.c_str());
            lv_obj_set_style_text_color(row.name, colors.text, 0);
            lv_obj_set_style_text_color(row.meta, focus ? colors.success : colors.text_disabled, 0);
        }
        set_key_guide("F/X Logs  Enter Info  U WiGLE  W WDG  D Delete  Esc Back");
        return;
    }
    const auto& files = view_model().pcap_files();
    const auto cursor = view_model().pcap_cursor();
    const bool details = view_model().pcap_details() && cursor < files.size();
    lv_label_set_text(title_, details ? "Capture details" : "Saved captures");
    const auto count = std::to_string(files.size()) + " files";
    lv_label_set_text(count_, count.c_str());
    lv_obj_set_style_text_color(title_, colors.text, 0);
    lv_obj_set_style_text_color(count_, colors.error, 0);
    if (details || files.empty()) lv_obj_remove_flag(message_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
    if (details) {
        const auto& item = files[cursor];
        const auto text = view_model().screen_capture_name(item.name) + "\n\nSize  " + size_label(item.bytes) +
            "\nSaved  " + item.modified + "\nFolder  ~/Documents/Marauder/pcap";
        lv_label_set_text(message_, text.c_str());
    } else if (files.empty()) {
        lv_label_set_text(message_, "No PCAP captures saved yet.\nSelect a monitor adapter and press P to record.");
    } else {
        const auto message = view_model().screen_capture_name(view_model().pcap_message());
        lv_label_set_text(message_, message.c_str());
    }
    lv_obj_set_style_text_color(message_, colors.text_disabled, 0);
    if (cursor < first_) first_ = cursor;
    if (cursor >= first_ + rows_.size()) first_ = cursor + 1 - rows_.size();
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        const auto index = first_ + i;
        if (details || index >= files.size()) { lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        const bool focus = index == cursor;
        lv_obj_set_style_bg_color(row.box, focus ? colors.button : colors.surface, 0);
        lv_obj_set_style_border_color(row.box, focus ? colors.error : colors.border, 0);
        const auto name = view_model().screen_capture_name(files[index].name);
        lv_label_set_text(row.name, name.c_str());
        const auto meta = files[index].modified + "    " + size_label(files[index].bytes);
        lv_label_set_text(row.meta, meta.c_str());
        lv_obj_set_style_text_color(row.name, colors.text, 0);
        lv_obj_set_style_text_color(row.meta, focus ? colors.error : colors.text_disabled, 0);
    }
    set_key_guide(details ? "D Delete  Esc List  P Capture" : "F/X Files  Enter Info  D Delete  P Rec  Esc Back");
}
} // namespace screen
