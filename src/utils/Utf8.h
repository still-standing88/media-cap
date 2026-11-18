#pragma once

#include <cstring>
#include <string>
#include <string_view>

namespace MediaCap::Utf8 {
inline bool continuation(unsigned char byte) noexcept { return (byte & 0xc0) == 0x80; }

// Returns the byte width of a Unicode scalar value, or zero for malformed input.
inline size_t width(std::string_view input) noexcept {
    if (input.empty()) return 0;
    const auto lead = static_cast<unsigned char>(input[0]);
    if (lead < 0x80) return 1;
    const size_t count = lead >= 0xc2 && lead <= 0xdf ? 2
        : lead >= 0xe0 && lead <= 0xef ? 3 : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
    if (!count || input.size() < count) return 0;
    const auto second = static_cast<unsigned char>(input[1]);
    if (!continuation(second) || (lead == 0xe0 && second < 0xa0) ||
        (lead == 0xed && second >= 0xa0) || (lead == 0xf0 && second < 0x90) ||
        (lead == 0xf4 && second >= 0x90)) return 0;
    for (size_t i = 2; i < count; ++i) if (!continuation(input[i])) return 0;
    return count;
}

inline bool valid(std::string_view input) noexcept {
    while (!input.empty()) {
        const auto count = width(input);
        if (!count || input.front() == '\0') return false;
        input.remove_prefix(count);
    }
    return true;
}

template<size_t N> std::string_view bounded(const char (&src)[N]) noexcept {
    const auto* end = static_cast<const char*>(std::memchr(src, '\0', N));
    return {src, end ? static_cast<size_t>(end - src) : N};
}

template<class Append>
inline void display(std::string_view input, size_t capacity, Append append) {
    while (!input.empty() && input.front() != '\0') {
        const auto count = width(input);
        const auto scalar = count ? input.substr(0, count) : std::string_view("\xef\xbf\xbd", 3);
        if (scalar.size() > capacity) return;
        append(scalar);
        capacity -= scalar.size();
        input.remove_prefix(count ? count : 1);
    }
}

template<size_t N>
inline void copy(char (&dest)[N], std::string_view input) noexcept {
    size_t used = 0;
    display(input, N - 1, [&](std::string_view scalar) noexcept {
        std::memcpy(dest + used, scalar.data(), scalar.size());
        used += scalar.size();
    });
    std::memset(dest + used, 0, N - used);
}

inline std::string clean(std::string_view input, size_t capacity = std::string_view::npos) {
    std::string result;
    display(input, capacity, [&](std::string_view scalar) { result.append(scalar); });
    return result;
}

inline char lower_ascii(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}
}
