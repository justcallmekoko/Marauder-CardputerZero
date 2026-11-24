/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#pragma once

#include "system_diagnostics.h"
#include "system_info_model.h"
#include "wireless_scan.h"

#include <cstddef>
#include <string>
#include <vector>

namespace viewmodel::format {

std::string clip_for_display(const std::string& value, std::size_t max_columns);
std::size_t display_width(const std::string& value);
std::string menu_row_label(const std::string& label, bool available);

std::string system_info_page(const platform::diagnostics::Snapshot& snapshot,
                             const model::SystemInfoModel& navigation);

std::string wifi_scan_page(const std::string& interface_name,
                           const std::vector<platform::wireless::WifiNetwork>& networks,
                           const std::string& error,
                           bool running,
                           bool details,
                           std::size_t page_index);

} // namespace viewmodel::format
