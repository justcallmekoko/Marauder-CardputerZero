/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "display_format.h"
#include "system_diagnostics.h"
#include "radio_power.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <chrono>
#include <fstream>
#include <cstdio>

#include <algorithm>
#include <sstream>
#include <system_error>

namespace viewmodel {
namespace {
constexpr const char* brightness_path = "/sys/class/backlight/backlight/brightness";
int backlight_value(const char* path) {
    int n = -1;
    std::ifstream(path) >> n;
    return n;
}

std::pair<std::string,std::string> active_wifi_profile() {
#if defined(__linux__) && !USE_DESKTOP
    FILE* pipe = popen("LC_ALL=C /usr/bin/nmcli -w 1 -t -e no -f TYPE,UUID,NAME connection show --active 2>/dev/null", "r");
    if (!pipe) return {};
    std::pair<std::string,std::string> profile;
    char buffer[1024];
    unsigned count = 0;
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        const std::string prefix = "802-11-wireless:";
        if (line.compare(0, prefix.size(), prefix) != 0) continue;
        const auto sep = line.find(':', prefix.size());
        if (sep == std::string::npos) continue;
        auto uuid = line.substr(prefix.size(), sep - prefix.size());
        if (uuid.size() != 36 || uuid.find_first_not_of("abcdefABCDEF0123456789-") != std::string::npos) continue;
        auto name = line.substr(sep + 1);
        name.erase(name.find_last_not_of("\r\n") + 1);
        profile = {uuid, name};
        ++count;
    }
    if (pclose(pipe) == 0 && count == 1) return profile;
#endif
    return {};
}

int page_to_int(model::AppPage page) { return static_cast<int>(page); }
}

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      battery_display_subject_(static_cast<int>(model_.battery_display())),
      marauder_menu_generation_subject_(0),
      dashboard_summary_subject_("Collecting system information..."),
      diagnostics_detail_subject_("Collecting system information..."),
      diagnostics_generation_subject_(0),
      adapter_detail_subject_("Collecting wireless adapters..."),
      wifi_scan_subject_("SELECT AN ADAPTER FIRST"),
      quit_requested_subject_(false) {
    lv_image_cache_resize(model_.cache_kib() * 1024U, false);
    wifi_scan_timer_ = lv_timer_create(wifi_scan_timer_cb, 100, this);
    original_light_ = backlight_value(brightness_path);
    light_max_ = backlight_value("/sys/class/backlight/backlight/max_brightness");
    activity_tick_ = lv_tick_get();
    if (model_.brightness() && original_light_ >= 0 && light_max_ > 0)
        set_light(std::max(1, static_cast<int>(model_.brightness()) * light_max_ / 100));
    refresh_diagnostics();
}

