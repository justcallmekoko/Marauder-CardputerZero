// SPDX-License-Identifier: AGPL-3.0-only
#include "raw_wifi.h"
#include <algorithm>
#include <cstdio>

namespace platform::wireless {
namespace {
unsigned le16(const uint8_t* p) { return unsigned(p[0]) | (unsigned(p[1]) << 8); }
unsigned le32(const uint8_t* p) { return le16(p) | (le16(p + 2) << 16); }
std::size_t aligned(std::size_t n, std::size_t alignment) { return (n + alignment - 1) & ~(alignment - 1); }
bool individual(const uint8_t* mac) {
    if (mac[0] & 1) return false;
    return std::any_of(mac, mac + 6, [](uint8_t b) { return b != 0; });
}
std::string address(const uint8_t* mac) {
    char text[18];
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return text;
}
}

bool parse_monitor_frame(const uint8_t* bytes, std::size_t size, FrameObservation& out) {
    out = {};
    if (!bytes || size < 8 || bytes[0] != 0) return false;
    const auto header = le16(bytes + 2);
    if (header < 8 || header + 24 > size) return false;
    std::size_t bitmap_end = 4;
    unsigned present = 0;
    do {
        if (bitmap_end + 4 > header) return false;
        present = le32(bytes + bitmap_end);
        bitmap_end += 4;
    } while (present & (1u << 31));
    const unsigned first = le32(bytes + 4);
    std::size_t cursor = bitmap_end;
    constexpr unsigned widths[] = {8, 1, 1, 4, 2, 1};
    constexpr unsigned alignments[] = {8, 1, 1, 2, 2, 1};
    for (unsigned bit = 0; bit <= 5; ++bit) {
        if (!(first & (1u << bit))) continue;
        cursor = aligned(cursor, alignments[bit]);
        if (cursor + widths[bit] > header) return false;
        if (bit == 3) out.frequency_mhz = static_cast<int>(le16(bytes + cursor));
        if (bit == 5) { out.rssi = static_cast<int8_t>(bytes[cursor]); out.has_rssi = true; }
        cursor += widths[bit];
    }
    const auto* frame = bytes + header;
    const auto length = size - header;
    const unsigned control = le16(frame);
    const unsigned type = (control >> 2) & 3;
    const unsigned subtype = (control >> 4) & 15;
    const bool to_ds = (control & 0x0100) != 0;
    const bool from_ds = (control & 0x0200) != 0;
    const uint8_t *bssid = nullptr, *client = nullptr;
    if (type == 0 && (subtype == 8 || subtype == 5)) {
        if (length < 36) return false;
        bssid = frame + 16;
        out.beacon = true;
        out.security = (le16(frame + 34) & 0x10) ? "Protected" : "Open";
        for (std::size_t pos = 36; pos + 2 <= length;) {
            const unsigned tag = frame[pos], count = frame[pos + 1];
            pos += 2;
            if (pos + count > length) break;
            if (tag == 0 && count && out.ssid.empty()) {
                for (unsigned n = 0; n < count && n < 32; ++n) {
                    const auto c = frame[pos + n];
                    out.ssid += c >= 32 && c < 127 ? static_cast<char>(c) : '?';
                }
            }
            if (tag == 48) out.security = "WPA2/3";
            pos += count;
        }
    } else if (type == 2) {
        if (to_ds == from_ds) return false;
        bssid = to_ds ? frame + 4 : frame + 10;
        client = to_ds ? frame + 10 : frame + 4;
        out.client_transmitted = to_ds;
    } else if (type == 0 && (subtype == 0 || subtype == 2)) {
        bssid = frame + 16;
        client = frame + 10;
        out.client_transmitted = true;
    } else return false;
    if (!bssid || !individual(bssid)) return false;
    out.bssid = address(bssid);
    if (client && individual(client) && address(client) != out.bssid) out.client = address(client);
    return true;
}
}
