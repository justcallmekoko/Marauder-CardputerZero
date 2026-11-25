/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>

namespace model {
namespace {
std::filesystem::path preference_path(const char* key) {
    std::filesystem::path root;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) root = xdg;
    else if (const char* home = std::getenv("HOME"); home && *home) root = std::filesystem::path(home) / ".config";
    else if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) root = appdata;
    if (root.empty()) return {};
    return root / "marauder-cardputerzero" / key;
}

std::string read_value(const char* key) {
    std::ifstream input(preference_path(key));
    std::string value;
    input >> value;
    return value;
}

bool save_value(const char* key, const std::string& value) {
    const auto path = preference_path(key);
    if (path.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(path, std::ios::trunc);
    file << value << '\n';
    file.close();
    return !file.fail();
}
}

static std::filesystem::path credential_path(Credential field) {
    switch (field) {
        case Credential::WigleName: return preference_path("wigle-api-name");
        case Credential::WigleToken: return preference_path("wigle-api-token");
        case Credential::WdgKey: return preference_path("wdgwars-api-key");
    }
    return {};
}

bool credential_saved(Credential field) {
    struct stat info{};
    const auto path = credential_path(field);
    return lstat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0;
}

bool store_credential(Credential field, const std::string& value) {
    const auto path = credential_path(field);
    if (path.empty()) return false;
    if (value.empty()) return unlink(path.c_str()) == 0 || errno == ENOENT;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    auto temp = path.string() + ".XXXXXX";
    const int fd = mkstemp(temp.data());
    if (fd < 0) return false;
    const bool written = write(fd, value.data(), value.size()) == static_cast<ssize_t>(value.size()) && fsync(fd) == 0;
    const bool closed = close(fd) == 0;
    if (written && closed && rename(temp.c_str(), path.c_str()) == 0) return true;
    unlink(temp.c_str());
    return false;
}

const char* battery_display_name(BatteryDisplay mode) {
    switch (mode) {
        case BatteryDisplay::Icon: return "Icon";
        case BatteryDisplay::Percent: return "Percentage";
        default: return "Both";
    }
}

BaseModel::BaseModel() {
    documents_ = read_value("screenshot-folder") == "Documents";
    const auto cache = read_value("image-cache-kib");
    if (cache == "256") cache_kib_ = 256;
    if (cache == "1024") cache_kib_ = 1024;
    const auto value = read_value("battery-display");
    if (value == "Icon") battery_display_ = BatteryDisplay::Icon;
    else if (value == "Percentage") battery_display_ = BatteryDisplay::Percent;
}

void BaseModel::cycle_battery_display() {
    switch (battery_display_) {
        case BatteryDisplay::Both: battery_display_ = BatteryDisplay::Icon; break;
        case BatteryDisplay::Icon: battery_display_ = BatteryDisplay::Percent; break;
        case BatteryDisplay::Percent: battery_display_ = BatteryDisplay::Both; break;
    }
    save_value("battery-display", battery_display_name(battery_display_));
}


bool BaseModel::cycle_storage() {
    if (!save_value("screenshot-folder", documents_ ? "Pictures" : "Documents")) return false;
    documents_ = !documents_;
    return true;
}

bool BaseModel::cycle_cache() {
    const unsigned next = cache_kib_ == 0 ? 256 : cache_kib_ == 256 ? 1024 : 0;
    if (!save_value("image-cache-kib", std::to_string(next))) return false;
    cache_kib_ = next;
    return true;
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
        case AppPage::RadioSetup:
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
