#include "output_path.hpp"
#include "../cli.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace mccli {
OutputPath::OutputPath(const std::string& name, bool overwrite) : path_(std::filesystem::u8path(name)) {
    const auto status = std::filesystem::symlink_status(path_);
    if (std::filesystem::exists(status)) {
        if (!std::filesystem::is_regular_file(status)) throw UsageError("Output must be a regular file: " + name);
        if (!overwrite) throw UsageError("Output already exists; use --overwrite: " + name);
        return;
    }
#ifdef _WIN32
    HANDLE file = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create output file: " + name);
    CloseHandle(file);
#else
    const int file = open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (file < 0) throw std::runtime_error("Cannot create output file: " + name);
    close(file);
#endif
    reserved_ = true;
}

void OutputPath::remove_empty_reservation() noexcept {
    if (!reserved_) return;
    std::error_code error;
    if (std::filesystem::file_size(path_, error) == 0 && !error) std::filesystem::remove(path_, error);
}

OutputPath::~OutputPath() { remove_empty_reservation(); }

void require_distinct_paths(const OutputPath* first, const OutputPath* second) {
    if (first && second && std::filesystem::equivalent(first->path(), second->path()))
        throw UsageError("Recording, video frames, and audio samples need different output paths.");
}
}
