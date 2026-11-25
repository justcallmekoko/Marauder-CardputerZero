/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "display_format.h"
#include "system_diagnostics.h"
#include "radio_power.h"
#include "channel_survey.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <chrono>
#include <fstream>
#include <cstdio>

#include <algorithm>
#include <sstream>
#include <system_error>
#include <iterator>
#if defined(__linux__) && !USE_DESKTOP
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#endif

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
    gps_id_ = model::saved_gps_id();
    if (!gps_id_.empty())
        gps_status_.set(gps_session_.start(gps_id_) ? "Wait" : "Unavailable");
    radio_session_ = platform::begin_radio_session();
    refresh_radios();
    activity_tick_ = lv_tick_get();
    if (model_.brightness() && original_light_ >= 0 && light_max_ > 0)
        set_light(std::max(1, static_cast<int>(model_.brightness()) * light_max_ / 100));
    refresh_diagnostics();
}

BaseViewModel::~BaseViewModel() {
    pcap_capture_.stop();
    if (changed_light_) set_light(original_light_);
    if (wifi_scan_timer_) {
        lv_timer_delete(wifi_scan_timer_);
        wifi_scan_timer_ = nullptr;
    }
    stop_wifi_capture();
    if (radio_job_.valid()) radio_job_.wait();
    if (monitor_job_.valid()) monitor_job_.wait();
    monitor_session_.stop();
    gps_session_.stop();
    platform::end_radio_session(radio_session_);
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
void BaseViewModel::show_dashboard_page() { stop_wifi_capture(); model_.set_current_page(model::AppPage::Dashboard); publish_navigation(); }
void BaseViewModel::show_diagnostics_page() {
    stop_wifi_capture();
    refresh_diagnostics();
    model_.set_current_page(model::AppPage::Diagnostics);
    publish_navigation();
}
void BaseViewModel::show_adapters_page() {
    stop_wifi_capture();
    refresh_adapters();
    model_.set_current_page(model::AppPage::Adapters);
    publish_navigation();
}
void BaseViewModel::show_wifi_scan_page() {
    model_.set_current_page(model::AppPage::WifiScan);
    publish_navigation();
    if (!wifi_scan_running_) start_wifi_scan();
}
void BaseViewModel::show_pcap_files_page() {
    stop_wifi_capture();
    refresh_pcap_files();
    pcap_details_ = false;
    model_.set_current_page(model::AppPage::PcapFiles);
    publish_navigation();
}

void BaseViewModel::refresh_pcap_files() {
    pcap_files_ = platform::pcap::list_files();
    if (pcap_cursor_ >= pcap_files_.size()) pcap_cursor_ = pcap_files_.empty() ? 0 : pcap_files_.size() - 1;
    pcap_files_generation_.set(pcap_files_generation_.value() + 1);
}

void BaseViewModel::move_pcap_file(int direction) {
    if (pcap_files_.empty() || pcap_details_) return;
    if (direction < 0 && pcap_cursor_) --pcap_cursor_;
    if (direction > 0 && pcap_cursor_ + 1 < pcap_files_.size()) ++pcap_cursor_;
    pcap_files_generation_.set(pcap_files_generation_.value() + 1);
}

void BaseViewModel::toggle_pcap_details() {
    if (pcap_files_.empty()) return;
    pcap_details_ = !pcap_details_;
    pcap_files_generation_.set(pcap_files_generation_.value() + 1);
}

std::string BaseViewModel::toggle_pcap_capture() {
    if (pcap_capture_.active()) {
        const auto path = pcap_capture_.stop();
        pcap_status_.set("P");
        pcap_message_ = "Saved " + std::to_string(pcap_capture_.frames()) + " frames";
        refresh_pcap_files();
        return pcap_message_ + " to " + path;
    }
    const auto warning = collection_warning(model::RadioRequirement::Monitor);
    if (!warning.empty()) return warning;
    if (monitor_job_.valid()) return "Radio mode is changing";
    const int socket = monitor_session_.capture_socket();
    if (socket < 0) return "Monitor capture unavailable";
    const auto error = pcap_capture_.start(socket, model_.reserve_mib());
    if (!error.empty()) return error;
    pcap_status_.set("REC");
    pcap_message_ = "Recording to " + pcap_capture_.path();
    refresh_pcap_files();
    return pcap_message_;
}
void BaseViewModel::toggle_page() { stop_wifi_capture(); model_.toggle_page(); publish_navigation(); }
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

std::string BaseViewModel::collection_warning(model::RadioRequirement requirement) const {
    using Requirement = model::RadioRequirement;
    if (requirement == Requirement::None) return {};
    if (model_.selected_adapter_id().empty()) return "Select a WiFi adapter first";
    const auto selected = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (selected == adapters_.end()) return "Selected adapter unavailable";
    const std::string path = "/sys/class/net/" + selected->interface_name + "/";
    std::string index;
    if (!(std::ifstream(path + "ifindex") >> index) || index != selected->ifindex)
        return "Selected adapter unavailable";
    if (requirement == Requirement::Selected) return {};
    int type{};
    if (!(std::ifstream(path + "type") >> type)) return "Cannot verify adapter mode";
    const bool monitor = type == 801 || type == 802 || type == 803;
    if (requirement == Requirement::Managed && monitor) return "Turn monitor mode off for AP scans";
    if (requirement == Requirement::Monitor && !monitor)
        return selected->monitor_mode == platform::wireless::CapabilityState::Unsupported ?
            "Select a monitor-capable adapter" : "Enable monitor mode first";
    return {};
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
    if (action == Action::CycleTrafficSource && !model_.cycle_traffic_source())
        resource_error_ = "Could not save graph source";
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
        case Action::OpenTraffic:
            model_.set_current_page(model::AppPage::Traffic);
            publish_navigation();
            break;
        case Action::OpenPcapFiles:
            show_pcap_files_page();
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
        case Action::CycleTrafficSource:
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
    if (action == Action::CycleTrafficSource) return model_.traffic_monitor_source() ? "Monitor" : "Adapter";
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
        radio_job_ = std::async(std::launch::async, platform::read_radios, std::move(key), enabled, radio_session_);
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
        const auto next = gps_id_ == radio.id ? std::string{} : radio.id;
        if (!model::save_gps_id(next)) {
            radio_list_.message = "Could not save GPS";
        } else if (next.empty()) {
            gps_session_.stop();
            gps_id_.clear();
            gps_status_.set("Off");
            radio_list_.message = "GPS released";
        } else if (gps_session_.start(radio.id)) {
            gps_id_ = radio.id;
            gps_status_.set("Wait");
            radio_list_.message = "GPS selected";
        } else {
            gps_id_ = next;
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
    auto& rows = radio_list_.devices;
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](const auto& d) {
        return d.kind == "GPS" && d.name == "Saved GPS";
    }), rows.end());
    bool found = false;
    for (auto& d : rows) {
        if (d.kind != "GPS") continue;
        d.state = d.id == gps_id_ ? "On" : "Off";
        found |= d.id == gps_id_;
    }
    if (!gps_id_.empty() && !found)
        rows.push_back({gps_id_, "GPS", "Saved GPS", "Disconnected", "Waiting for receiver",
                        "Enter to disable", "", "On", ""});
    if (radio_cursor_ >= rows.size()) radio_cursor_ = rows.empty() ? 0 : rows.size() - 1;
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

