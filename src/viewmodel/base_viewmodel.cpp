/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "system_diagnostics.h"

#include <sstream>

namespace viewmodel {
namespace {
int page_to_int(model::AppPage page) { return static_cast<int>(page); }

std::string clip_for_display(const std::string& value, std::size_t max_length) {
    if (value.size() <= max_length) return value;
    if (max_length < 4) return value.substr(0, max_length);
    return value.substr(0, max_length - 3) + "...";
}
}

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      dashboard_summary_subject_("Collecting system information..."),
      diagnostics_detail_subject_("Collecting system information..."),
      diagnostics_generation_subject_(0),
      adapter_detail_subject_("Collecting wireless adapters..."),
      quit_requested_subject_(false) {
    refresh_diagnostics();
}

BaseViewModel::~BaseViewModel() = default;
lv_subject_t* BaseViewModel::title_subject() { return title_subject_.native(); }
lv_subject_t* BaseViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }
lv_subject_t* BaseViewModel::current_page_subject() { return current_page_subject_.native(); }
lv_subject_t* BaseViewModel::dashboard_summary_subject() { return dashboard_summary_subject_.native(); }
lv_subject_t* BaseViewModel::diagnostics_detail_subject() { return diagnostics_detail_subject_.native(); }
lv_subject_t* BaseViewModel::diagnostics_generation_subject() { return diagnostics_generation_subject_.native(); }
lv_subject_t* BaseViewModel::adapter_detail_subject() { return adapter_detail_subject_.native(); }
lv_subject_t* BaseViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }
bool BaseViewModel::is_dark_mode() const { return model_.dark_mode(); }
void BaseViewModel::set_dark_mode(bool enabled) { model_.set_dark_mode(enabled); publish_navigation(); }
void BaseViewModel::toggle_dark_mode() { model_.toggle_dark_mode(); publish_navigation(); }
model::AppPage BaseViewModel::current_page() const { return model_.current_page(); }
void BaseViewModel::show_dashboard_page() { model_.set_current_page(model::AppPage::Dashboard); publish_navigation(); }
void BaseViewModel::show_diagnostics_page() { model_.set_current_page(model::AppPage::Diagnostics); publish_navigation(); }
void BaseViewModel::show_adapters_page() {
    refresh_adapters();
    model_.set_current_page(model::AppPage::Adapters);
    publish_navigation();
}
void BaseViewModel::toggle_page() { model_.toggle_page(); publish_navigation(); }

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
    model_.set_selected_adapter_id(adapters_[highlighted_adapter_].stable_id);
    publish_adapter_detail();
}

void BaseViewModel::toggle_adapter_details() {
    adapter_details_expanded_ = !adapter_details_expanded_;
    publish_adapter_detail();
}

void BaseViewModel::request_quit() { quit_requested_subject_.set(true); }

void BaseViewModel::publish_navigation() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
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
            << "Read-only; radio state unchanged\n"
            << "8  SUMMARY";
    }
    adapter_detail_subject_.set(out.str().c_str());
}

} // namespace viewmodel
