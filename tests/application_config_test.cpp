#include "application_config.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {
std::string read_all(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      "marauder-cardputerzero-config-test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    const auto path = root / "settings.conf";

    {
        std::ofstream initial(path);
        initial << "# preserve me\n[application]\ndark_mode=no\ncustom=value\n";
    }

    app::ApplicationConfig config;
    config.dark_mode = true;
    std::string error;
    assert(app::save_application_config(path.string(), config, error));
    assert(error.empty());
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        assert(entry.path().filename().string().find("settings.conf.tmp.") != 0);
    }

    config.dark_mode = false;
    assert(app::save_application_config(path.string(), config, error));
    config.dark_mode = true;
    assert(app::save_application_config(path.string(), config, error));
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        assert(entry.path().filename().string().find("settings.conf.tmp.") != 0);
    }

    const auto saved = read_all(path);
    assert(saved.find("# preserve me") != std::string::npos);
    assert(saved.find("custom=value") != std::string::npos);
    assert(saved.find("dark_mode=yes") != std::string::npos);
    assert(saved.find("dark_mode=no") == std::string::npos);

    app::ApplicationConfig loaded;
    assert(app::load_application_config(path.string(), loaded, error));
    assert(loaded.dark_mode);

    std::filesystem::remove_all(root, ignored);
    return 0;
}