void BaseViewModel::toggle_monitor() {
    if (monitor_busy() || highlighted_adapter_ >= adapters_.size()) return;
    const auto& adapter = adapters_[highlighted_adapter_];
    if (adapter.stable_id != model_.selected_adapter_id()) adapter_error_ = "Select adapter first";
    else if (wifi_scan_running_) adapter_error_ = "Leave Access Points first";
    else if (pcap_capture_.active() && !monitor_owner_.empty()) adapter_error_ = "Stop PCAP capture first";
    else if (monitor_owner_.empty() && adapter.monitor_mode != platform::wireless::CapabilityState::Supported)
        adapter_error_ = "Monitor support unavailable";
    else {
        const bool start = monitor_owner_.empty();
        const auto name = adapter.interface_name;
        monitor_owner_ = adapter.stable_id;
        try {
            monitor_job_ = std::async(std::launch::async, [this, name, start] {
                return start ? monitor_session_.start(name) : monitor_session_.stop();
            });
            adapter_error_ = "Switching radio mode...";
        } catch (const std::system_error&) {
            if (start) monitor_owner_.clear();
            adapter_error_ = "Cannot switch mode now";
        }
    }
    publish_adapter_detail();
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
    const auto saved = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (saved != adapters_.end()) highlighted_adapter_ = saved - adapters_.begin();
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
    const bool changed = model_.selected_adapter_id() != adapter.stable_id;
    if (changed && !monitor_owner_.empty()) {
        adapter_error_ = "Turn monitor off first";
        publish_adapter_detail();
        return;
    }
    if (!model_.set_selected_adapter_id(adapter.stable_id)) {
        adapter_error_ = "Could not save WiFi selection";
        publish_adapter_detail();
        return;
    }
    adapter_error_.clear();
    if (changed) {
        wifi_networks_.clear();
        wifi_scan_interface_.clear();
        wifi_scan_error_.clear();
        wifi_scan_detail_index_ = 0;
        wifi_scan_details_expanded_ = false;
        wifi_view_ = 0;
        wifi_info_tab_ = 0;
    }
    publish_adapter_detail();
    publish_wifi_scan();
}

