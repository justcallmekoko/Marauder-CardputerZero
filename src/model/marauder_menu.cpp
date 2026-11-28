/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "marauder_menu.h"

#include <algorithm>
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
    };
    static const std::vector<Item> wifi = {
        {"Recon", Color::Orange, Action::None, Id::WifiNetworks, true, true},
        {"Collection", Color::Yellow, Action::None, Id::WifiActivity, true, true},
        {"Attacks", Color::Red, Action::None, Id::WifiAttacks, true, true},
        {"General", Color::Purple, Action::None, Id::WifiGeneral, true, true},
    };
    static const std::vector<Item> wifi_networks = {
        {"Access Points", Color::Orange, Action::OpenWifiScan, Id::Main, false, true, RadioRequirement::Monitor},
    };
    static const std::vector<Item> wifi_activity = {
        {"Traffic graph", Color::Green, Action::OpenTraffic, Id::Main, false, true, RadioRequirement::Selected},
        {"Raw capture", Color::Orange, Action::OpenRawCapture, Id::Main, false, true, RadioRequirement::Monitor},
        {"Pineapple scan", Color::Yellow, Action::OpenPineapple, Id::Main, false, true, RadioRequirement::Monitor},
        {"Remote ID", Color::Magenta, Action::OpenRemoteId, Id::Main, false, true, RadioRequirement::Monitor},
    };
    static const std::vector<Item> wifi_attacks = {
        {"Deauth", Color::Red, Action::None, Id::Main, false, false},
        {"Beacon Spam", Color::Orange, Action::None, Id::Main, false, false},
        {"Evil Portal", Color::Magenta, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> wifi_general = {
        {"Adapter Setup", Color::Purple, Action::OpenAdapters, Id::Main, false, true},
        {"Saved Networks", Color::LightGrey, Action::None, Id::Main, false, false},
    };
    static const std::vector<Item> bluetooth = {
        {"Advertisement feed", Color::Cyan, Action::OpenBluetoothRaw, Id::Main, false, true},
        {"Find My", Color::Green, Action::OpenBluetoothFindMy, Id::Main, false, true},
        {"iBeacon", Color::Blue, Action::OpenBluetoothIBeacon, Id::Main, false, true},
        {"Flipper", Color::Magenta, Action::OpenBluetoothFlipper, Id::Main, false, true},
        {"Flock", Color::Orange, Action::OpenBluetoothFlock, Id::Main, false, true},
        {"Meta", Color::Purple, Action::OpenBluetoothMeta, Id::Main, false, true},
        {"Serial modules", Color::Red, Action::OpenBluetoothSerial, Id::Main, false, true},
    };
    static const std::vector<Item> gps = {
        {"GPS Info", Color::Red, Action::OpenGpsInfo, Id::Main, false, true},
        {"Wardrive", Color::Green, Action::OpenWardrive, Id::Main, false, true},
        {"Track", Color::Orange, Action::OpenGpsTrack, Id::Main, false, true},
    };
    static const std::vector<Item> device = {
        {"Radios and GPS", Color::Cyan, Action::None, Id::Radios, true, true},
        {"Display and power", Color::Green, Action::None, Id::Display, true, true},
        {"Storage and memory", Color::Blue, Action::None, Id::FilesAndMemory, true, true},
        {"Accounts and uploads", Color::Magenta, Action::None, Id::Accounts, true, true},
        {"System Info", Color::Purple, Action::OpenDiagnostics, Id::Main, false, true},
    };
    static const std::vector<Item> radios = {
        {"Radio setup", Color::Cyan, Action::OpenRadioSetup, Id::Main, false, true},
        {"WiFi adapter", Color::Green, Action::OpenAdapters, Id::Main, false, true},
        {"Graph source", Color::Yellow, Action::CycleTrafficSource, Id::Main, false, true},
        {"Wardrive setup", Color::Orange, Action::None, Id::WardriveSettings, true, true},
    };
    static const std::vector<Item> wardrive_settings = {
        {"Channel dwell", Color::Green, Action::CycleWardriveDwell, Id::Main, false, true},
        {"Priority channels", Color::Yellow, Action::CycleWardrivePriority, Id::Main, false, true},
        {"WiFi adapters", Color::Cyan, Action::CycleWardriveAdapters, Id::Main, false, true},
    };
    static const std::vector<Item> display = {
        {"Brightness", Color::Green, Action::Brightness, Id::Main, false, true},
        {"Dim after", Color::Green, Action::DimDelay, Id::Main, false, true},
        {"Battery display", Color::Green, Action::CycleBatteryDisplay, Id::Main, false, true},
        {"Censor display", Color::Magenta, Action::CycleCensor, Id::Main, false, true},
    };
    static const std::vector<Item> accounts = {
        {"Upload policy", Color::Purple, Action::UploadPolicy, Id::Main, false, true},
        {"WiGLE API name", Color::Cyan, Action::WigleName, Id::Main, false, true},
        {"WiGLE API token", Color::Cyan, Action::WigleToken, Id::Main, false, true},
        {"WDGWars API key", Color::Magenta, Action::WdgKey, Id::Main, false, true},
        {"Check WiGLE login", Color::Cyan, Action::CheckWigle, Id::Main, false, true},
        {"Check WDGWars key", Color::Magenta, Action::CheckWdg, Id::Main, false, true},
    };

    static const std::vector<Item> files_memory = {
        {"Screenshots", Color::Blue, Action::CycleStorage, Id::Main, false, true},
        {"Saved captures", Color::Red, Action::OpenPcapFiles, Id::Main, false, true},
        {"Wardrive logs", Color::Green, Action::OpenWardriveFiles, Id::Main, false, true},
        {"Offline maps", Color::Cyan, Action::OpenMapPacks, Id::Main, false, true},
        {"Keep free", Color::Blue, Action::Reserve, Id::Main, false, true},
        {"Image cache", Color::Purple, Action::CycleCache, Id::Main, false, true},
    };
    switch (menu) {
        case Id::FilesAndMemory: return files_memory;
        case Id::Main: return main;
        case Id::Wifi: return wifi;
        case Id::WifiNetworks: return wifi_networks;
        case Id::WifiActivity: return wifi_activity;
        case Id::WifiAttacks: return wifi_attacks;
        case Id::WifiGeneral: return wifi_general;
        case Id::Bluetooth: return bluetooth;
        case Id::Gps: return gps;
        case Id::Device: return device;
        case Id::Radios: return radios;
        case Id::WardriveSettings: return wardrive_settings;
        case Id::Display: return display;
        case Id::Accounts: return accounts;
    }
    return main;
}

const char* title_for(Id menu) {
    switch (menu) {
        case Id::Main: return "MARAUDER";
        case Id::Wifi: return "WiFi";
        case Id::WifiNetworks: return "WiFi > Recon";
        case Id::WifiActivity: return "WiFi > Collection";
        case Id::WifiAttacks: return "WiFi > Attacks";
        case Id::WifiGeneral: return "WiFi > General";
        case Id::Bluetooth: return "Bluetooth";
        case Id::Gps: return "GPS";
        case Id::Device: return "Device";
        case Id::Radios: return "Radios and GPS";
        case Id::WardriveSettings: return "Wardrive setup";
        case Id::Display: return "Display and power";
        case Id::Accounts: return "Accounts and uploads";
        case Id::FilesAndMemory: return "Storage and memory";
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
    switch (current_menu()) {
        case MarauderMenuId::Radios:
        case MarauderMenuId::WardriveSettings:
        case MarauderMenuId::Display:
        case MarauderMenuId::Device:
        case MarauderMenuId::FilesAndMemory:
        case MarauderMenuId::Accounts: return 1;
        case MarauderMenuId::Bluetooth: return 1;
        default: return 2;
    }
}

void MarauderMenuModel::move(int dx, int dy) {
    const int width = static_cast<int>(columns());
    const int count = static_cast<int>(item_count());
    if (!count) return;
    const int current = static_cast<int>(selected_index());
    const auto wrap = [](int value, int size) { return (value % size + size) % size; };
    if (width == 1) {
        if (dy) stack_.back().selected = wrap(current + dy, count);
        return;
    }
    if (dx) {
        const int first = current / width * width;
        const int cells = std::min(width, count - first);
        stack_.back().selected = first + wrap(current - first + dx, cells);
    } else if (dy) {
        const int rows = (count + width - 1) / width;
        const int column = current % width;
        for (int step = 1; step <= rows; ++step) {
            const int candidate = wrap(current / width + dy * step, rows) * width + column;
            if (candidate < count) {
                stack_.back().selected = candidate;
                break;
            }
        }
    }
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
