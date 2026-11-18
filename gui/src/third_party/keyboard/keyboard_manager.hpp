#pragma once

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>

#include "keyboard_common.hpp"

namespace Keyboard {

class KeyboardManager {
public:
    virtual ~KeyboardManager() = default;
    static std::shared_ptr<KeyboardManager> instance();

    virtual void press(int scan_code) = 0;
    virtual void release(int scan_code) = 0;
    virtual void type(wchar_t character) { type_unicode(static_cast<unsigned long>(character)); }
    virtual void type_unicode(unsigned long code_point) = 0;
    virtual void init() {}
    virtual bool supports_suppression() const noexcept { return false; }

    KeyMappings map_name(std::wstring_view name) {
        auto mappings = lookup_name(get_name(std::wstring(name)));
        if (mappings.empty())
            throw std::invalid_argument("Unknown keyboard key");
        std::stable_sort(mappings.begin(), mappings.end(), [](const auto& left, const auto& right) {
            return left.modifiers.size() < right.modifiers.size();
        });
        return mappings;
    }

    // Return false from an event callback to discard input when suppression is enabled.
    void listen(const EventHandler& callback, bool suppress = false) {
        auto stop = std::make_shared<std::atomic<bool>>(false);
        listen(callback, stop, [] {}, suppress);
    }

    void listen(const std::function<bool()>& callback) {
        listen([&](const KeyboardEvent&) { return callback(); }, supports_suppression());
    }

    void free() {
        std::lock_guard<std::mutex> lock(capture_mutex_);
        if (capture_stop_)
            capture_stop_->store(true);
    }

    void listen(const EventHandler& callback, const std::shared_ptr<std::atomic<bool>>& stop,
                const std::function<void()>& ready, bool suppress) {
        if (!callback || !stop || !ready)
            throw std::invalid_argument("Keyboard capture requires a callback and stop state");
        if (suppress && !supports_suppression())
            throw std::logic_error("Keyboard event suppression is unavailable on this platform");
        {
            std::lock_guard<std::mutex> lock(capture_mutex_);
            if (capture_stop_)
                throw std::logic_error("Keyboard capture is already running on this manager");
            capture_stop_ = stop;
        }
        const auto clear = [&] {
            std::lock_guard<std::mutex> lock(capture_mutex_);
            capture_stop_.reset();
        };
        try {
            capture(callback, *stop, ready, suppress);
        } catch (...) {
            clear();
            throw;
        }
        clear();
    }

protected:
    virtual KeyMappings lookup_name(const std::wstring& name) = 0;
    virtual void capture(const EventHandler& callback, const std::atomic<bool>& stop,
                         const std::function<void()>& ready, bool suppress) = 0;

private:
    std::mutex capture_mutex_;
    std::shared_ptr<std::atomic<bool>> capture_stop_;
};

} // namespace Keyboard

#if defined(_WIN32)
#include "detail/windows.hpp"
#elif defined(__linux__)
#include "detail/linux.hpp"
#elif defined(__APPLE__)
#include "detail/darwin.hpp"
#endif

namespace Keyboard {

inline std::shared_ptr<KeyboardManager> KeyboardManager::instance() {
#if defined(_WIN32)
    static auto manager = std::make_shared<WindowsKeyboardManager>();
#elif defined(__linux__)
    static auto manager = std::make_shared<LinuxKeyboardManager>();
#elif defined(__APPLE__)
    static auto manager = std::make_shared<DarwinKeyboardManager>();
#endif
    return manager;
}

} // namespace Keyboard