void BaseViewModel::toggle_adapter_details() {
    adapter_details_expanded_ = !adapter_details_expanded_;
    publish_adapter_detail();
}

void BaseViewModel::start_wifi_scan() {
    stop_wifi_capture();
    const auto selected = std::find_if(adapters_.begin(), adapters_.end(), [&](const auto& adapter) {
        return adapter.stable_id == model_.selected_adapter_id();
    });
    if (selected == adapters_.end()) {
        wifi_scan_error_ = "Select an adapter on the Adapters page";
        publish_wifi_scan();
        return;
    }
    wifi_scan_interface_ = selected->interface_name;
    wifi_frequencies_ = selected->frequencies;
    std::stable_sort(wifi_frequencies_.begin(), wifi_frequencies_.end(), [](unsigned a, unsigned b) {
        return (a < 3000) != (b < 3000) ? a < 3000 : a < b;
    });
    wifi_frequencies_.erase(std::unique(wifi_frequencies_.begin(), wifi_frequencies_.end()), wifi_frequencies_.end());
    wifi_scan_error_ = collection_warning(model::RadioRequirement::Monitor);
    if (!wifi_scan_error_.empty()) { publish_wifi_scan(); return; }
    const int capture = monitor_session_.capture_socket();
    if (capture < 0) {
        wifi_scan_error_ = "Monitor capture unavailable";
        publish_wifi_scan();
        return;
    }
    wifi_networks_.clear();
    wifi_scan_detail_index_ = 0;
    wifi_client_index_ = 0;
    wifi_view_ = 0;
    wifi_info_tab_ = 0;
    wifi_feed_frames_.clear();
    wifi_feed_target_.clear();
    wifi_feed_count_ = 0;
    wifi_capture_stop_ = false;
    wifi_capture_failed_ = false;
    wifi_hop_index_ = 0;
    wifi_tuned_frequency_ = 0;
    wifi_tune_failed_frequency_ = 0;
    wifi_hop_tick_ = 0;
    wifi_scan_running_ = true;
    try {
        wifi_scan_thread_ = std::thread([this, capture] {
#if defined(__linux__) && !USE_DESKTOP
            uint8_t bytes[8192];
            while (!wifi_capture_stop_) {
                pollfd ready{capture, POLLIN, 0};
                const int status = poll(&ready, 1, 150);
                if (status < 0) { if (errno == EINTR) continue; break; }
                if (status == 0) continue;
                if (ready.revents & (POLLERR | POLLHUP | POLLNVAL)) break;
                if (!(ready.revents & POLLIN)) continue;
                const auto count = read(capture, bytes, sizeof(bytes));
                if (count <= 0) break;
                platform::wireless::FrameObservation observation;
                platform::wireless::FrameSummary summary;
                const bool has_summary = platform::wireless::parse_frame_summary(bytes, static_cast<size_t>(count), summary);
                const bool has_observation = platform::wireless::parse_monitor_frame(bytes, static_cast<size_t>(count), observation);
                if (!has_summary && !has_observation) continue;
                std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
                if (has_observation && incoming_frames_.size() < 1024) incoming_frames_.push_back(std::move(observation));
                if (has_summary && incoming_summaries_.size() < 1024) incoming_summaries_.push_back(std::move(summary));
            }
            if (!wifi_capture_stop_) wifi_capture_failed_ = true;
            close(capture);
#endif
        });
        wifi_scan_error_.clear();
    } catch (const std::system_error&) {
#if defined(__linux__) && !USE_DESKTOP
        close(capture);
#endif
        wifi_scan_running_ = false;
        wifi_scan_error_ = "Unable to start frame capture";
    }
    publish_wifi_scan();
}

