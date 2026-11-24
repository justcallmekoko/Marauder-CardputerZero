/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace app {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) {
        return !std::isspace(character);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool parse_bool(const std::string& value, bool& result) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "yes" || normalized == "true" || normalized == "1") {
        result = true;
        return true;
    }
    if (normalized == "no" || normalized == "false" || normalized == "0") {
        result = false;
        return true;
    }
    return false;
}

std::filesystem::path unique_temporary_path(const std::filesystem::path& destination) {
    static std::atomic<unsigned long> sequence{0};
#if defined(_WIN32)
    const auto process_id = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto process_id = static_cast<unsigned long>(getpid());
#endif
    auto temporary = destination;
    temporary += ".tmp." + std::to_string(process_id) + "." +
                 std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    return temporary;
}

bool sync_file(const std::filesystem::path& path, std::string& error) {
#if defined(_WIN32)
    const int descriptor = _wopen(path.c_str(), _O_WRONLY | _O_BINARY);
    if (descriptor < 0) {
        error = "could not reopen temporary config for sync";
        return false;
    }
    const int result = _commit(descriptor);
    const int sync_error = errno;
    _close(descriptor);
#else
    const int descriptor = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (descriptor < 0) {
        error = "could not reopen temporary config for sync: " +
                std::error_code(errno, std::generic_category()).message();
        return false;
    }
    const int result = fsync(descriptor);
    const int sync_error = errno;
    close(descriptor);
#endif
    if (result != 0) {
        error = "could not sync temporary config: " +
                std::error_code(sync_error, std::generic_category()).message();
        return false;
    }
    return true;
}

#if !defined(_WIN32)
bool sync_parent_directory(const std::filesystem::path& parent, std::string& error) {
    const auto directory = parent.empty() ? std::filesystem::path(".") : parent;
    const int descriptor = open(directory.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (descriptor < 0) {
        error = "could not open config directory for sync: " +
                std::error_code(errno, std::generic_category()).message();
        return false;
    }
    const int result = fsync(descriptor);
    const int sync_error = errno;
    close(descriptor);
    if (result != 0) {
        error = "could not sync config directory: " +
                std::error_code(sync_error, std::generic_category()).message();
        return false;
    }
    return true;
}
#endif

bool is_section(const std::string& line, const std::string& expected) {
    const auto normalized = trim(line);
    return normalized.size() >= 2 && normalized.front() == '[' && normalized.back() == ']' &&
           lowercase(trim(normalized.substr(1, normalized.size() - 2))) == expected;
}

bool is_key(const std::string& line, const std::string& expected) {
    const auto separator = line.find('=');
    return separator != std::string::npos &&
           lowercase(trim(line.substr(0, separator))) == expected;
}

} // namespace

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = lowercase(trim(line.substr(0, separator)));
        if (section == "application" && key == "dark_mode") {
            if (!parse_bool(line.substr(separator + 1), config.dark_mode)) {
                error = "invalid dark_mode value at line " + std::to_string(line_number);
                return false;
            }
        }
    }

    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();

    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code filesystem_error;
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "could not create parent directory: " + filesystem_error.message();
            return false;
        }
    }

    std::vector<std::string> lines;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }

    std::vector<std::string> updated;
    bool in_application_section = false;
    bool found_application_section = false;
    bool wrote_dark_mode = false;
    const std::string dark_mode_line = config.dark_mode ? "dark_mode=yes" : "dark_mode=no";

    for (const auto& current_line : lines) {
        const auto normalized = trim(current_line);
        const bool section_header = normalized.size() >= 2 && normalized.front() == '[' &&
                                    normalized.back() == ']';
        if (section_header) {
            if (in_application_section && !wrote_dark_mode) {
                updated.push_back(dark_mode_line);
                wrote_dark_mode = true;
            }
            in_application_section = is_section(current_line, "application");
            found_application_section = found_application_section || in_application_section;
        }

        if (in_application_section && is_key(current_line, "dark_mode")) {
            if (!wrote_dark_mode) {
                updated.push_back(dark_mode_line);
                wrote_dark_mode = true;
            }
            continue;
        }
        updated.push_back(current_line);
    }

    if (!wrote_dark_mode) {
        if (!updated.empty() && !updated.back().empty()) {
            updated.emplace_back();
        }
        if (!found_application_section) {
            updated.emplace_back("[application]");
        }
        updated.push_back(dark_mode_line);
    }

    const auto temporary_path = unique_temporary_path(config_path);
    {
        std::ofstream output(temporary_path, std::ios::trunc);
        if (!output) {
            error = "could not open temporary file for writing";
            return false;
        }
        for (const auto& updated_line : updated) {
            output << updated_line << '\n';
        }
        output.flush();
        if (!output) {
            error = "failed while writing temporary file";
            output.close();
            std::error_code cleanup_error;
            std::filesystem::remove(temporary_path, cleanup_error);
            return false;
        }
    }

    if (!sync_file(temporary_path, error)) {
        std::error_code cleanup_error;
        std::filesystem::remove(temporary_path, cleanup_error);
        return false;
    }

#if defined(_WIN32)
    if (!MoveFileExW(temporary_path.c_str(),
                     config_path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto windows_error = GetLastError();
        std::error_code cleanup_error;
        std::filesystem::remove(temporary_path, cleanup_error);
        error = "could not atomically replace config: Windows error " +
                std::to_string(windows_error);
        return false;
    }
#else
    std::error_code replace_error;
    std::filesystem::rename(temporary_path, config_path, replace_error);
    if (replace_error) {
        std::error_code cleanup_error;
        std::filesystem::remove(temporary_path, cleanup_error);
        error = "could not atomically replace config: " + replace_error.message();
        return false;
    }
#endif

#if !defined(_WIN32)
    if (!sync_parent_directory(parent, error)) {
        return false;
    }
#endif
    return true;
}

} // namespace app
