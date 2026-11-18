#pragma once

#include <limits>

#include "keyboard_listener.hpp"

namespace Keyboard {

inline KeyboardListener& listener() {
#ifdef _WIN32
    static KeyboardListener instance(KeyboardManager::instance(), true);
#else
    static KeyboardListener instance;
#endif
    return instance;
}

namespace detail {

inline void send_steps(KeyboardManager& manager, const std::vector<ScanCodes>& steps,
                       bool do_press, bool do_release) {
    for (const auto& step : steps) {
        std::size_t pressed = 0;
        try {
            if (do_press)
                for (const int code : step) {
                    manager.press(code);
                    ++pressed;
                }
            if (do_release) {
                if (!do_press) pressed = step.size();
                while (pressed != 0) {
                    manager.release(step[pressed - 1]);
                    --pressed;
                }
            }
        } catch (...) {
            if (do_press)
                while (pressed != 0) {
                    try { manager.release(step[--pressed]); } catch (...) {}
                }
            throw;
        }
    }
}

} // namespace detail

inline void send(std::wstring_view expression, bool do_press = true, bool do_release = true) {
    auto manager = KeyboardManager::instance();
    std::vector<ScanCodes> steps;
    for (const auto& step : parse_hotkey(expression, *manager)) {
        ScanCodes codes;
        for (const auto& alternatives : step) {
            const auto choice = std::find_if(alternatives.begin(), alternatives.end(), [&](int code) {
                return std::find(codes.begin(), codes.end(), code) == codes.end();
            });
            if (choice == alternatives.end())
                throw std::invalid_argument("Hotkey step repeats a scan code");
            codes.push_back(*choice);
        }
        steps.push_back(std::move(codes));
    }
    detail::send_steps(*manager, steps, do_press, do_release);
}

inline void send(int code, bool do_press = true, bool do_release = true) {
    detail::send_steps(*KeyboardManager::instance(), {{code}}, do_press, do_release);
}

inline void press(std::wstring_view keys) { send(keys, true, false); }
inline void release(std::wstring_view keys) { send(keys, false, true); }
inline void press(int code) { KeyboardManager::instance()->press(code); }
inline void release(int code) { KeyboardManager::instance()->release(code); }

inline bool is_pressed(std::wstring_view keys) { return listener().is_pressed(keys); }
inline bool is_pressed(int code) { return listener().is_pressed(code); }

inline int hook(EventHandler callback, bool suppress = false) {
    return listener().hook(std::move(callback), suppress);
}

inline int hook_key(std::wstring_view key, EventHandler callback, bool suppress = false) {
    return listener().hook_key(key, std::move(callback), suppress);
}

inline int on_press(std::function<void(const KeyboardEvent&)> callback) {
    return hook([callback = std::move(callback)](const KeyboardEvent& event) {
        if (event.type == EventType::Down) callback(event);
        return false;
    });
}

inline int on_release(std::function<void(const KeyboardEvent&)> callback) {
    return hook([callback = std::move(callback)](const KeyboardEvent& event) {
        if (event.type == EventType::Up) callback(event);
        return false;
    });
}

inline int block_key(std::wstring_view key) { return listener().block_key(key); }
inline void unhook(int id) { listener().remove_handler(id); }
inline void unhook_all() { listener().clear_handlers(); }

inline int add_hotkey(std::wstring_view keys, std::function<void()> callback,
                      std::chrono::milliseconds timeout = std::chrono::seconds(1),
                      bool trigger_on_release = false) {
    return listener().add_hotkey(keys, std::move(callback), timeout, trigger_on_release);
}

inline int add_hotkey(int code, std::function<void()> callback, bool trigger_on_release = false) {
    return listener().add_hotkey(parse_hotkey(code), std::move(callback),
                                 std::chrono::seconds(1), trigger_on_release);
}

inline void remove_hotkey(int id) { unhook(id); }

inline void wait(std::wstring_view keys, bool trigger_on_release = false) {
    listener().wait(keys, trigger_on_release);
}

inline KeyboardEvent read_event(bool suppress = false) { return listener().read_event(suppress); }

inline std::wstring read_key(bool suppress = false) {
    return listener().read_event(suppress, EventType::Down).name;
}

inline void write(std::wstring_view text, std::chrono::milliseconds delay = {}, bool exact =
#ifdef __linux__
                  false
#else
                  true
#endif
) {
    if (delay.count() < 0)
        throw std::invalid_argument("Keyboard write delay cannot be negative");
    auto manager = KeyboardManager::instance();
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto first = i;
        auto point = static_cast<unsigned long>(text[i]);
        if (point >= 0xd800 && point <= 0xdbff && i + 1 < text.size()) {
            const auto low = static_cast<unsigned long>(text[i + 1]);
            if (low >= 0xdc00 && low <= 0xdfff) {
                point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00;
                ++i;
            }
        }
        if (point == L'\n' || point == L'\r' || point == L'\t') {
            send(point == L'\t' ? L"tab" : L"enter");
            if (point == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
                ++i;
        } else {
            KeyMappings mappings;
            if (!exact) {
                try { mappings = manager->map_name(text.substr(first, i - first + 1)); }
                catch (const std::invalid_argument&) {}
            }
            const auto mapping = std::find_if(mappings.begin(), mappings.end(), [](const auto& entry) {
                return std::all_of(entry.modifiers.begin(), entry.modifiers.end(), [](const auto& modifier) {
                    return modifier == L"shift" || modifier == L"alt" || modifier == L"alt gr";
                });
            });
            if (mapping == mappings.end()) {
                manager->type_unicode(point);
            } else {
                ScanCodes codes;
                for (const auto& modifier : mapping->modifiers)
                    codes.push_back(manager->map_name(modifier).front().scan_code);
                codes.push_back(mapping->scan_code);
                detail::send_steps(*manager, {std::move(codes)}, true, true);
            }
        }
        if (delay.count())
            std::this_thread::sleep_for(delay);
    }
}

inline void play(const std::vector<KeyboardEvent>& events, double speed_factor = 1) {
    if (!(speed_factor >= 0 && speed_factor <= (std::numeric_limits<double>::max)()))
        throw std::invalid_argument("Invalid keyboard playback speed");
    auto manager = KeyboardManager::instance();
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        if (i && speed_factor > 0 && event.time > events[i - 1].time) {
            const std::chrono::duration<double> interval = event.time - events[i - 1].time;
            std::this_thread::sleep_for(interval / speed_factor);
        }
        if (event.type == EventType::Down) manager->press(event.scan_code);
        else manager->release(event.scan_code);
    }
}

} // namespace Keyboard