void BaseViewModel::stop_wifi_capture() {
    wifi_capture_stop_ = true;
    if (wifi_scan_thread_.joinable()) wifi_scan_thread_.join();
    if (wifi_hop_job_.valid()) wifi_hop_job_.wait();
    wifi_scan_running_ = false;
    std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
    incoming_frames_.clear();
    incoming_summaries_.clear();
}

void BaseViewModel::advance_wifi_channel() {
    if (!wifi_scan_running_ || wifi_frequencies_.empty()) return;
    if (wifi_hop_job_.valid()) {
        if (wifi_hop_job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        const auto reply = wifi_hop_job_.get();
        if (reply == "OK") {
            wifi_tuned_frequency_ = wifi_frequencies_[wifi_hop_index_];
            wifi_tune_failed_frequency_ = 0;
            wifi_scan_error_.clear();
        }
        else { wifi_tune_failed_frequency_ = wifi_frequencies_[wifi_hop_index_]; wifi_scan_error_ = "Channel tune failed"; }
        publish_wifi_scan();
    }
    if (wifi_hop_tick_ && lv_tick_elaps(wifi_hop_tick_) < 650 && wifi_view_ == 0) return;
    unsigned requested{};
    if (wifi_view_ == 0) {
        wifi_hop_index_ = wifi_hop_tick_ ? (wifi_hop_index_ + 1) % wifi_frequencies_.size() : 0;
        requested = wifi_frequencies_[wifi_hop_index_];
    } else if (wifi_scan_detail_index_ < wifi_networks_.size()) {
        requested = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
        const auto found = std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), requested);
        if (found == wifi_frequencies_.end()) return;
        wifi_hop_index_ = static_cast<size_t>(found - wifi_frequencies_.begin());
        if (requested == wifi_tuned_frequency_) return;
        if (requested == wifi_tune_failed_frequency_) return;
    }
    if (!requested) return;
    wifi_hop_tick_ = lv_tick_get();
    wifi_hop_job_ = std::async(std::launch::async, [this, requested] { return monitor_session_.tune(requested); });
}

void BaseViewModel::previous_wifi_scan_page() {
    if (wifi_view_ == 6) { if (wifi_feed_offset_ + 1 < wifi_feed_frames_.size()) ++wifi_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 4 || wifi_view_ == 5) { if (wifi_operation_index_) --wifi_operation_index_; publish_wifi_scan(); return; }
    auto& index = wifi_view_ == 2 || wifi_view_ == 3 ? wifi_client_index_ : wifi_scan_detail_index_;
    if (index > 0) { --index; if (wifi_view_ <= 1) wifi_info_tab_ = 0; }
    publish_wifi_scan();
}

void BaseViewModel::next_wifi_scan_page() {
    if (wifi_view_ == 6) { if (wifi_feed_offset_) --wifi_feed_offset_; publish_wifi_scan(); return; }
    if (wifi_view_ == 4 || wifi_view_ == 5) {
        if (wifi_operation_index_ + 1 < (wifi_view_ == 4 ? 3u : 2u)) ++wifi_operation_index_;
        publish_wifi_scan(); return;
    }
    auto& index = wifi_view_ == 2 || wifi_view_ == 3 ? wifi_client_index_ : wifi_scan_detail_index_;
    const auto limit = (wifi_view_ == 2 || wifi_view_ == 3) && wifi_scan_detail_index_ < wifi_networks_.size() ?
        wifi_networks_[wifi_scan_detail_index_].clients.size() : wifi_networks_.size();
    if (index + 1 < limit) { ++index; if (wifi_view_ <= 1) wifi_info_tab_ = 0; }
    publish_wifi_scan();
}

