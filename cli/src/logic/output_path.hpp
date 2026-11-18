#pragma once

#include <filesystem>
#include <string>

namespace mccli {
class OutputPath {
    std::filesystem::path path_;
    bool reserved_ = false;

public:
    OutputPath(const std::string& name, bool overwrite);
    ~OutputPath();
    OutputPath(const OutputPath&) = delete;
    OutputPath& operator=(const OutputPath&) = delete;

    const std::filesystem::path& path() const noexcept { return path_; }
    void remove_empty_reservation() noexcept;
};

void require_distinct_paths(const OutputPath* first, const OutputPath* second);
}
