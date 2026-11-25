/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

#include <stdexcept>

namespace model {
namespace {

using Action = MarauderMenuAction;
using Color = MarauderMenuColor;
using Id = MarauderMenuId;
using Item = MarauderMenuItem;

const std::vector<Item>& items_for(Id menu) {
    static const std::vector<Item> main = {
        {"WiFi", Color::Green, Action::None, Id::Wifi, true, true},
        {"Bluetooth", Color::Cyan, Action::None, Id::Bluetooth, true, true},
        {"GPS", Color::Red, Action::None, Id::Gps, true, true},
        {"Device", Color::Blue, Action::None, Id::Device, true, true},
        {"Settings", Color::LightGrey, Action::None, Id::Settings, true, true},
        {"Radios", Color::Purple, Action::OpenAdapters, Id::Main, false, true},
    };
    static const std::vector<Item> wifi = {
        {"Scanners", Color::Orange, Action::None, Id::WifiScanners, true, true},
        {"Sniffers", Color::Yellow, Action::None, Id::WifiSniffers, true, true},
        {"Attacks", Color::Red, Action::None, Id::WifiAttacks, true, true},
        {"General", Color::Purple, Action::None, Id::WifiGeneral, true, true},
    };
    static const std::vector<Item> wifi_scanners = {
        {"Access Points", Color::Orange, Action::OpenWifiScan, Id::Main, false, true},
        {"Stations", Color::Cyan, Action::None, Id::Main, false, false},
        {"Packet Monitor", Color::Blue, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_sniffers = {
        {"Probe Requests", Color::Cyan, Action::None, Id::Main, false, false},
        {"Deauth", Color::Red, Action::None, Id::Main, false, false},
        {"Pwnagotchi", Color::Orange, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_attacks = {
        {"Deauth", Color::Red, Action::None, Id::Main, false, false},
        {"Beacon Spam", Color::Orange, Action::None, Id::Main, false, false},
        {"Evil Portal", Color::Magenta, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_general = {
        {"Adapter Setup", Color::Purple, Action::OpenAdapters, Id::Main, false, true},
        {"Scan Networks", Color::Green, Action::OpenWifiScan, Id::Main, false, true},
        {"Saved Networks", Color::LightGrey, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> bluetooth = {
        {"Scanners", Color::Cyan, Action::None, Id::Main, false, false},
        {"Sniffers", Color::Magenta, Action::None, Id::Main, false, false},
        {"General", Color::LightGrey, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> gps = {
        {"GPS Info", Color::Red, Action::None, Id::Main, false, false},
        {"Wardrive", Color::Green, Action::None, Id::Main, false, false},
        {"Track", Color::Orange, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> device = {
        {"Adapter Setup", Color::Blue, Action::OpenAdapters, Id::Main, false, true},
        {"System Info", Color::Cyan, Action::OpenDiagnostics, Id::Main, false, true},
        {"Storage", Color::LightGrey, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> settings = {
        {"Battery", Color::Green, Action::CycleBatteryDisplay, Id::Main, false, true},
        {"Adapter Setup", Color::Green, Action::OpenAdapters, Id::Main, false, true},
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
        case Id::Main: return "MARAUDER";
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
    return "MARAUDER";
}

} // namespace

MarauderMenuModel::MarauderMenuModel() : stack_{{MarauderMenuId::Main, 0}} {}
const char* MarauderMenuModel::title() const { return title_for(current_menu()); }
MarauderMenuId MarauderMenuModel::current_menu() const { return stack_.back().menu; }
std::size_t MarauderMenuModel::depth() const { return stack_.size(); }
bool MarauderMenuModel::can_go_back() const { return stack_.size() > 1; }
std::size_t MarauderMenuModel::selected_index() const { return stack_.back().selected; }
std::size_t MarauderMenuModel::item_count() const { return items_for(current_menu()).size(); }
const MarauderMenuItem& MarauderMenuModel::item(std::size_t index) const {
    const auto& items = items_for(current_menu());
    if (index >= items.size()) throw std::out_of_range("Marauder menu item");
    return items[index];
}
std::size_t MarauderMenuModel::columns() const {
    return current_menu() == MarauderMenuId::Main ? 3 : 2;
}

void MarauderMenuModel::move(int dx, int dy) {
    const int width = static_cast<int>(columns());
    const int count = static_cast<int>(item_count());
    const int current = static_cast<int>(selected_index());
    const int row = current / width + dy;
    const int col = current % width + dx;
    if (row < 0 || row * width >= count || col < 0 || col >= width) return;
    const int next = row * width + col;
    if (next < count) stack_.back().selected = next;
    else if (dy != 0) stack_.back().selected = count - 1;
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