void BaseViewModel::toggle_wifi_scan_details() {
    if (wifi_networks_.empty()) return;
    if (wifi_view_ == 0) { wifi_view_ = 4; wifi_operation_index_ = 0; }
    else if (wifi_view_ == 4) {
        if (wifi_operation_index_ == 0) { wifi_view_ = 1; wifi_info_tab_ = 0; }
        else if (wifi_operation_index_ == 1) { wifi_view_ = 2; wifi_client_index_ = 0; }
        else {
            const auto mhz = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
            if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), static_cast<unsigned>(mhz)) == wifi_frequencies_.end()) {
                wifi_scan_error_ = "AP channel not available";
                publish_wifi_scan();
                return;
            }
            wifi_scan_error_.clear();
            wifi_feed_target_ = wifi_networks_[wifi_scan_detail_index_].bssid;
            wifi_feed_station_ = false; wifi_feed_frames_.clear(); wifi_feed_count_ = 0; wifi_feed_offset_ = 0;
            wifi_view_ = 6;
        }
    } else if (wifi_view_ == 2 && !wifi_networks_[wifi_scan_detail_index_].clients.empty()) {
        wifi_view_ = 5; wifi_operation_index_ = 0;
    } else if (wifi_view_ == 5) {
        if (wifi_operation_index_ == 0) wifi_view_ = 3;
        else {
            const auto mhz = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
            if (!mhz || std::find(wifi_frequencies_.begin(), wifi_frequencies_.end(), static_cast<unsigned>(mhz)) == wifi_frequencies_.end()) {
                wifi_scan_error_ = "AP channel not available";
                publish_wifi_scan();
                return;
            }
            wifi_scan_error_.clear();
            wifi_feed_target_ = wifi_networks_[wifi_scan_detail_index_].clients[wifi_client_index_].mac;
            wifi_feed_station_ = true; wifi_feed_frames_.clear(); wifi_feed_count_ = 0; wifi_feed_offset_ = 0;
            wifi_view_ = 6;
        }
    }
    wifi_hop_tick_ = 0;
    wifi_tune_failed_frequency_ = 0;
    wifi_scan_details_expanded_ = wifi_view_ != 0;
    publish_wifi_scan();
}

void BaseViewModel::wifi_back() {
    if (wifi_view_ == 6) wifi_view_ = wifi_feed_station_ ? 5 : 4;
    else if (wifi_view_ == 1 || wifi_view_ == 2) wifi_view_ = 4;
    else if (wifi_view_ == 3) wifi_view_ = 5;
    else if (wifi_view_ == 5) wifi_view_ = 2;
    else wifi_view_ = 0;
    if (wifi_view_ == 0) wifi_info_tab_ = 0;
    wifi_scan_details_expanded_ = wifi_view_ != 0;
    publish_wifi_scan();
}

void BaseViewModel::advance_wifi_info_tab() {
    if (wifi_view_ != 1) return;
    wifi_info_tab_ = (wifi_info_tab_ + 1) % 3;
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
        if (self->monitor_busy() && self->monitor_job_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const auto message = self->monitor_job_.get();
            if (message != "Monitor on") self->monitor_owner_.clear();
            self->refresh_adapters();
            self->adapter_error_ = message;
            self->publish_adapter_detail();
        }
        self->finish_wifi_scan_if_ready();
        const auto pcap_error = self->pcap_capture_.take_error();
        if (!pcap_error.empty()) {
            self->pcap_status_.set("PCAP!");
            self->pcap_message_ = pcap_error;
            self->refresh_pcap_files();
        }
        if (self->wifi_capture_failed_.exchange(false)) {
            self->stop_wifi_capture();
            self->wifi_scan_error_ = "Monitor capture stopped";
            self->publish_wifi_scan();
        }
        self->advance_wifi_channel();
        self->finish_radio_setting();
        self->update_gps_status();
    }
}