BaseViewModel::~BaseViewModel() {
    if (changed_light_) set_light(original_light_);
    if (wifi_scan_timer_) {
        lv_timer_delete(wifi_scan_timer_);
        wifi_scan_timer_ = nullptr;
    }
    if (wifi_scan_thread_.joinable()) wifi_scan_thread_.join();
}
lv_subject_t* BaseViewModel::title_subject() { return title_subject_.native(); }
lv_subject_t* BaseViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }
lv_subject_t* BaseViewModel::current_page_subject() { return current_page_subject_.native(); }
lv_subject_t* BaseViewModel::marauder_menu_generation_subject() { return marauder_menu_generation_subject_.native(); }
lv_subject_t* BaseViewModel::dashboard_summary_subject() { return dashboard_summary_subject_.native(); }
lv_subject_t* BaseViewModel::diagnostics_detail_subject() { return diagnostics_detail_subject_.native(); }
lv_subject_t* BaseViewModel::diagnostics_generation_subject() { return diagnostics_generation_subject_.native(); }
lv_subject_t* BaseViewModel::adapter_detail_subject() { return adapter_detail_subject_.native(); }
lv_subject_t* BaseViewModel::wifi_scan_subject() { return wifi_scan_subject_.native(); }
lv_subject_t* BaseViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }
bool BaseViewModel::is_dark_mode() const { return model_.dark_mode(); }
model::AppPage BaseViewModel::current_page() const { return model_.current_page(); }
void BaseViewModel::show_dashboard_page() { model_.set_current_page(model::AppPage::Dashboard); publish_navigation(); }
void BaseViewModel::show_diagnostics_page() {
    refresh_diagnostics();
    model_.set_current_page(model::AppPage::Diagnostics);
    publish_navigation();
}
void BaseViewModel::show_adapters_page() {
    refresh_adapters();
    model_.set_current_page(model::AppPage::Adapters);
    publish_navigation();
}
void BaseViewModel::show_wifi_scan_page() {
    model_.set_current_page(model::AppPage::WifiScan);
    publish_navigation();
    if (wifi_networks_.empty() && !wifi_scan_running_) start_wifi_scan();
}
void BaseViewModel::toggle_page() { model_.toggle_page(); publish_navigation(); }
const char* BaseViewModel::marauder_menu_title() const { return marauder_menu_.title(); }
std::size_t BaseViewModel::marauder_menu_item_count() const { return marauder_menu_.item_count(); }
const model::MarauderMenuItem& BaseViewModel::marauder_menu_item(std::size_t index) const {
    return marauder_menu_.item(index);
}
std::size_t BaseViewModel::marauder_menu_selected_index() const { return marauder_menu_.selected_index(); }
bool BaseViewModel::marauder_menu_at_root() const { return !marauder_menu_.can_go_back(); }
bool BaseViewModel::marauder_menu_can_go_back() const { return marauder_menu_.can_go_back(); }
void BaseViewModel::move_menu_focus(int dx, int dy) {
    pending_uuid_.clear(); pending_network_.clear(); resource_error_.clear();
    const auto previous = marauder_menu_.selected_index();
    marauder_menu_.move(dx, dy);
    if (previous != marauder_menu_.selected_index())
        marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}
namespace {
bool credential_action(model::MarauderMenuAction action, model::Credential& field) {
    using A = model::MarauderMenuAction;
    switch (action) {
        case A::WigleName: field = model::Credential::WigleName; return true;
        case A::WigleToken: field = model::Credential::WigleToken; return true;
        case A::WdgKey: field = model::Credential::WdgKey; return true;
        default: return false;
    }
}
}

