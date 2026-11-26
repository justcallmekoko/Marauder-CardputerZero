// SPDX-License-Identifier: AGPL-3.0-only
#include "raw_wifi.h"
#include "remote_id_wifi.h"
#include "pineapple_wifi.h"
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

bool radiotap(const uint8_t* bytes, std::size_t size, std::size_t& header,
              int& frequency, int& rssi, bool& has_rssi) {
    if (!bytes || size < 8 || bytes[0] != 0) return false;
    header = le16(bytes + 2);
    if (header < 8 || header > size) return false;
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
        if (bit == 3) frequency = static_cast<int>(le16(bytes + cursor));
        if (bit == 5) { rssi = static_cast<int8_t>(bytes[cursor]); has_rssi = true; }
        cursor += widths[bit];
    }
    return true;
}
}

const char* frame_kind_label(FrameKind kind) {
    static constexpr const char* labels[] = {"Beacon", "Probe", "Deauth / disassoc", "EAPOL / PMKID",
        "Remote ID", "Pineapple", "SAE", "Data", "Association", "Authentication", "Action", "Control", "Other"};
    const auto index = static_cast<unsigned>(kind);
    return index < static_cast<unsigned>(FrameKind::Count) ? labels[index] : "Other";
}
uint32_t frame_kind_bit(FrameKind kind) { return 1u << static_cast<unsigned>(kind); }
bool monitor_header(const uint8_t* bytes, std::size_t size, std::size_t& header,
                    int& frequency, int& rssi, bool& has_rssi) {
    return radiotap(bytes, size, header, frequency, rssi, has_rssi);
}

bool parse_frame_summary(const uint8_t* bytes, std::size_t size, FrameSummary& out) {
    out = {};
    std::size_t header{};
    if (!radiotap(bytes, size, header, out.frequency_mhz, out.rssi, out.has_rssi) || size - header < 10)
        return false;
    const auto* frame = bytes + header;
    const auto length = size - header;
    const unsigned control = le16(frame);
    if ((control & 3) != 0) return false;
    const unsigned type = (control >> 2) & 3;
    const unsigned subtype = (control >> 4) & 15;
    if (type == 0 || type == 2) {
        if (length < 24 || (type == 2 && (control & 0x0300) == 0x0300 && length < 30)) return false;
        out.receiver = address(frame + 4);
        out.sender = address(frame + 10);
        if (type == 2) {
            out.type = subtype == 4 || subtype == 12 ? "Null" : subtype >= 8 ? "QoS data" : "Data";
            out.kind = FrameKind::Data;
            const bool protected_payload = (control & 0x4000) != 0;
            const std::size_t payload = 24 + ((control & 0x0300) == 0x0300 ? 6 : 0) +
                ((subtype & 8) ? 2 : 0);
            if (!protected_payload && payload + 8 <= length &&
                frame[payload] == 0xAA && frame[payload + 1] == 0xAA && frame[payload + 2] == 0x03 &&
                frame[payload + 6] == 0x88 && frame[payload + 7] == 0x8E) {
                out.type = "EAPOL";
                out.kind = FrameKind::Eapol;
                // PMKID KDE in the EAPOL key data: vendor IE, OUI 00:0F:AC, type 4.
                // Check the bounded key-data region; payload bytes are never retained.
                const auto eapol = payload + 8;
                if (eapol + 99 <= length && frame[eapol + 1] == 3) {
                    const auto key_start = eapol + 99;
                    const auto key_length = (unsigned(frame[eapol + 97]) << 8) | frame[eapol + 98];
                    const auto end = std::min(length, key_start + key_length);
                    for (auto pos = key_start; pos + 22 <= end;) {
                        const auto item_length = frame[pos + 1];
                        if (pos + 2 + item_length > end) break;
                        if (frame[pos] == 221 && item_length >= 20 && frame[pos + 2] == 0 &&
                            frame[pos + 3] == 0x0F && frame[pos + 4] == 0xAC && frame[pos + 5] == 4)
                            { out.type = "PMKID"; break; }
                        pos += 2 + item_length;
                    }
                }
            }
        } else {
            static constexpr const char* names[] = {"Assoc req", "Assoc resp", "Reassoc req", "Reassoc resp",
                "Probe req", "Probe resp", "Timing", "Reserved", "Beacon", "ATIM", "Disassoc",
                "Auth", "Deauth", "Action", "Action", "Reserved"};
            out.type = names[subtype];
            out.kind = subtype == 8 ? FrameKind::Beacon :
                (subtype == 4 || subtype == 5) ? FrameKind::Probe :
                (subtype == 10 || subtype == 12) ? FrameKind::Deauth :
                (subtype <= 3) ? FrameKind::Association :
                (subtype == 13 || subtype == 14) ? FrameKind::Action :
                subtype == 11 ? FrameKind::Authentication : FrameKind::Other;
            if (subtype == 11 && length >= 26 && le16(frame + 24) == 3) {
                out.type = "SAE auth";
                out.kind = FrameKind::Sae;
            }
        }
    } else if (type == 1) {
        out.receiver = address(frame + 4);
        if (length >= 16 && subtype != 12 && subtype != 13) out.sender = address(frame + 10);
        out.type = subtype == 13 ? "ACK" : subtype == 12 ? "CTS" : subtype == 11 ? "RTS" : "Control";
        out.kind = FrameKind::Control;
    } else return false;
    if (type == 0 && (subtype == 8 || subtype == 13)) {
        platform::remote_id::Packet remote;
        if (platform::remote_id::decode_monitor_frame(bytes, size, remote)) {
            out.type = "Remote ID";
            out.kind = FrameKind::RemoteId;
        }
    }
    if (type == 0 && subtype == 8 && out.kind == FrameKind::Beacon) {
        PineappleObservation candidate;
        if (detect_pineapple_beacon(bytes, size, candidate)) {
            out.type = "Pineapple";
            out.kind = FrameKind::Pineapple;
        }
    }
    return true;
}

