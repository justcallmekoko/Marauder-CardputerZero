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
    return 0;
}