void BaseViewModel::credential_key(uint32_t key) {
    if (!credential_open_) return;
    if (key == LV_KEY_ESC) credential_open_ = false;
    else if (key == LV_KEY_ENTER) {
        if (credential_input_.empty() && !clear_credential_) return;
        if (model::store_credential(credential_field_, credential_input_)) credential_open_ = false;
        else credential_prompt_ = "Save failed. Enter to retry.";
    } else if (key == LV_KEY_DEL && credential_input_.empty()) {
        clear_credential_ = true;
        credential_prompt_ = "Enter to remove saved value";
    } else if (key == LV_KEY_BACKSPACE) {
        if (!credential_input_.empty()) credential_input_.pop_back();
    } else if (key >= 33 && key <= 126 && credential_input_.size() < 512) {
        credential_input_.push_back(static_cast<char>(key));
        clear_credential_ = false;
        credential_prompt_ = "Enter to save";
    } else return;
    if (!credential_open_) {
        std::fill(credential_input_.begin(), credential_input_.end(), '\0');
        credential_input_.clear();
    }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

void BaseViewModel::marauder_menu_activate() {
    using Action = model::MarauderMenuAction;
    const auto action = marauder_menu_.activate();
    resource_error_.clear();
    if (action == Action::Brightness) {
        if (light_max_ <= 0 || original_light_ < 0) resource_error_ = "Backlight unavailable";
        else if (!model_.cycle_brightness()) resource_error_ = "Could not save preference";
        else if (!set_light(model_.brightness() ? std::max(1, light_max_ * static_cast<int>(model_.brightness()) / 100) : original_light_))
            resource_error_ = "Backlight write failed";
    }
    if (action == Action::DimDelay && !model_.cycle_dim()) resource_error_ = "Could not save preference";
    if (action == Action::Reserve && !model_.cycle_reserve()) resource_error_ = "Could not save preference";
    if (action == Action::UploadPolicy) choose_upload_policy();
    if (credential_action(action, credential_field_)) {
        credential_open_ = true;
        clear_credential_ = false;
        credential_prompt_ = model::credential_saved(credential_field_) ?
            "Type replacement or Del to remove" : "Type a new value";
    }
    if (action == Action::CycleStorage || action == Action::CycleCache) {
        const bool storage = action == Action::CycleStorage;
        const bool saved = storage ? model_.cycle_storage() : model_.cycle_cache();
        if (saved && !storage) lv_image_cache_resize(model_.cache_kib() * 1024U, true);
        resource_message_ = !saved ? "Could not save preference" : !storage ? "Image cache limit applied" :
            model_.screenshots_in_documents() ? "~/Documents/Marauder" : "~/Pictures/Screenshots";
    }
    if (action == model::MarauderMenuAction::CycleBatteryDisplay) {
        model_.cycle_battery_display();
        battery_display_subject_.set(static_cast<int>(model_.battery_display()));
    }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
    switch (action) {
        case model::MarauderMenuAction::OpenWifiScan:
            show_wifi_scan_page();
            break;
        case model::MarauderMenuAction::OpenAdapters:
            show_adapters_page();
            break;
        case model::MarauderMenuAction::OpenDiagnostics:
            show_diagnostics_page();
            break;
        case Action::OpenRadioSetup:
            model_.set_current_page(model::AppPage::RadioSetup);
            publish_navigation();
            refresh_radios();
            break;
        case Action::WigleName:
        case Action::WigleToken:
        case Action::WdgKey:
        case Action::CycleStorage:
        case Action::Brightness:
        case Action::DimDelay:
        case Action::Reserve:
        case Action::UploadPolicy:
        case Action::CycleCache:
        case model::MarauderMenuAction::CycleBatteryDisplay:
        case model::MarauderMenuAction::None:
            break;
    }
}
const char* BaseViewModel::settings_message() const {
    if (!resource_error_.empty()) return resource_error_.c_str();
    if (!pending_uuid_.empty()) return "Enter to trust WiFi";
    if (marauder_menu_.current_menu() == model::MarauderMenuId::FilesAndMemory) return resource_message_.c_str();
    return "";
}

std::string BaseViewModel::setting_value(model::MarauderMenuAction action) const {
    using Action = model::MarauderMenuAction;
    if (action == Action::Brightness) return model_.brightness() ? std::to_string(model_.brightness()) + "%" : "System";
    if (action == Action::DimDelay) return model_.dim_seconds() ? std::to_string(model_.dim_seconds()) + " sec" : "Never";
    if (action == Action::Reserve) return model_.reserve_mib() ? std::to_string(model_.reserve_mib()) + " MiB" : "Off";
    if (action == Action::UploadPolicy) return !pending_uuid_.empty() ? pending_network_ :
        model_.trusted_profile().empty() ? "Manual" : "Trusted WiFi";
    model::Credential field;
    if (credential_action(action, field)) return model::credential_saved(field) ? "Saved" : "Not set";
    if (action == model::MarauderMenuAction::CycleBatteryDisplay) return model::battery_display_name(model_.battery_display());
    if (action == model::MarauderMenuAction::CycleStorage)
        return model_.screenshots_in_documents() ? "Documents" : "Pictures";
    if (action == model::MarauderMenuAction::CycleCache)
        return model_.cache_kib() ? std::to_string(model_.cache_kib()) + " KiB" : "Off";
    return {};
}

void BaseViewModel::load_radios(std::string key, bool enabled) {
    if (radio_busy()) return;
    radio_list_.message = key.empty() ? "Refreshing" : "Applying";
    try {
        radio_job_ = std::async(std::launch::async, platform::read_radios, std::move(key), enabled);
    } catch (const std::system_error&) { radio_list_.message = "Worker unavailable"; }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

void BaseViewModel::refresh_radios() { load_radios({}, false); }

void BaseViewModel::move_radio(int direction) {
    if (radios().empty()) return;
    if (direction < 0 && radio_cursor_ > 0) --radio_cursor_;
    if (direction > 0 && radio_cursor_ + 1 < radios().size()) ++radio_cursor_;
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

void BaseViewModel::toggle_radio() {
    if (radios().empty()) return;
    const auto& radio = radios()[radio_cursor_];
    if (radio_busy()) return;
    if (radio.kind == "GPS") {
        if (gps_id_ == radio.id) {
            gps_session_.stop();
            gps_id_.clear();
            gps_status_.set("Off");
            radio_list_.message = "GPS released";
        } else if (gps_session_.start(radio.id)) {
            gps_id_ = radio.id;
            gps_status_.set("Wait");
            radio_list_.message = "GPS selected";
        } else {
            gps_id_.clear();
            gps_status_.set("Unavailable");
            radio_list_.message = "GPS reader unavailable";
        }
        sync_gps_rows();
        marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
        return;
    }
    if (radio.state == "On" || radio.state == "Off") load_radios(radio.id, radio.state == "Off");
}

void BaseViewModel::update_gps_status() {
    const auto state = gps_session_.poll();
    if (!state.empty()) gps_status_.set(state.c_str());
}

void BaseViewModel::sync_gps_rows() {
    for (auto& d : radio_list_.devices) {
        if (d.kind == "GPS") d.state = d.id == gps_id_ ? "On" : "Off";
    }
}

void BaseViewModel::finish_radio_setting() {
    if (!radio_busy() || radio_job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const auto selected = radios().empty() ? std::string{} : radios()[radio_cursor_].id;
    try { radio_list_ = radio_job_.get(); }
    catch (const std::exception&) { radio_list_ = {"Radio request failed", {}}; }
    sync_gps_rows();
    radio_cursor_ = 0;
    for (std::size_t i = 0; i < radios().size(); ++i) {
        if (radios()[i].id == selected) { radio_cursor_ = i; break; }
    }
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}

bool BaseViewModel::marauder_menu_back() {
    pending_uuid_.clear(); pending_network_.clear(); resource_error_.clear();
    if (!marauder_menu_.back()) return false;
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
    return true;
}

void BaseViewModel::refresh_diagnostics() {
    diagnostics_snapshot_ = platform::diagnostics::collect();
    system_info_.refresh_counts(diagnostics_snapshot_.network_interfaces.size(),
                                diagnostics_snapshot_.usb_devices.size(),
                                diagnostics_snapshot_.peripheral_nodes.size(),
                                diagnostics_snapshot_.tools.size());
    const auto summary = platform::diagnostics::dashboard_summary(diagnostics_snapshot_);
    dashboard_summary_subject_.set(summary.c_str());
    publish_diagnostics();
    refresh_adapters();
}

void BaseViewModel::previous_system_info_page() { system_info_.previous_page(); publish_diagnostics(); }
void BaseViewModel::next_system_info_page() { system_info_.next_page(); publish_diagnostics(); }
void BaseViewModel::toggle_system_info_details() { system_info_.toggle_details(); publish_diagnostics(); }

void BaseViewModel::publish_diagnostics() {
    const auto text = format::system_info_page(diagnostics_snapshot_, system_info_);
    diagnostics_detail_subject_.set(text.c_str());
    diagnostics_generation_subject_.set(diagnostics_generation_subject_.value() + 1);
}

void BaseViewModel::refresh_adapters() {
    auto catalog = platform::wireless::enumerate();
    adapters_ = std::move(catalog.adapters);
    adapter_error_ = std::move(catalog.error);
    if (adapters_.empty()) {
        highlighted_adapter_ = 0;
    } else if (highlighted_adapter_ >= adapters_.size()) {
        highlighted_adapter_ = adapters_.size() - 1;
    }
    adapter_details_expanded_ = false;
    publish_adapter_detail();
}

void BaseViewModel::previous_adapter() {
    if (adapters_.empty()) return;
    highlighted_adapter_ = highlighted_adapter_ == 0 ? adapters_.size() - 1 : highlighted_adapter_ - 1;
    adapter_details_expanded_ = false;
    publish_adapter_detail();
}

void BaseViewModel::next_adapter() {
    if (adapters_.empty()) return;
    highlighted_adapter_ = (highlighted_adapter_ + 1) % adapters_.size();
    adapter_details_expanded_ = false;
    publish_adapter_detail();
}

void BaseViewModel::select_highlighted_adapter() {
    if (adapters_.empty()) return;
    const auto& adapter = adapters_[highlighted_adapter_];
    if (model_.selected_adapter_id() != adapter.stable_id) {
        wifi_networks_.clear();
        wifi_scan_interface_.clear();
        wifi_scan_error_.clear();
        wifi_scan_page_ = 0;
        wifi_scan_detail_index_ = 0;
        wifi_scan_details_expanded_ = false;
    }
    model_.set_selected_adapter_id(adapter.stable_id);
    publish_adapter_detail();
    publish_wifi_scan();
}

void BaseViewModel::toggle_adapter_details() {
    adapter_details_expanded_ = !adapter_details_expanded_;
    publish_adapter_detail();
}

void BaseViewModel::start_wifi_scan() {
    if (wifi_scan_running_) return;
    const auto selected = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (selected == adapters_.end()) {
        wifi_scan_error_ = "Select an adapter on the Adapters page";
        publish_wifi_scan();
        return;
    }
    if (wifi_scan_thread_.joinable()) wifi_scan_thread_.join();
    wifi_scan_interface_ = selected->interface_name;
    wifi_scan_error_.clear();
    wifi_scan_running_ = true;
    wifi_scan_ready_ = false;
    publish_wifi_scan();
    const auto interface_name = wifi_scan_interface_;
    try {
        wifi_scan_thread_ = std::thread([this, interface_name] {
            platform::wireless::WifiScanResult result;
            try {
                result = platform::wireless::scan_wifi_networks(interface_name);
            } catch (...) {
                result.error = "Unexpected Wi-Fi scan failure";
            }
            {
                std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
                pending_wifi_scan_ = std::move(result);
                pending_wifi_scan_interface_ = interface_name;
            }
            wifi_scan_ready_ = true;
        });
    } catch (const std::system_error&) {
        wifi_scan_running_ = false;
        wifi_scan_error_ = "Unable to start background scan";
        publish_wifi_scan();
    }
}

void BaseViewModel::previous_wifi_scan_page() {
    if (wifi_scan_details_expanded_) {
        if (wifi_scan_detail_index_ > 0) --wifi_scan_detail_index_;
    } else {
        const std::size_t pages = std::max<std::size_t>(1, (wifi_networks_.size() + 4) / 5);
        wifi_scan_page_ = wifi_scan_page_ == 0 ? pages - 1 : wifi_scan_page_ - 1;
    }
    publish_wifi_scan();
}

void BaseViewModel::next_wifi_scan_page() {
    if (wifi_scan_details_expanded_) {
        if (wifi_scan_detail_index_ + 1 < std::max<std::size_t>(1, wifi_networks_.size())) ++wifi_scan_detail_index_;
    } else {
        const std::size_t pages = std::max<std::size_t>(1, (wifi_networks_.size() + 4) / 5);
        wifi_scan_page_ = (wifi_scan_page_ + 1) % pages;
    }
    publish_wifi_scan();
}

void BaseViewModel::toggle_wifi_scan_details() {
    if (!wifi_scan_details_expanded_) {
        wifi_scan_detail_index_ = std::min(wifi_scan_page_ * 5, wifi_networks_.empty() ? std::size_t{0} : wifi_networks_.size() - 1);
        wifi_scan_details_expanded_ = true;
    } else {
        wifi_scan_page_ = wifi_scan_detail_index_ / 5;
        wifi_scan_details_expanded_ = false;
    }
    publish_wifi_scan();
}

bool BaseViewModel::set_light(int value) {
    if (value < 0 || light_max_ <= 0) return false;
    std::ofstream file(brightness_path);
    file << value;
    file.close();
    if (!file) return false;
    changed_light_ = value != original_light_;
    return true;
}

bool BaseViewModel::wake_display() {
    activity_tick_ = lv_tick_get();
    if (before_dim_ < 0) return false;
    if (set_light(before_dim_)) before_dim_ = -1;
    return true;
}

void BaseViewModel::idle_display() {
    if (!model_.dim_seconds() || before_dim_ >= 0 || light_max_ <= 0 ||
        lv_tick_elaps(activity_tick_) < model_.dim_seconds() * 1000) return;
    const int current = backlight_value(brightness_path);
    const int target = std::max(1, current / 5);
    if (current > target && set_light(target)) before_dim_ = current;
    else activity_tick_ = lv_tick_get();
}

void BaseViewModel::choose_upload_policy() {
    if (!model_.trusted_profile().empty()) {
        if (!model_.set_trusted_profile("")) resource_error_ = "Could not save policy";
        return;
    }
    const auto profile = active_wifi_profile();
    if (profile.first.empty()) {
        resource_error_ = "Connect one WiFi network";
        pending_uuid_.clear();
    } else if (profile.first == pending_uuid_) {
        if (!model_.set_trusted_profile(pending_uuid_)) resource_error_ = "Could not save policy";
        pending_uuid_.clear();
    } else {
        pending_uuid_ = profile.first;
        pending_network_ = profile.second;
    }
}

void BaseViewModel::request_quit() { quit_requested_subject_.set(true); }

void BaseViewModel::wifi_scan_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (self) {
        self->idle_display();
        self->finish_wifi_scan_if_ready();
        self->finish_radio_setting();
        self->update_gps_status();
    }
}

void BaseViewModel::finish_wifi_scan_if_ready() {
    if (!wifi_scan_ready_.exchange(false)) return;
    if (wifi_scan_thread_.joinable()) wifi_scan_thread_.join();
    platform::wireless::WifiScanResult completed;
    std::string completed_interface;
    {
        std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
        completed = std::move(pending_wifi_scan_);
        completed_interface = std::move(pending_wifi_scan_interface_);
        pending_wifi_scan_ = {};
    }
    const auto selected = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (selected != adapters_.end() && selected->interface_name == completed_interface) {
        wifi_scan_interface_ = completed_interface;
        wifi_networks_ = std::move(completed.networks);
        wifi_scan_error_ = std::move(completed.error);
        wifi_scan_page_ = 0;
        wifi_scan_detail_index_ = 0;
        wifi_scan_details_expanded_ = false;
    }
    wifi_scan_running_ = false;
    publish_wifi_scan();
}

void BaseViewModel::publish_navigation() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

void BaseViewModel::publish_wifi_scan() {
    const auto index = wifi_scan_details_expanded_ ? wifi_scan_detail_index_ : wifi_scan_page_;
    const auto text = format::wifi_scan_page(wifi_scan_interface_,
                                             wifi_networks_,
                                             wifi_scan_error_,
                                             wifi_scan_running_,
                                             wifi_scan_details_expanded_,
                                             index);
    wifi_scan_subject_.set(text.c_str());
}

void BaseViewModel::publish_adapter_detail() {
    std::ostringstream out;
    if (adapters_.empty()) {
        out << "NO WIRELESS ADAPTERS\n"
            << (adapter_error_.empty() ? "No wireless sysfs interfaces detected" : adapter_error_);
        adapter_detail_subject_.set(out.str().c_str());
        return;
    }

    const auto& adapter = adapters_[highlighted_adapter_];
    const bool selected = model_.selected_adapter_id() == adapter.stable_id;
    if (!adapter_details_expanded_) {
        out << "ADAPTER " << (highlighted_adapter_ + 1) << '/' << adapters_.size()
            << (selected ? "  [SELECTED]" : "  [C/ENTER SELECT]") << '\n'
            << adapter.interface_name << "  " << adapter.bus_type << "  " << adapter.operstate << '\n';
        const auto product = format::clip_for_display(adapter.manufacturer +
                                              (adapter.manufacturer.empty() || adapter.product_name.empty() ? "" : " ") +
                                              adapter.product_name,
                                              42);
        if (!product.empty()) out << product << '\n';
        if (!adapter.vendor_id.empty()) out << "USB " << adapter.vendor_id << ':' << adapter.product_id << '\n';
        out << "Driver " << (adapter.driver.empty() ? "unknown" : adapter.driver)
            << "  PHY " << (adapter.phy_name.empty() ? "unknown" : adapter.phy_name) << '\n'
            << "MAC " << (adapter.mac_address.empty() ? "unknown" : adapter.mac_address) << '\n'
            << "F/X ITEM C/ENTER SELECT\n"
            << "Z/ESC BACK D MORE";
    } else {
        out << "ADAPTER " << (highlighted_adapter_ + 1) << '/' << adapters_.size() << "  DETAILS\n"
            << adapter.interface_name << "  ifindex " << (adapter.ifindex.empty() ? "?" : adapter.ifindex) << '\n'
            << "ID " << format::clip_for_display(adapter.stable_id, 30) << '\n'
            << "MAC " << (adapter.permanent_mac ? "PERMANENT" : "ASSIGNED") << '\n'
            << "Monitor " << platform::wireless::capability_label(adapter.monitor_mode) << '\n'
            << "Injection " << platform::wireless::capability_label(adapter.injection) << '\n'
            << adapter.capability_note;
    }
    adapter_detail_subject_.set(out.str().c_str());
}

} // namespace viewmodel
