#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace fixture {
struct TemporaryOutput {
    std::filesystem::path path;

    explicit TemporaryOutput(const std::string& suffix)
        : path(std::filesystem::temp_directory_path() / ("mediacap-" + std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()) + suffix)) {}

    ~TemporaryOutput() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
}
