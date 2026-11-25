/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace model {
namespace {
std::filesystem::path preference_path() {
    std::filesystem::path root;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) root = xdg;
    else if (const char* home = std::getenv("HOME"); home && *home) root = std::filesystem::path(home) / ".config";
    else if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) root = appdata;
    if (root.empty()) return {};
    return root / "marauder-cardputerzero" / "battery-display";
}
}

const char* battery_display_name(BatteryDisplay mode) {
    switch (mode) {
        case BatteryDisplay::Icon: return "Icon";
        case BatteryDisplay::Percent: return "Percentage";
        default: return "Both";
    }
}

BaseModel::BaseModel() {
    std::ifstream input(preference_path());
    std::string value;
    if (input >> value) {
        if (value == "Icon") battery_display_ = BatteryDisplay::Icon;
        else if (value == "Percentage") battery_display_ = BatteryDisplay::Percent;
    }
}

void BaseModel::cycle_battery_display() {
    switch (battery_display_) {
        case BatteryDisplay::Both: battery_display_ = BatteryDisplay::Icon; break;
        case BatteryDisplay::Icon: battery_display_ = BatteryDisplay::Percent; break;
        case BatteryDisplay::Percent: battery_display_ = BatteryDisplay::Both; break;
    }
    const auto path = preference_path();
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;
    std::ofstream file(path, std::ios::trunc);
    file << battery_display_name(battery_display_) << '\n';
}


const char* BaseModel::app_title() const { return "Marauder"; }
bool BaseModel::dark_mode() const { return true; }
AppPage BaseModel::current_page() const { return current_page_; }
void BaseModel::set_current_page(AppPage page) { current_page_ = page; }
void BaseModel::toggle_page() {
    switch (current_page_) {
        case AppPage::Dashboard:
            current_page_ = AppPage::Adapters;
            break;
        case AppPage::Adapters:
            current_page_ = AppPage::WifiScan;
            break;
        case AppPage::WifiScan:
            current_page_ = AppPage::Diagnostics;
            break;
        case AppPage::Diagnostics:
            current_page_ = AppPage::Dashboard;
            break;
    }
}
const std::string& BaseModel::selected_adapter_id() const { return selected_adapter_id_; }
void BaseModel::set_selected_adapter_id(std::string stable_id) {
    selected_adapter_id_ = std::move(stable_id);
}

} // namespace model