bool frame_matches_target(const FrameSummary& frame, std::string_view mac, int frequency_mhz) {
    return !mac.empty() && (frame.sender == mac || frame.receiver == mac) &&
           (!frequency_mhz || !frame.frequency_mhz || frame.frequency_mhz == frequency_mhz);
}

bool parse_monitor_frame(const uint8_t* bytes, std::size_t size, FrameObservation& out) {
    out = {};
    std::size_t header{};
    if (!radiotap(bytes, size, header, out.frequency_mhz, out.rssi, out.has_rssi) || header + 24 > size)
        return false;
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
        out.security_info = interpret_security((le16(frame + 34) & 0x10) != 0,
                                               frame + 36, length - 36);
        out.security = out.security_info.summary;
        unsigned ds_channel{}, ht_channel{};
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
            if (tag == 3 && count == 1) ds_channel = frame[pos];
            if (tag == 61 && count >= 1) ht_channel = frame[pos];
            pos += count;
        }
        const unsigned primary = ds_channel ? ds_channel : ht_channel;
        if (primary == 14) out.advertised_frequency_mhz = 2484;
        else if (primary >= 1 && primary <= 13) out.advertised_frequency_mhz = 2407 + 5 * primary;
        else if (primary >= 30 && primary <= 177) out.advertised_frequency_mhz = 5000 + 5 * primary;
    } else if (type == 2) {
        if (to_ds == from_ds) return false;
        bssid = to_ds ? frame + 4 : frame + 10;
        client = to_ds ? frame + 10 : frame + 4;
        out.client_transmitted = to_ds;
    } else return false;
    if (!bssid || !individual(bssid)) return false;
    out.bssid = address(bssid);
    if (client && individual(client) && address(client) != out.bssid) out.client = address(client);
    return true;
}
}
