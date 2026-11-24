/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "system_diagnostics.h"

#include <algorithm>
#include <sstream>
#include <system_error>

namespace viewmodel {
namespace {
int page_to_int(model::AppPage page) { return static_cast<int>(page); }

std::string clip_for_display(const std::string& value, std::size_t max_length) {
    auto next_codepoint = [&](std::size_t index) {
        const auto byte = static_cast<unsigned char>(value[index]);
        std::size_t length = byte < 0x80 ? 1 :
                             (byte & 0xe0) == 0xc0 ? 2 :
                             (byte & 0xf0) == 0xe0 ? 3 :
                             (byte & 0xf8) == 0xf0 ? 4 : 1;
        return std::min(value.size(), index + length);
    };
    std::size_t count = 0;
    for (std::size_t index = 0; index < value.size(); index = next_codepoint(index)) ++count;
    if (count <= max_length) return value;
    const std::size_t keep = max_length < 4 ? max_length : max_length - 3;
    std::size_t end = 0;
    for (std::size_t index = 0; index < keep && end < value.size(); ++index) end = next_codepoint(end);
    return value.substr(0, end) + (max_length < 4 ? "" : "...");
}
}

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      marauder_menu_generation_subject_(0),
      dashboard_summary_subject_("Collecting system information..."),
      diagnostics_detail_subject_("Collecting system information..."),
      diagnostics_generation_subject_(0),
      adapter_detail_subject_("Collecting wireless adapters..."),
      wifi_scan_subject_("SELECT AN ADAPTER FIRST"),
      quit_requested_subject_(false) {
    wifi_scan_timer_ = lv_timer_create(wifi_scan_timer_cb, 100, this);
    refresh_diagnostics();
}

BaseViewModel::~BaseViewModel() {
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
void BaseViewModel::set_dark_mode(bool enabled) { model_.set_dark_mode(enabled); publish_navigation(); }
void BaseViewModel::toggle_dark_mode() { model_.toggle_dark_mode(); publish_navigation(); }
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
bool BaseViewModel::marauder_menu_at_root() const { return marauder_menu_.depth() == 1; }
void BaseViewModel::marauder_menu_previous() {
    marauder_menu_.previous();
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}
void BaseViewModel::marauder_menu_next() {
    marauder_menu_.next();
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
}
void BaseViewModel::marauder_menu_activate() {
    const auto action = marauder_menu_.activate();
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
        case model::MarauderMenuAction::ToggleTheme:
            toggle_dark_mode();
            break;
        case model::MarauderMenuAction::None:
            break;
    }
}
bool BaseViewModel::marauder_menu_back() {
    if (!marauder_menu_.back()) return false;
    marauder_menu_generation_subject_.set(marauder_menu_generation_subject_.value() + 1);
    return true;
}

void BaseViewModel::refresh_diagnostics() {
    const auto snapshot = platform::diagnostics::collect();
    const auto summary = platform::diagnostics::dashboard_summary(snapshot);
    const auto details = platform::diagnostics::detail_text(snapshot);
    dashboard_summary_subject_.set(summary.c_str());
    diagnostics_detail_subject_.set(details.c_str());
    diagnostics_generation_subject_.set(diagnostics_generation_subject_.value() + 1);
    refresh_adapters();
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
    const std::size_t pages = std::max<std::size_t>(1, (wifi_networks_.size() + 4) / 5);
    wifi_scan_page_ = wifi_scan_page_ == 0 ? pages - 1 : wifi_scan_page_ - 1;
    publish_wifi_scan();
}

void BaseViewModel::next_wifi_scan_page() {
    const std::size_t pages = std::max<std::size_t>(1, (wifi_networks_.size() + 4) / 5);
    wifi_scan_page_ = (wifi_scan_page_ + 1) % pages;
    publish_wifi_scan();
}

void BaseViewModel::request_quit() { quit_requested_subject_.set(true); }

void BaseViewModel::wifi_scan_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<BaseViewModel*>(lv_timer_get_user_data(timer));
    if (self) self->finish_wifi_scan_if_ready();
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
    std::ostringstream out;
    if (wifi_scan_interface_.empty()) {
        out << "WIFI SCAN\n"
            << (wifi_scan_error_.empty() ? "Select an adapter on the Adapters page" : wifi_scan_error_) << "\n"
            << "4 BACK   8 ADAPTERS";
        wifi_scan_subject_.set(out.str().c_str());
        return;
    }

    const std::size_t pages = std::max<std::size_t>(1, (wifi_networks_.size() + 4) / 5);
    wifi_scan_page_ = std::min(wifi_scan_page_, pages - 1);
    out << "WIFI SCAN " << wifi_scan_interface_ << "  " << (wifi_scan_page_ + 1) << '/' << pages << '\n';
    if (wifi_scan_running_) {
        out << "SCANNING... previous results below\n";
    } else if (!wifi_scan_error_.empty()) {
        out << wifi_scan_error_ << "\n";
    } else {
        out << wifi_networks_.size() << " NETWORKS  strongest first\n";
    }

    const std::size_t begin = wifi_scan_page_ * 5;
    const std::size_t end = std::min(begin + 5, wifi_networks_.size());
    for (std::size_t index = begin; index < end; ++index) {
        const auto& network = wifi_networks_[index];
        out << network.signal_percent << "% C" << network.channel << ' '
            << clip_for_display(network.ssid, 22) << ' '
            << clip_for_display(network.security, 8) << '\n';
    }
    if (begin == end && !wifi_scan_running_ && wifi_scan_error_.empty()) out << "No networks found\n";
    wifi_scan_subject_.set(out.str().c_str());
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
            << (selected ? "  [SELECTED]" : "  [6 SELECT]") << '\n'
            << adapter.interface_name << "  " << adapter.bus_type << "  " << adapter.operstate << '\n';
        const auto product = clip_for_display(adapter.manufacturer +
                                              (adapter.manufacturer.empty() || adapter.product_name.empty() ? "" : " ") +
                                              adapter.product_name,
                                              42);
        if (!product.empty()) out << product << '\n';
        if (!adapter.vendor_id.empty()) out << "USB " << adapter.vendor_id << ':' << adapter.product_id << '\n';
        out << "Driver " << (adapter.driver.empty() ? "unknown" : adapter.driver)
            << "  PHY " << (adapter.phy_name.empty() ? "unknown" : adapter.phy_name) << '\n'
            << "MAC " << (adapter.mac_address.empty() ? "unknown" : adapter.mac_address) << '\n'
            << "8  MORE DETAILS";
    } else {
        out << "ADAPTER " << (highlighted_adapter_ + 1) << '/' << adapters_.size() << "  DETAILS\n"
            << adapter.interface_name << "  ifindex " << (adapter.ifindex.empty() ? "?" : adapter.ifindex) << '\n'
            << "ID " << clip_for_display(adapter.stable_id, 42) << '\n'
            << "MAC " << (adapter.permanent_mac ? "PERMANENT" : "ASSIGNED") << '\n'
            << "Monitor " << platform::wireless::capability_label(adapter.monitor_mode) << '\n'
            << "Injection " << platform::wireless::capability_label(adapter.injection) << '\n'
            << adapter.capability_note << '\n'
            << "8  SUMMARY";
    }
    adapter_detail_subject_.set(out.str().c_str());
}

} // namespace viewmodel
