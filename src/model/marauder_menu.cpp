/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

#include <stdexcept>

namespace model {
namespace {

using Action = MarauderMenuAction;
using Id = MarauderMenuId;
using Item = MarauderMenuItem;

const std::vector<Item>& items_for(Id menu) {
    static const std::vector<Item> main = {
        {"WiFi", Action::None, Id::Wifi, true, true},
        {"Bluetooth", Action::None, Id::Bluetooth, true, true},
        {"GPS", Action::None, Id::Gps, true, true},
        {"Device", Action::None, Id::Device, true, true},
        {"Settings", Action::None, Id::Settings, true, true},
    };
    static const std::vector<Item> wifi = {
        {"Scanners", Action::None, Id::WifiScanners, true, true},
        {"Sniffers", Action::None, Id::WifiSniffers, true, true},
        {"Attacks", Action::None, Id::WifiAttacks, true, true},
        {"General", Action::None, Id::WifiGeneral, true, true},
    };
    static const std::vector<Item> wifi_scanners = {
        {"Access Points", Action::OpenWifiScan, Id::Main, false, true},
        {"Stations", Action::None, Id::Main, false, false},
        {"Packet Monitor", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_sniffers = {
        {"Probe Requests", Action::None, Id::Main, false, false},
        {"Deauth", Action::None, Id::Main, false, false},
        {"Pwnagotchi", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_attacks = {
        {"Deauth", Action::None, Id::Main, false, false},
        {"Beacon Spam", Action::None, Id::Main, false, false},
        {"Evil Portal", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_general = {
        {"Adapter Setup", Action::OpenAdapters, Id::Main, false, true},
        {"Scan Networks", Action::OpenWifiScan, Id::Main, false, true},
        {"Saved Networks", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> bluetooth = {
        {"Scanners", Action::None, Id::Main, false, false},
        {"Sniffers", Action::None, Id::Main, false, false},
        {"General", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> gps = {
        {"GPS Info", Action::None, Id::Main, false, false},
        {"Wardrive", Action::None, Id::Main, false, false},
        {"Track", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> device = {
        {"Adapter Setup", Action::OpenAdapters, Id::Main, false, true},
        {"System Info", Action::OpenDiagnostics, Id::Main, false, true},
        {"Storage", Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> settings = {
        {"Light / Dark", Action::ToggleTheme, Id::Main, false, true},
        {"Adapter Setup", Action::OpenAdapters, Id::Main, false, true},
        {"System Info", Action::OpenDiagnostics, Id::Main, false, true},
    };

    switch (menu) {
        case Id::Main: return main;
        case Id::Wifi: return wifi;
        case Id::WifiScanners: return wifi_scanners;
        case Id::WifiSniffers: return wifi_sniffers;
        case Id::WifiAttacks: return wifi_attacks;
        case Id::WifiGeneral: return wifi_general;
        case Id::Bluetooth: return bluetooth;
        case Id::Gps: return gps;
        case Id::Device: return device;
        case Id::Settings: return settings;
    }
    return main;
}

const char* title_for(Id menu) {
    switch (menu) {
        case Id::Main: return "ESP32 MARAUDER";
        case Id::Wifi: return "WiFi";
        case Id::WifiScanners: return "WiFi > Scanners";
        case Id::WifiSniffers: return "WiFi > Sniffers";
        case Id::WifiAttacks: return "WiFi > Attacks";
        case Id::WifiGeneral: return "WiFi > General";
        case Id::Bluetooth: return "Bluetooth";
        case Id::Gps: return "GPS";
        case Id::Device: return "Device";
        case Id::Settings: return "Settings";
    }
    return "ESP32 MARAUDER";
}

} // namespace

MarauderMenuModel::MarauderMenuModel() : stack_{{MarauderMenuId::Main, 0}} {}
const char* MarauderMenuModel::title() const { return title_for(current_menu()); }
MarauderMenuId MarauderMenuModel::current_menu() const { return stack_.back().menu; }
std::size_t MarauderMenuModel::depth() const { return stack_.size(); }
std::size_t MarauderMenuModel::selected_index() const { return stack_.back().selected; }
std::size_t MarauderMenuModel::item_count() const { return items_for(current_menu()).size(); }
const MarauderMenuItem& MarauderMenuModel::item(std::size_t index) const {
    const auto& items = items_for(current_menu());
    if (index >= items.size()) throw std::out_of_range("Marauder menu item");
    return items[index];
}
void MarauderMenuModel::previous() {
    auto& frame = stack_.back();
    const auto count = item_count();
    frame.selected = frame.selected == 0 ? count - 1 : frame.selected - 1;
}
void MarauderMenuModel::next() {
    auto& frame = stack_.back();
    frame.selected = (frame.selected + 1) % item_count();
}
MarauderMenuAction MarauderMenuModel::activate() {
    const auto selected = item(selected_index());
    if (!selected.available) return MarauderMenuAction::None;
    if (selected.opens_submenu) {
        stack_.push_back({selected.submenu, 0});
        return MarauderMenuAction::None;
    }
    return selected.action;
}
bool MarauderMenuModel::back() {
    if (stack_.size() <= 1) return false;
    stack_.pop_back();
    return true;
}

} // namespace model
