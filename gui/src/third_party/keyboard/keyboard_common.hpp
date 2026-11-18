#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "wstring_util.hpp"

#if defined(_WIN32)
#include "canonical_names_windows.hpp"
#elif defined(__linux__)
#include "canonical_names_linux.hpp"
#elif defined(__APPLE__)
#include "canonical_names_darwin.hpp"
#else
#error Keyboard++ supports Windows, Linux and macOS.
#endif

namespace Keyboard {

enum class EventType {
    Down, Up,
#if !defined(__linux__) && !defined(KEY_DOWN) && !defined(KEY_UP)
    KEY_DOWN = Down, KEY_UP = Up
#endif
};

struct KeyboardEvent {
    EventType type;
    int scan_code;
    std::wstring name;
    std::string device;
    std::vector<std::wstring> modifiers;
    std::chrono::system_clock::time_point time;
    bool is_keypad;
    bool is_repeat = false;

    KeyboardEvent(EventType type, int code, std::wstring name = {},
                  std::string device = {}, bool keypad = false)
        : type(type), scan_code(code), name(std::move(name)), device(std::move(device)),
          time(std::chrono::system_clock::now()), is_keypad(keypad) {}

    template<typename Result = double>
    Result get_time() const {
        if constexpr (std::is_arithmetic_v<Result>) {
            return std::chrono::duration_cast<std::chrono::duration<Result>>(
                time.time_since_epoch()).count();
        } else {
            static_assert(std::is_same_v<Result, std::string> || std::is_same_v<Result, char*>,
                          "Use a numeric type or std::string for a timestamp");
            const auto seconds = std::chrono::system_clock::to_time_t(time);
            std::tm local{};
#ifdef _WIN32
            const bool valid = localtime_s(&local, &seconds) == 0;
#else
            const bool valid = localtime_r(&seconds, &local) != nullptr;
#endif
            thread_local char buffer[32];
            buffer[0] = '\0';
            if (valid)
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
            return buffer;
        }
    }

    bool operator==(const KeyboardEvent& other) const {
        return type == other.type && scan_code == other.scan_code && name == other.name;
    }
};

using EventHandler = std::function<bool(const KeyboardEvent&)>;

struct KeyMapping {
    int scan_code;
    std::vector<std::wstring> modifiers;

    bool operator==(const KeyMapping& other) const {
        return scan_code == other.scan_code && modifiers == other.modifiers;
    }
};
using KeyMappings = std::vector<KeyMapping>;

inline std::wstring get_name(std::wstring key) {
    if (key.size() > 1) {
        wstrutil::trim(key);
        if (key.size() > 1) {
            key = wstrutil::to_lower(key);
            std::replace(key.begin(), key.end(), L'_', L' ');
        }
    }
    static const auto aliases = [] {
        std::unordered_map<std::wstring_view, std::wstring_view> result;
        result.reserve(Canonical_Names.size());
        for (const auto& entry : Canonical_Names)
            result.emplace(entry.first, entry.second);
        return result;
    }();
    const auto match = aliases.find(key);
    if (match != aliases.end())
        key = match->second;
#ifdef __APPLE__
    if (key == L"windows") return L"command";
    if (key == L"left windows" || key == L"left command") return L"left command";
    if (key == L"right windows" || key == L"right command") return L"right command";
    if (key == L"alt gr") return L"alt";
#endif
    return key;
}

inline const std::unordered_set<std::wstring> sided_modifiers = {
#ifdef __APPLE__
    L"command",
#else
    L"windows",
#endif
    L"alt", L"shift", L"ctrl"
};

inline const std::unordered_set<std::wstring> all_modifiers = [] {
    std::unordered_set<std::wstring> result{L"alt gr"};
    for (const auto& modifier : sided_modifiers) {
        result.insert(modifier);
        result.insert(L"left " + modifier);
        result.insert(L"right " + modifier);
    }
    return result;
}();

inline void init_modifiers() {}

inline bool is_modifier(std::wstring_view name) {
    return all_modifiers.count(get_name(std::wstring(name))) != 0;
}

namespace detail {

inline std::wstring_view unside(std::wstring_view name) {
    if (name.substr(0, 5) == L"left ") return name.substr(5);
    if (name.substr(0, 6) == L"right ") return name.substr(6);
    return name;
}

inline void add_mapping(KeyMappings& mappings, int code, std::vector<std::wstring> modifiers) {
    KeyMapping mapping{code, std::move(modifiers)};
    if (std::find(mappings.begin(), mappings.end(), mapping) == mappings.end())
        mappings.push_back(std::move(mapping));
}

inline bool matches_name(std::wstring_view name, const std::wstring& candidate) {
    return name == candidate ||
           (all_modifiers.count(candidate) != 0 && name == unside(candidate));
}

inline std::vector<unsigned short> utf16(unsigned long point) {
    if (point <= 0xffff && !(point >= 0xd800 && point <= 0xdfff))
        return {static_cast<unsigned short>(point)};
    if (point < 0x10000 || point > 0x10ffff)
        throw std::invalid_argument("Invalid Unicode code point");
    point -= 0x10000;
    return {static_cast<unsigned short>(0xd800 + (point >> 10)),
            static_cast<unsigned short>(0xdc00 + (point & 0x3ff))};
}

} // namespace detail
} // namespace Keyboard
