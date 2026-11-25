// SPDX-License-Identifier: AGPL-3.0-only
#include "radio_power.h"
#include "wireless_adapters.h"
#include <cstdio>
#include <sstream>

#if defined(__linux__) && !USE_DESKTOP
#include <cerrno>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace platform {
GpsSession::~GpsSession() { stop(); }

void GpsSession::stop() {
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ >= 0) {
        shutdown(socket_, SHUT_WR);
        if (child_ > 0) {
            while (waitpid(child_, nullptr, 0) < 0 && errno == EINTR) {}
        }
        close(socket_);
    }
#endif
    child_ = socket_ = -1;
    pending_.clear();
}

bool GpsSession::start(const std::string& id) {
    stop();
#if defined(__linux__) && !USE_DESKTOP
    int ends[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ends) != 0) return false;
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    if (!err) {
        err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDIN_FILENO);
        if (!err) err = posix_spawn_file_actions_adddup2(&actions, ends[1], STDOUT_FILENO);
        char program[] = "/usr/lib/marauder/gps-session";
        char* args[] = {program, const_cast<char*>(id.c_str()), nullptr};
        if (!err) err = posix_spawn(&child_, program, &actions, nullptr, args, environ);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(ends[1]);
    if (!err) { socket_ = ends[0]; return true; }
    close(ends[0]);
#endif
    return false;
}

std::string GpsSession::poll() {
    std::string state;
#if defined(__linux__) && !USE_DESKTOP
    if (socket_ < 0) return {};
    char buffer[256];
    for (;;) {
        auto n = recv(socket_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { stop(); return "Unavailable"; }
        pending_.append(buffer, n);
        std::size_t end;
        while ((end = pending_.find('\n')) != std::string::npos) {
            state = pending_.substr(0, end);
            pending_.erase(0, end + 1);
        }
    }
#endif
    return state;
}

RadioInventory read_radios(std::string id, bool enable) {
#if !defined(__linux__) || USE_DESKTOP
    return {"Device radios unavailable", {}};
#else
    std::string command = "/usr/bin/timeout 20 /usr/bin/python3 -I /usr/lib/marauder/radio-inventory";
    if (id.empty()) command += " --restore";
    else {
        if (id.size() != 64 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
            return {"Invalid device", {}};
        command += " " + id + (enable ? " on" : " off");
    }
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return {"Inventory unavailable", {}};
    RadioInventory result{"Inventory unavailable", {}};
    char buffer[2048];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        line.erase(line.find_last_not_of("\r\n") + 1);
        std::istringstream row(line);
        std::vector<std::string> fields;
        for (std::string field; std::getline(row, field, '\t');) fields.push_back(field);
        if (fields.size() == 2 && fields[0] == "status") result.message = fields[1];
        if (fields.size() == 9 && fields[0] == "radio")
            result.devices.push_back({fields[1], fields[2], fields[3], fields[4],
                                      fields[5], fields[6], fields[7], fields[8]});
    }
    if (pclose(pipe) != 0) return {"Inventory unavailable", {}};
    const auto catalog = wireless::enumerate();
    for (auto& device : result.devices) {
        if (device.kind != "WiFi") continue;
        for (const auto& adapter : catalog.adapters) {
            if (adapter.interface_name != device.name) continue;
            if (!adapter.manufacturer.empty() || !adapter.product_name.empty())
                device.product = adapter.display_name();
            device.bands = adapter.bands;
            break;
        }
    }
    return result;
#endif
}
}
