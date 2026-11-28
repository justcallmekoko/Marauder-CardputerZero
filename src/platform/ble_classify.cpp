// SPDX-License-Identifier: AGPL-3.0-only
#include "ble_classify.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <vector>

namespace platform::ble {
namespace {
struct Field { std::string key; std::vector<uint8_t> bytes; };

std::vector<Field> fields(const std::string& text) {
    std::vector<Field> result;
    for (size_t pos = 0; pos < text.size() && result.size() < 8;) {
        const auto end = text.find(',', pos);
        const auto part = text.substr(pos, end == std::string::npos ? end : end - pos);
        const auto colon = part.find(':');
        if (colon != std::string::npos && colon && part.size() - colon - 1 <= 128 &&
            (part.size() - colon - 1) % 2 == 0) {
            Field field{part.substr(0, colon), {}};
            bool valid = true;
            for (size_t i = colon + 1; i + 1 < part.size(); i += 2) {
                unsigned byte{};
                if (std::sscanf(part.c_str() + i, "%2x", &byte) != 1 ||
                    !std::isxdigit(static_cast<unsigned char>(part[i])) ||
                    !std::isxdigit(static_cast<unsigned char>(part[i + 1]))) { valid = false; break; }
                field.bytes.push_back(static_cast<uint8_t>(byte));
            }
            if (valid) result.push_back(std::move(field));
        }
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return result;
}

bool has_pair(const std::vector<uint8_t>& data, uint8_t a, uint8_t b) {
    for (size_t i = 0; i + 1 < data.size(); ++i)
        if (data[i] == a && data[i + 1] == b) return true;
    return false;
}
bool uuid(const std::string& list, const char* value) {
    return list.find(value) != std::string::npos;
}
std::string company_vendor(unsigned id) {
    switch (id) {
        case 0x004c: return "Apple";
        case 0x09c8: return "Xuntong";
        case 0x0006: return "Microsoft";
        case 0x00e0: return "Google";
        case 0x0fba: return "Flipper Devices";
        case 0x0d53: return "Luxottica";
        default: {
            char text[24];
            std::snprintf(text, sizeof(text), "Company 0x%04X", id);
            return text;
        }
    }
}
} // namespace

const char* label(Kind kind) {
    static constexpr std::array<const char*, 6> names = {
        "Find My", "iBeacon", "Flipper", "Flock", "Meta", "Serial module"
    };
    return static_cast<unsigned>(kind) < names.size() ? names[static_cast<unsigned>(kind)] : "BLE";
}

Classification classify(const BluetoothObservation& item) {
    Classification result;
    if (item.address_type == "public" && item.manufacturer_data.empty() && item.service_data.empty() &&
        item.advertising_data.empty() && item.uuids.empty()) result.type = "Bluetooth device";
    const auto manufacturers = fields(item.manufacturer_data);
    const auto services = fields(item.service_data);
    const auto advertisements = fields(item.advertising_data);
    unsigned first_company{};
    bool meta_blocked = false, meta_match = false;
    for (const auto& field : manufacturers) {
        unsigned company{};
        if (field.key.size() != 4 || std::sscanf(field.key.c_str(), "%4x", &company) != 1) continue;
        if (!first_company) first_company = company;
        if (company == 0x004c) {
            if (field.bytes.size() >= 23 && field.bytes[0] == 0x02 && field.bytes[1] == 0x15) {
                result.matches |= bit(Kind::IBeacon);
                const auto major = (field.bytes[18] << 8) | field.bytes[19];
                const auto minor = (field.bytes[20] << 8) | field.bytes[21];
                result.detail = "iBeacon " + std::to_string(major) + ":" + std::to_string(minor);
            }
            if (!field.bytes.empty() && field.bytes[0] == 0x12)
                result.matches |= bit(Kind::FindMy);
        }
        if (company == 0x09c8 && (item.name.empty() || item.name.rfind("Penguin-", 0) == 0 ||
                                  item.name == "FS Ext Battery")) result.matches |= bit(Kind::Flock);
        if (company == 0xfd5f || company == 0xfeb7 || company == 0xfeb8 ||
            company == 0x01ab || company == 0x058e || company == 0x0d53) meta_match = true;
        if (company == 0xfd5a || company == 0xfd69 || company == 0x004c ||
            company == 0x0006 || company == 0xfef3) meta_blocked = true;
        if (has_pair(field.bytes, 0x81, 0x30) || has_pair(field.bytes, 0x82, 0x30) ||
            has_pair(field.bytes, 0x83, 0x30)) result.matches |= bit(Kind::Flipper);
    }
    for (const auto& field : advertisements) {
        if (has_pair(field.bytes, 0x81, 0x30) || has_pair(field.bytes, 0x82, 0x30) ||
            has_pair(field.bytes, 0x83, 0x30)) result.matches |= bit(Kind::Flipper);
    }
    const auto all_uuids = item.uuids + "," + item.service_data;
    if (uuid(all_uuids, "0000FD44-") || uuid(all_uuids, "15190001-12F4-C226-88ED-2AC5579F2A85"))
        result.matches |= bit(Kind::FindMy);
    static constexpr std::array<const char*, 6> meta_uuids = {
        "0000FD5F-", "0000FEB7-", "0000FEB8-", "000001AB-", "0000058E-", "00000D53-"
    };
    for (const auto* value : meta_uuids) if (uuid(all_uuids, value)) meta_match = true;
    static constexpr std::array<const char*, 4> blocked_uuids = {
        "0000FD5A-", "0000FD69-", "00000006-", "0000FEF3-"
    };
    for (const auto* value : blocked_uuids) if (uuid(all_uuids, value)) meta_blocked = true;
    if (meta_match && !meta_blocked) result.matches |= bit(Kind::Meta);
    if (item.name == "HC-03" || item.name == "HC-05" || item.name == "HC-06")
        result.matches |= bit(Kind::SerialModule);
    if (first_company) result.vendor = company_vendor(first_company);
    if (result.matches) {
        for (unsigned kind = 0; kind < static_cast<unsigned>(Kind::Count); ++kind)
            if (result.matches & bit(static_cast<Kind>(kind))) {
                result.type = label(static_cast<Kind>(kind));
                break;
            }
        result.evidence = "Advertisement signature; confirm identity";
    }
    if (result.matches & bit(Kind::SerialModule))
        result.evidence = "Name match only; not proof of a skimmer";
    if (result.matches & bit(Kind::Flipper) && result.vendor == "Unknown")
        result.vendor = "Unconfirmed";
    return result;
}
} // namespace platform::ble