void BaseViewModel::finish_wifi_scan_if_ready() {
    std::vector<platform::wireless::FrameObservation> incoming;
    std::vector<platform::wireless::FrameSummary> summaries;
    {
        std::lock_guard<std::mutex> lock(wifi_scan_mutex_);
        incoming.swap(incoming_frames_);
        summaries.swap(incoming_summaries_);
    }
    if (incoming.empty() && summaries.empty()) return;
    if (wifi_view_ == 6 && !wifi_feed_target_.empty() && wifi_scan_detail_index_ < wifi_networks_.size()) {
        const int channel = wifi_networks_[wifi_scan_detail_index_].frequency_mhz;
        for (auto& frame : summaries) {
            if (!platform::wireless::frame_matches_target(frame, wifi_feed_target_, channel)) continue;
            ++wifi_feed_count_;
            if (wifi_feed_frames_.size() == 128) wifi_feed_frames_.pop_front();
            wifi_feed_frames_.push_back(std::move(frame));
            if (wifi_feed_offset_) wifi_feed_offset_ = std::min(wifi_feed_offset_ + 1, wifi_feed_frames_.size() - 1);
        }
    }
    for (const auto& frame : incoming) {
        auto ap = std::find_if(wifi_networks_.begin(), wifi_networks_.end(), [&](const auto& item) { return item.bssid == frame.bssid; });
        if (ap == wifi_networks_.end()) {
            if (wifi_networks_.size() >= 512) continue;
            platform::wireless::WifiNetwork item;
            item.bssid = frame.bssid;
            item.ssid = frame.ssid.empty() ? "[hidden/unknown]" : frame.ssid;
            item.vendor = platform::wireless::vendor_for_mac(frame.bssid);
            wifi_networks_.push_back(std::move(item));
            ap = std::prev(wifi_networks_.end());
        }
        ++ap->frames;
        if (frame.beacon) {
            if (!frame.ssid.empty()) ap->ssid = frame.ssid;
            ap->security = frame.security;
            ap->security_info = frame.security_info;
            if (frame.has_rssi) { ap->rssi = frame.rssi; ap->has_rssi = true; }
        }
        // Radiotap can report the tuned channel for an adjacent-channel AP.
        // Prefer the primary channel advertised in the beacon/response IEs.
        if (frame.advertised_frequency_mhz) {
            ap->frequency_mhz = frame.advertised_frequency_mhz;
            ap->channel_advertised = true;
        } else if (frame.frequency_mhz && !ap->channel_advertised &&
                   (frame.beacon || !ap->frequency_mhz)) {
            ap->frequency_mhz = frame.frequency_mhz;
        }
        ap->channel = platform::wireless::channel_number(ap->frequency_mhz);
        if (frame.client.empty()) continue;
        auto client = std::find_if(ap->clients.begin(), ap->clients.end(), [&](const auto& item) { return item.mac == frame.client; });
        if (client == ap->clients.end()) {
            if (ap->clients.size() >= 64) continue;
            ap->clients.push_back({frame.client, platform::wireless::vendor_for_mac(frame.client)});
            client = std::prev(ap->clients.end());
        }
        ++client->frames;
        if (frame.client_transmitted && frame.has_rssi) { client->rssi = frame.rssi; client->has_rssi = true; }
    }
    publish_wifi_scan();
}

void BaseViewModel::publish_navigation() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

void BaseViewModel::publish_wifi_scan() {
    const auto text = format::wifi_scan_page(wifi_scan_interface_,
                                             wifi_networks_,
                                             wifi_scan_error_,
                                             wifi_scan_running_,
                                             wifi_scan_details_expanded_,
                                             wifi_scan_detail_index_);
    wifi_scan_subject_.set(text.c_str());
    inventory_generation_.set(inventory_generation_.value() + 1);
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
            << "Monitor now " << adapter.monitor_status << "   Capability " << platform::wireless::capability_label(adapter.monitor_mode) << '\n'
            << "Injection " << platform::wireless::capability_label(adapter.injection) << '\n'
            << adapter.capability_note;
    }
    if (!adapter_error_.empty()) out << '\n' << adapter_error_;
    adapter_detail_subject_.set(out.str().c_str());
}

} // namespace viewmodel
