// SPDX-License-Identifier: AGPL-3.0-only
#include "censor_text.h"
#include <algorithm>
#include <cctype>

namespace model::censor {
namespace {
bool hex(char ch) { return std::isxdigit(static_cast<unsigned char>(ch)) != 0; }
bool looks_like_mac(const std::string& value) {
    if (value.size() != 17) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if ((i + 1) % 3 == 0) { if (value[i] != ':' && value[i] != '-') return false; }
        else if (!hex(value[i])) return false;
    }
    return true;
}
}

std::string name(const std::string& value) {
    if (value.empty() || value == "[hidden/unknown]" || value == "Unknown" || value == "?") return value;
    std::string result = value;
    std::size_t total = 0;
    for (unsigned char ch : result) if ((ch & 0xc0) != 0x80) ++total;
    const auto visible = total <= 3 ? (total > 1 ? 1u : 0u) : 3u;
    std::size_t characters = 0;
    for (std::size_t i = 0; i < result.size();) {
        const auto lead = static_cast<unsigned char>(result[i]);
        const std::size_t bytes = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 :
            (lead & 0xf0) == 0xe0 ? 3 : (lead & 0xf8) == 0xf0 ? 4 : 1;
        if (characters >= visible) {
            result.replace(i, std::min(bytes, result.size() - i), "X");
            ++i;
        } else i += std::min(bytes, result.size() - i);
        ++characters;
    }
    return result;
}

std::string mac(const std::string& value) {
    std::string result = value;
    std::size_t digits = 0;
    for (char& ch : result) {
        if (!hex(ch)) continue;
        if (digits >= 4) ch = 'X';
        ++digits;
    }
    return result;
}

std::string identifier(const std::string& value) {
    return looks_like_mac(value) ? mac(value) : name(value);
}

std::string coordinate(const std::string& value) {
    std::string result = value;
    const auto decimal = result.find('.');
    if (decimal == std::string::npos) return name(value);
    for (std::size_t i = decimal + 1; i < result.size(); ++i)
        if (std::isdigit(static_cast<unsigned char>(result[i]))) result[i] = 'X';
    return result;
}

std::string capture_name(const std::string& value) {
    std::string result = value;
    const auto prefix = result.find("ap_") != std::string::npos ? result.find("ap_") + 3 :
        result.find("station_") != std::string::npos ? result.find("station_") + 8 : std::string::npos;
    if (prefix == std::string::npos || result.size() < prefix + 13 || result[prefix + 12] != '_') return result;
    for (std::size_t i = prefix + 4; i < prefix + 12; ++i)
        if (hex(result[i])) result[i] = 'X';
    return result;
}

std::string macs_in_text(const std::string& value) {
    std::string result = value;
    for (std::size_t i = 0; i + 17 <= result.size(); ++i) {
        if (looks_like_mac(result.substr(i, 17))) {
            result.replace(i, 17, mac(result.substr(i, 17)));
            i += 16;
        }
    }
    return result;
}
}
