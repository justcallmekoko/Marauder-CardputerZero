// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string>

namespace model::censor {
std::string name(const std::string& value);
std::string mac(const std::string& value);
std::string identifier(const std::string& value);
std::string coordinate(const std::string& value);
std::string capture_name(const std::string& value);
std::string macs_in_text(const std::string& value);
}
