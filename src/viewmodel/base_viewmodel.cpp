/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "system_diagnostics.h"

namespace viewmodel {
namespace {
int page_to_int(model::AppPage page) { return static_cast<int>(page); }
}

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      dashboard_summary_subject_("Collecting system information..."),
      diagnostics_detail_subject_("Collecting system information..."),
      diagnostics_generation_subject_(0),
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
lv_subject_t* BaseViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }
bool BaseViewModel::is_dark_mode() const { return model_.dark_mode(); }
void BaseViewModel::set_dark_mode(bool enabled) { model_.set_dark_mode(enabled); publish_navigation(); }
void BaseViewModel::toggle_dark_mode() { model_.toggle_dark_mode(); publish_navigation(); }
model::AppPage BaseViewModel::current_page() const { return model_.current_page(); }
void BaseViewModel::show_dashboard_page() { model_.set_current_page(model::AppPage::Dashboard); publish_navigation(); }
void BaseViewModel::show_diagnostics_page() { model_.set_current_page(model::AppPage::Diagnostics); publish_navigation(); }
void BaseViewModel::toggle_page() { model_.toggle_page(); publish_navigation(); }

void BaseViewModel::refresh_diagnostics() {
    const auto snapshot = platform::diagnostics::collect();
    const auto summary = platform::diagnostics::dashboard_summary(snapshot);
    const auto details = platform::diagnostics::detail_text(snapshot);
    dashboard_summary_subject_.set(summary.c_str());
    diagnostics_detail_subject_.set(details.c_str());
    diagnostics_generation_subject_.set(diagnostics_generation_subject_.value() + 1);
}

void BaseViewModel::request_quit() { quit_requested_subject_.set(true); }

void BaseViewModel::publish_navigation() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

} // namespace viewmodel
