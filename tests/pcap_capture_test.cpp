// SPDX-License-Identifier: AGPL-3.0-only
#include "pcap_capture.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "marauder-pcap-writer-test.pcap";
    const std::array<uint8_t, 4> frame{{0, 0, 4, 0}};
    platform::pcap::Writer writer;
    if (!writer.open(path) || !writer.append(frame.data(), frame.size(), 123456789) || !writer.flush()) return 1;
    writer.close();
    std::ifstream file(path, std::ios::binary);
    const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    file.close();
    std::filesystem::remove(path);
    if (bytes.size() != 44 || bytes[0] != 0xd4 || bytes[1] != 0xc3 || bytes[2] != 0xb2 || bytes[3] != 0xa1 ||
        bytes[4] != 2 || bytes[6] != 4 || bytes[20] != 127 || bytes[24] != 123 ||
        bytes[28] != 0x55 || bytes[29] != 0xf8 || bytes[30] != 0x06 || bytes[31] != 0 ||
        bytes[32] != 4 || bytes[36] != 4 || bytes[40] != 0 || bytes[43] != 0) {
        std::cerr << "PCAP header or packet record invalid\n";
        return 2;
    }
    std::vector<uint8_t> beacon{0, 0, 13, 0, 0x28, 0, 0, 0, 0x6c, 0x09, 0, 0, 0xd0};
    beacon.resize(13 + 24);
    beacon[13] = 0x80;
    const uint8_t ap[6]{0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    const uint8_t station[6]{0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    for (int i = 0; i < 6; ++i) {
        beacon[13 + 4 + i] = 0xff;
        beacon[13 + 10 + i] = ap[i];
        beacon[13 + 16 + i] = ap[i];
    }
    const platform::pcap::Filter ap_filter{platform::pcap::Scope::AccessPoint, "10:20:30:40:50:60", 2412};
    const platform::pcap::Filter station_filter{platform::pcap::Scope::Station, "22:33:44:55:66:77", 2412};
    if (!platform::pcap::matches_packet(beacon.data(), beacon.size(), ap_filter) ||
        platform::pcap::matches_packet(beacon.data(), beacon.size(), station_filter) ||
        platform::pcap::matches_packet(beacon.data(), 4, ap_filter)) return 3;
    beacon[13] = 0x08;
    beacon[14] = 0x01;
    for (int i = 0; i < 6; ++i) {
        beacon[13 + 4 + i] = ap[i];
        beacon[13 + 10 + i] = station[i];
    }
    if (!platform::pcap::matches_packet(beacon.data(), beacon.size(), ap_filter) ||
        !platform::pcap::matches_packet(beacon.data(), beacon.size(), station_filter) ||
        platform::pcap::matches_packet(beacon.data(), beacon.size(),
                                      {platform::pcap::Scope::AccessPoint, "AA:BB:CC:DD:EE:FF", 2412}) ||
        platform::pcap::matches_packet(beacon.data(), beacon.size(),
                                      {platform::pcap::Scope::AccessPoint, "10:20:30:40:50:60", 2437}) ||
        !platform::pcap::matches_packet(beacon.data(), 4, {})) return 4;
    return 0;
}
