#include "../cli.hpp"
#include "json.hpp"
#include "../../../src/utils/Utf8.h"
#include <algorithm>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#define MC_DUP _dup
#define MC_DUP2 _dup2
#define MC_FILENO _fileno
#define MC_FDOPEN _fdopen
#define MC_CLOSE _close
#else
#include <unistd.h>
#define MC_DUP dup
#define MC_DUP2 dup2
#define MC_FILENO fileno
#define MC_FDOPEN fdopen
#define MC_CLOSE close
#endif
namespace mccli {
void check_error() {
    const auto error = MediaCap::get_last_error();
    if (error.code != MEDIACAP_ERROR_NONE) throw LibraryError(error);
}
std::string quote(const std::string& text) {
    return nlohmann::json(text).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}
std::string single_line(const std::string& text, size_t limit) {
    std::string clean;
    for (unsigned char c : text) {
        if (c < 32 || c == 127) c = ' ';
        if (c == ' ' && (clean.empty() || clean.back() == ' ')) continue;
        clean += static_cast<char>(c);
    }
    if (clean.size() > limit) {
        return MediaCap::Utf8::clean(clean, limit) + "...";
    }
    return MediaCap::Utf8::clean(clean);
}
std::string error_json(const MediaCap::Error& e) {
    return "\"code\":" + std::to_string(e.code) + ",\"component\":" + std::to_string(e.component) +
        ",\"source_type\":" + std::to_string(e.source_type) + ",\"source_index\":" + std::to_string(e.source_index) +
        ",\"session_id\":" + std::to_string(e.session_id) + ",\"native_code\":" + std::to_string(e.native_code);
}
std::string friendly_error(const MediaCap::Error& e) {
    const bool listing = e.component == MEDIACAP_ERROR_COMPONENT_DISCOVERY;
    if (e.component == MEDIACAP_ERROR_COMPONENT_ENCODER &&
        (e.code == MEDIACAP_ERROR_UNSUPPORTED || e.code == MEDIACAP_ERROR_ENCODING_FAILED ||
         e.code == MEDIACAP_ERROR_INVALID_ARGUMENT) && e.message[0])
        return single_line(e.message);
    if (e.code == MEDIACAP_ERROR_UNSUPPORTED && e.source_type == MEDIACAP_SOURCE_TYPE_CAMERA && e.message[0])
        return single_line(e.message);
    if ((e.code == MEDIACAP_ERROR_UNSUPPORTED || e.code == MEDIACAP_ERROR_INVALID_ARGUMENT) &&
        e.source_type >= MEDIACAP_SOURCE_TYPE_AUDIO_INPUT && e.source_type <= MEDIACAP_SOURCE_TYPE_AUDIO_MIX && e.message[0])
        return single_line(e.message);
    switch (e.code) {
    case MEDIACAP_ERROR_PERMISSION_DENIED: return "Capture access denied. Enable access in system privacy settings.";
    case MEDIACAP_ERROR_DEVICE_UNAVAILABLE: return "Source unavailable. Refresh the source list or close the app using it.";
    case MEDIACAP_ERROR_DEVICE_LOST: return "Source disconnected or stopped responding. Reconnect it and retry.";
    case MEDIACAP_ERROR_TIMEOUT:
        return listing && e.source_type == MEDIACAP_SOURCE_TYPE_CAMERA ? "Camera details timed out. Basic camera IDs are still available." : "Source did not respond in time. Check the device and retry.";
    case MEDIACAP_ERROR_BUFFER_OVERFLOW: return "Capture could not keep up. Lower the resolution or frame rate.";
    case MEDIACAP_ERROR_UNSUPPORTED: return "This capture mode is unavailable. Check 'MediaCap capabilities'.";
    case MEDIACAP_ERROR_ENCODING_FAILED: return "Cannot encode this format. Check 'MediaCap help codecs'.";
    case MEDIACAP_ERROR_IO: return "Cannot write capture output. Check the output folder and free space.";
    case MEDIACAP_ERROR_INVALID_ARGUMENT: return "Invalid source settings. Check the command's --help.";
    case MEDIACAP_ERROR_CAPTURE_FAILED:
        return listing ? "Cannot list sources. Check the connected devices and retry." : "Capture failed. Check the selected source and retry.";
    default: return listing ? "Source discovery failed. Retry with --verbose for details." : "Capture failed. Retry with --verbose for details.";
    }
}
static FILE* duplicate(FILE* original) {
    const int fd = MC_DUP(MC_FILENO(original));
    if (fd < 0) throw std::runtime_error("Cannot open terminal output.");
    auto* copy = MC_FDOPEN(fd, "w");
    if (!copy) { MC_CLOSE(fd); throw std::runtime_error("Cannot open terminal output."); }
    return copy;
}
static void sync_handles() {
#ifdef _WIN32
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
    SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
#endif
}
Output::Output(bool json_mode, bool verbose_mode) : json(json_mode), verbose(verbose_mode) {
    std::cout.flush(); std::cerr.flush(); std::fflush(nullptr);
    // Duplicate both first: CMD can give stdout and stderr the same console handle.
    stream_ = duplicate(stdout);
    FILE* sink = nullptr;
    try {
        errors_ = duplicate(stderr);
        if (!verbose) {
#ifdef _WIN32
            fopen_s(&sink, "NUL", "w");
#else
            sink = std::fopen("/dev/null", "w");
#endif
            if (!sink) throw std::runtime_error("Cannot configure diagnostic output.");
        }
        FILE* target = verbose ? errors_ : sink;
        if (MC_DUP2(MC_FILENO(target), MC_FILENO(stdout)) < 0 || MC_DUP2(MC_FILENO(target), MC_FILENO(stderr)) < 0)
            throw std::runtime_error("Cannot configure diagnostic output.");
        sync_handles();
        if (sink) std::fclose(sink);
    } catch (...) {
        MC_DUP2(MC_FILENO(stream_), MC_FILENO(stdout));
        if (errors_) { MC_DUP2(MC_FILENO(errors_), MC_FILENO(stderr)); std::fclose(errors_); errors_ = nullptr; }
        sync_handles();
        if (sink) std::fclose(sink);
        std::fclose(stream_); stream_ = nullptr;
        throw;
    }
}
Output::~Output() {
    std::cout.flush(); std::cerr.flush(); std::fflush(nullptr);
    if (stream_) MC_DUP2(MC_FILENO(stream_), MC_FILENO(stdout));
    if (errors_) MC_DUP2(MC_FILENO(errors_), MC_FILENO(stderr));
    sync_handles();
    if (stream_) std::fclose(stream_);
    if (errors_) std::fclose(errors_);
}
static void write_line(FILE* stream, const std::string& value) {
    if (std::fwrite(value.data(), 1, value.size(), stream) != value.size() || std::fputc('\n', stream) == EOF || std::fflush(stream) != 0)
        throw std::runtime_error("Cannot write CLI output.");
}
void Output::line(const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_line(stream_, value);
}
void Output::error(const std::string& message, const std::string& fields) {
    failed = true;
    if (json) event("error", "\"message\":" + quote(message) + (fields.empty() ? "" : "," + fields), message);
    else {
        std::lock_guard<std::mutex> lock(mutex_);
        write_line(errors_, "Error: " + single_line(message));
    }
}
void Output::warning(const MediaCap::Error& e) {
    failed = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        write_line(errors_, "Warning: " + friendly_error(e));
    }
    diagnostic(e.message);
}
void Output::diagnostic(const std::string& detail) noexcept {
    try {
        if (verbose && !detail.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            write_line(errors_, "Details: " + single_line(detail, 500));
        }
    }
    catch (...) {} // Diagnostics must never mask the original failure.
}
void Output::event(const std::string& type, const std::string& fields, const std::string& human) {
    line(json ? "{\"event\":" + quote(type) + (fields.empty() ? "" : "," + fields) + "}" : human);
}
}
