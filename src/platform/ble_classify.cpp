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
uint16_t le16(const uint8_t* data) { return uint16_t(data[0]) | (uint16_t(data[1]) << 8); }

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
std::string hex_bytes(const uint8_t* data, size_t length) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(length * 2);
    for (size_t i = 0; i < length; ++i) {
        result += digits[data[i] >> 4];
        result += digits[data[i] & 15];
    }
    return result;
}
void add_field(std::string& target, const std::string& key, const uint8_t* data, size_t length) {
    if (target.size() > 500) return;
    if (!target.empty()) target += ',';
    target += key + ':' + hex_bytes(data, length);
}
std::string uuid16(uint16_t value) {
    char text[37];
    std::snprintf(text, sizeof(text), "0000%04X-0000-1000-8000-00805F9B34FB", value);
    return text;
}
std::string uuid128(const uint8_t* little_endian) {
    const auto bytes = hex_bytes(little_endian, 16);
    std::string result;
    result.reserve(36);
    for (int i = 15; i >= 0; --i) {
        if (i == 11 || i == 9 || i == 7 || i == 5) result += '-';
        result += bytes[2 * i];
        result += bytes[2 * i + 1];
    }
    return result;
}
} // namespace

BluetoothObservation decode_report(std::string address, std::string address_type,
                                   int rssi, const std::string& raw_hex) {
    BluetoothObservation item;
    item.address = std::move(address);
    item.address_type = std::move(address_type);
    item.rssi = rssi;
    if (raw_hex.size() > 510 || raw_hex.size() % 2) return item;
    std::vector<uint8_t> bytes;
    bytes.reserve(raw_hex.size() / 2);
    for (size_t i = 0; i < raw_hex.size(); i += 2) {
        if (!std::isxdigit(static_cast<unsigned char>(raw_hex[i])) ||
            !std::isxdigit(static_cast<unsigned char>(raw_hex[i + 1]))) return item;
        unsigned value{};
        if (std::sscanf(raw_hex.c_str() + i, "%2x", &value) != 1) return item;
        bytes.push_back(static_cast<uint8_t>(value));
    }
    item.raw_ad = hex_bytes(bytes.data(), bytes.size());
    for (size_t i = 0; i < bytes.size();) {
        const size_t size = bytes[i++];
        if (!size) break;
        if (i + size > bytes.size()) break;
        const auto type = bytes[i++];
        const auto* value = bytes.data() + i;
        const auto length = size - 1;
        if ((type == 0x08 || type == 0x09) && item.name.empty()) {
            for (size_t j = 0; j < length && item.name.size() < 48; ++j) {
                const auto c = value[j];
                item.name += c >= 32 && c < 127 ? static_cast<char>(c) : ' ';
            }
        } else if (type == 0xff && length >= 2) {
            const auto company = le16(value);
            char key[5];
            std::snprintf(key, sizeof(key), "%04X", company);
            add_field(item.manufacturer_data, key, value + 2, length - 2);
        } else if (type == 0x16 && length >= 2) {
            add_field(item.service_data, uuid16(le16(value)), value + 2, length - 2);
        } else if (type == 0x21 && length >= 16) {
            add_field(item.service_data, uuid128(value), value + 16, length - 16);
        } else if ((type == 0x02 || type == 0x03) && length >= 2) {
            for (size_t j = 0; j + 1 < length; j += 2) {
                if (!item.uuids.empty()) item.uuids += ',';
                item.uuids += uuid16(le16(value + j));
            }
        } else if ((type == 0x06 || type == 0x07) && length >= 16) {
            for (size_t j = 0; j + 15 < length; j += 16) {
                if (!item.uuids.empty()) item.uuids += ',';
                item.uuids += uuid128(value + j);
            }
        } else {
            char key[5];
            std::snprintf(key, sizeof(key), "%04X", type);
            add_field(item.advertising_data, key, value, length);
        }
        i += length;
    }
    return item;
}

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
