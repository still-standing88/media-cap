#pragma once

#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <system_error>

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace Keyboard {

class LinuxKeyboardManager final : public KeyboardManager {
public:
    void init() override {
        std::lock_guard<std::mutex> lock(output_mutex_);
        if (output_.value < 0)
            open_output();
    }

    void press(int code) override {
        std::lock_guard<std::mutex> lock(output_mutex_);
        emit_key(code, true);
    }

    void release(int code) override {
        std::lock_guard<std::mutex> lock(output_mutex_);
        emit_key(code, false);
    }

    void type_unicode(unsigned long point) override {
        detail::utf16(point);
        Layout layout;
        const auto unmodified_key = [&](wchar_t character) {
            const auto mappings = map_layout_name(std::wstring(1, character), layout);
            const auto entry = std::find_if(mappings.begin(), mappings.end(), [](const auto& key) {
                return key.modifiers.empty();
            });
            if (entry == mappings.end())
                throw std::runtime_error("Unicode input needs an ASCII-capable XKB layout");
            return entry->scan_code;
        };
        constexpr wchar_t hex[] = L"0123456789abcdef";
        const int start_key = unmodified_key(L'u');
        std::vector<KeyMapping> digits;
        do {
            auto choices = map_layout_name(std::wstring(1, hex[point & 0xf]), layout);
            const auto usable = std::find_if(choices.begin(), choices.end(), [](const auto& key) {
                return std::all_of(key.modifiers.begin(), key.modifiers.end(), [](const auto& modifier) {
                    return modifier == L"shift" || modifier == L"alt gr";
                });
            });
            if (usable == choices.end())
                throw std::runtime_error("Unicode input needs hex digits in the XKB layout");
            digits.push_back(*usable);
            point >>= 4;
        } while (point != 0);

        std::lock_guard<std::mutex> lock(output_mutex_);
        tap({KEY_LEFTCTRL, KEY_LEFTSHIFT, start_key});
        for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
            std::vector<int> codes;
            for (const auto& modifier : it->modifiers)
                codes.push_back(modifier == L"shift" ? KEY_LEFTSHIFT : KEY_RIGHTALT);
            codes.push_back(it->scan_code);
            tap(codes);
        }
        tap({KEY_ENTER});
    }

protected:
    KeyMappings lookup_name(const std::wstring& name) override {
        return map_layout_name(name, Layout{});
    }

    void capture(const EventHandler& callback, const std::atomic<bool>& stop,
                 const std::function<void()>& ready, bool) override {
        Layout layout;
        auto state = layout.make_state();
        Devices devices;
        discover_devices(devices);
        if (devices.empty())
            throw std::runtime_error("No readable evdev keyboard found under /dev/input");
        KeyCounts counts{};
        for (auto& device : devices)
            sync_device(device, counts, state.get(), nullptr);
        ready();
        auto next_scan = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (!stop.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= next_scan) {
                const auto previous = devices.size();
                discover_devices(devices);
                for (std::size_t i = previous; i < devices.size(); ++i)
                    sync_device(devices[i], counts, state.get(), &callback);
                next_scan = now + std::chrono::seconds(1);
            }
            std::vector<pollfd> poll_list;
            for (const auto& device : devices)
                poll_list.push_back({device.fd.value, POLLIN, 0});
            const int status = poll(poll_list.data(), poll_list.size(), 50);
            if (status < 0 && errno != EINTR)
                fail("poll keyboard devices");
            if (status <= 0)
                continue;
            for (std::size_t i = devices.size(); i-- > 0 && !stop.load();) {
                auto& device = devices[i];
                const short events = poll_list[i].revents;
                constexpr short unavailable = POLLERR | POLLHUP | POLLNVAL;
                if ((events & unavailable) || ((events & POLLIN) &&
                    !read_device(device, counts, state.get(), callback))) {
                    reset_device(device, counts, state.get(), callback);
                    devices.erase(devices.begin() + i);
                }
            }
        }
    }

private:
    struct File {
        int value = -1;
        explicit File(int fd = -1) : value(fd) {}
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        File(File&& other) noexcept : value(std::exchange(other.value, -1)) {}
        File& operator=(File&& other) noexcept {
            std::swap(value, other.value);
            return *this;
        }
        ~File() { if (value >= 0) close(value); }
    };

    using XkbContext = std::unique_ptr<xkb_context, decltype(&xkb_context_unref)>;
    using XkbKeymap = std::unique_ptr<xkb_keymap, decltype(&xkb_keymap_unref)>;
    using XkbState = std::unique_ptr<xkb_state, decltype(&xkb_state_unref)>;

    struct Layout {
        XkbContext context{xkb_context_new(XKB_CONTEXT_NO_FLAGS), xkb_context_unref};
        XkbKeymap keymap{nullptr, xkb_keymap_unref};

        Layout() {
            if (context)
                keymap.reset(xkb_keymap_new_from_names(context.get(), nullptr,
                                                       XKB_KEYMAP_COMPILE_NO_FLAGS));
            if (!keymap)
                throw std::runtime_error("Could not load the XKB keyboard layout");
        }

        XkbState make_state() const {
            XkbState state{xkb_state_new(keymap.get()), xkb_state_unref};
            if (!state)
                throw std::runtime_error("Could not create XKB keyboard state");
            return state;
        }
    };

    static KeyMappings map_layout_name(const std::wstring& name, const Layout& layout) {
        KeyMappings result;
        for (const bool num_lock : {false, true}) {
            for (unsigned flags = 0; flags < 4; ++flags) {
                auto state = layout.make_state();
                std::vector<std::wstring> modifiers;
                if (num_lock) {
                    xkb_state_update_key(state.get(), KEY_NUMLOCK + 8, XKB_KEY_DOWN);
                    xkb_state_update_key(state.get(), KEY_NUMLOCK + 8, XKB_KEY_UP);
                    modifiers.emplace_back(L"num lock");
                }
                if (flags & 1) {
                    xkb_state_update_key(state.get(), KEY_LEFTSHIFT + 8, XKB_KEY_DOWN);
                    modifiers.emplace_back(L"shift");
                }
                if (flags & 2) {
                    xkb_state_update_key(state.get(), KEY_RIGHTALT + 8, XKB_KEY_DOWN);
                    modifiers.emplace_back(L"alt gr");
                }
                for (int code = 1; code <= KEY_MAX; ++code) {
                    if (!is_keyboard_code(code))
                        continue;
                    const auto candidate = key_name(code, state.get());
                    const bool keypad = is_keypad(code, state.get());
                    if ((candidate != L"unknown" && detail::matches_name(name, candidate)) ||
                        (code == KEY_RIGHTALT && name == L"right alt") ||
                        (keypad && name == L"keypad " + candidate)) {
                        if (std::none_of(result.begin(), result.end(), [code](const auto& entry) {
                            return entry.scan_code == code;
                        }))
                            detail::add_mapping(result, code, modifiers);
                    }
                    if (!num_lock && flags == 0 && keypad &&
                        name.size() > 7 && name == L"keypad " + physical_keypad_name(code))
                        detail::add_mapping(result, code, {});
                }
            }
        }
        if (name == L"alt gr" && result.empty())
            detail::add_mapping(result, KEY_RIGHTALT, {});
        return result;
    }

    using KeyBits = std::array<unsigned char, (KEY_MAX + 8) / 8>;
    using KeyCounts = std::array<unsigned int, KEY_MAX + 1>;

    struct Device {
        File fd;
        std::string path;
        KeyBits held{};
        bool realtime = false;
        bool dropped = false;
    };

    using Devices = std::vector<Device>;
    inline static constexpr char output_name_[] = "Keyboard++ virtual keyboard";
    inline static constexpr int modifiers_[] = {
        KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_RIGHTCTRL,
        KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTMETA, KEY_RIGHTMETA
    };
    std::mutex output_mutex_;
    File output_;

    [[noreturn]] static void fail(const char* operation) {
        throw std::system_error(errno, std::generic_category(), operation);
    }

    static bool is_keyboard_code(int code) {
        return (code > 0 && code < BTN_MISC) ||
               (code >= KEY_OK && code < BTN_DPAD_UP) ||
               (code > BTN_DPAD_RIGHT && code < BTN_TRIGGER_HAPPY);
    }

    static bool has(const KeyBits& bits, int code) {
        return (bits[code / 8] & (1u << (code % 8))) != 0;
    }

    static void set(KeyBits& bits, int code, bool down) {
        const auto mask = static_cast<unsigned char>(1u << (code % 8));
        if (down) bits[code / 8] |= mask;
        else bits[code / 8] &= static_cast<unsigned char>(~mask);
    }

    static const wchar_t* fixed_name(int code, xkb_keysym_t sym) {
        switch (code) {
        case KEY_LEFTSHIFT: return L"left shift";
        case KEY_RIGHTSHIFT: return L"right shift";
        case KEY_LEFTCTRL: return L"left ctrl";
        case KEY_RIGHTCTRL: return L"right ctrl";
        case KEY_LEFTALT: return L"left alt";
        case KEY_RIGHTALT: return sym == XKB_KEY_ISO_Level3_Shift ? L"alt gr" : L"right alt";
        case KEY_LEFTMETA: return L"left windows";
        case KEY_RIGHTMETA: return L"right windows";
        case KEY_MENU: case KEY_COMPOSE: return L"menu";
        case KEY_MUTE: return L"volume mute";
        case KEY_VOLUMEDOWN: return L"volume down";
        case KEY_VOLUMEUP: return L"volume up";
        case KEY_NEXTSONG: return L"next track";
        case KEY_PREVIOUSSONG: return L"previous track";
        case KEY_PLAYPAUSE: return L"play/pause media";
        case KEY_STOPCD: return L"stop media";
        default: break;
        }
        switch (sym) {
        case XKB_KEY_BackSpace: return L"backspace";
        case XKB_KEY_Tab: case XKB_KEY_ISO_Left_Tab: return L"tab";
        case XKB_KEY_Return: case XKB_KEY_KP_Enter: return L"enter";
        case XKB_KEY_Escape: return L"esc";
        case XKB_KEY_Delete: case XKB_KEY_KP_Delete: return L"delete";
        case XKB_KEY_Insert: case XKB_KEY_KP_Insert: return L"insert";
        case XKB_KEY_Page_Up: case XKB_KEY_KP_Page_Up: return L"page up";
        case XKB_KEY_Page_Down: case XKB_KEY_KP_Page_Down: return L"page down";
        case XKB_KEY_Home: case XKB_KEY_KP_Home: return L"home";
        case XKB_KEY_End: case XKB_KEY_KP_End: return L"end";
        case XKB_KEY_Left: case XKB_KEY_KP_Left: return L"left";
        case XKB_KEY_Right: case XKB_KEY_KP_Right: return L"right";
        case XKB_KEY_Up: case XKB_KEY_KP_Up: return L"up";
        case XKB_KEY_Down: case XKB_KEY_KP_Down: return L"down";
        case XKB_KEY_Caps_Lock: return L"caps lock";
        case XKB_KEY_Num_Lock: return L"num lock";
        case XKB_KEY_Scroll_Lock: return L"scroll lock";
        case XKB_KEY_Print: case XKB_KEY_Sys_Req: return L"print screen";
        case XKB_KEY_Pause: return L"pause";
        default: return nullptr;
        }
    }

    static std::wstring key_name(int code, xkb_state* state) {
        const auto sym = xkb_state_key_get_one_sym(state, code + 8);
        if (const auto name = fixed_name(code, sym))
            return name;
        const auto point = xkb_keysym_to_utf32(sym);
        if (point >= 32 && point != 127)
            return get_name(std::wstring(1, static_cast<wchar_t>(point)));
        char symbol[64]{};
        const int length = xkb_keysym_get_name(sym, symbol, sizeof(symbol));
        if (sym == XKB_KEY_NoSymbol || length <= 0 || length >= static_cast<int>(sizeof(symbol)))
            return L"unknown";
        std::wstring result(symbol, symbol + length);
        if (result.compare(0, 5, L"dead_") == 0)
            result.erase(0, 5);
        return get_name(std::move(result));
    }

    static std::wstring physical_keypad_name(int code) {
        switch (code) {
        case KEY_KP0: return L"0";
        case KEY_KP1: return L"1";
        case KEY_KP2: return L"2";
        case KEY_KP3: return L"3";
        case KEY_KP4: return L"4";
        case KEY_KP5: return L"5";
        case KEY_KP6: return L"6";
        case KEY_KP7: return L"7";
        case KEY_KP8: return L"8";
        case KEY_KP9: return L"9";
        case KEY_KPDOT: return L".";
        case KEY_KPPLUS: return L"+";
        case KEY_KPMINUS: return L"-";
        case KEY_KPASTERISK: return L"*";
        case KEY_KPSLASH: return L"/";
        case KEY_KPEQUAL: return L"=";
        case KEY_KPCOMMA: return L",";
        case KEY_KPENTER: return L"enter";
        default: return {};
        }
    }

    static bool is_keypad(int code, xkb_state* state) {
        const auto sym = xkb_state_key_get_one_sym(state, code + 8);
        return (sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal) || code == KEY_NUMLOCK;
    }

    static void discover_devices(Devices& devices) {
        std::error_code error;
        std::filesystem::directory_iterator entries("/dev/input", error);
        if (error)
            throw std::system_error(error, "Open /dev/input");
        for (const auto& entry : entries) {
            if (entry.path().filename().string().compare(0, 5, "event") != 0)
                continue;
            const auto path = entry.path().string();
            if (std::any_of(devices.begin(), devices.end(), [&](const auto& device) {
                return path == device.path;
            }))
                continue;
            File fd(open(entry.path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC));
            if (fd.value < 0)
                continue;
            char name[256]{};
            ioctl(fd.value, EVIOCGNAME(sizeof(name) - 1), name);
            if (std::strcmp(name, output_name_) == 0)
                continue;
            KeyBits bits{};
            if (ioctl(fd.value, EVIOCGBIT(EV_KEY, bits.size()), bits.data()) < 0)
                continue;
            for (int code = 1; code <= KEY_MAX; ++code) {
                if (is_keyboard_code(code) && has(bits, code)) {
                    int clock_id = CLOCK_REALTIME;
                    const bool realtime = ioctl(fd.value, EVIOCSCLOCKID, &clock_id) == 0;
                    devices.push_back({std::move(fd), path, {}, realtime});
                    break;
                }
            }
        }
    }

    static void update_state(Device& device, KeyCounts& counts, xkb_state* state,
                             int code, bool down) {
        if (has(device.held, code) == down)
            return;
        set(device.held, code, down);
        if (down) {
            if (++counts[code] == 1)
                xkb_state_update_key(state, code + 8, XKB_KEY_DOWN);
        } else if (--counts[code] == 0) {
            xkb_state_update_key(state, code + 8, XKB_KEY_UP);
        }
    }

    static void apply_event(Device& device, KeyCounts& counts, xkb_state* state,
                            int code, bool down, bool repeat, const EventHandler* callback,
                            const input_event* native = nullptr) {
        if (!callback) {
            update_state(device, counts, state, code, down);
            return;
        }
        KeyboardEvent event(down ? EventType::Down : EventType::Up, code,
                            key_name(code, state), device.path, is_keypad(code, state));
        event.is_repeat = repeat;
        if (native && device.realtime) {
            const auto timestamp = std::chrono::seconds(native->input_event_sec) +
                                   std::chrono::microseconds(native->input_event_usec);
            event.time = std::chrono::system_clock::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(timestamp));
        }
        for (const int modifier : modifiers_)
            if (counts[modifier])
                event.modifiers.push_back(key_name(modifier, state));
        update_state(device, counts, state, code, down);
        (*callback)(event);
    }

    static void sync_device(Device& device, KeyCounts& counts, xkb_state* state,
                            const EventHandler* callback) {
        KeyBits pressed{};
        if (ioctl(device.fd.value, EVIOCGKEY(pressed.size()), pressed.data()) < 0)
            return;
        for (int code = 1; code <= KEY_MAX; ++code)
            if (is_keyboard_code(code) && has(device.held, code) != has(pressed, code))
                apply_event(device, counts, state, code, has(pressed, code), false, callback);
    }

    static void reset_device(Device& device, KeyCounts& counts, xkb_state* state,
                             const EventHandler& callback) {
        for (int code = 1; code <= KEY_MAX; ++code)
            if (is_keyboard_code(code) && has(device.held, code))
                apply_event(device, counts, state, code, false, false, &callback);
    }

    static bool read_device(Device& device, KeyCounts& counts, xkb_state* state,
                            const EventHandler& callback) {
        std::array<input_event, 32> events{};
        const auto bytes = read(device.fd.value, events.data(), sizeof(events));
        if (bytes <= 0)
            return bytes < 0 && (errno == EAGAIN || errno == EINTR);
        for (std::size_t i = 0; i < static_cast<std::size_t>(bytes) / sizeof(input_event); ++i) {
            const auto& event = events[i];
            if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                device.dropped = true;
            } else if (device.dropped) {
                if (event.type == EV_SYN && event.code == SYN_REPORT) {
                    sync_device(device, counts, state, &callback);
                    device.dropped = false;
                }
            } else if (event.type == EV_KEY && is_keyboard_code(event.code) &&
                       event.value >= 0 && event.value <= 2) {
                apply_event(device, counts, state, event.code, event.value != 0,
                            event.value == 2, &callback, &event);
            }
        }
        return true;
    }

    void open_output() {
        File fd(open("/dev/uinput", O_WRONLY | O_CLOEXEC));
        if (fd.value < 0)
            fail("Open /dev/uinput");
        if (ioctl(fd.value, UI_SET_EVBIT, EV_KEY) < 0)
            fail("UI_SET_EVBIT");
        for (int code = 1; code <= KEY_MAX; ++code)
            if (is_keyboard_code(code) && ioctl(fd.value, UI_SET_KEYBIT, code) < 0)
                fail("UI_SET_KEYBIT");
        uinput_setup setup{};
        std::memcpy(setup.name, output_name_, sizeof(output_name_));
        setup.id.bustype = BUS_VIRTUAL;
        setup.id.vendor = 0x1209;
        setup.id.product = 1;
        if (ioctl(fd.value, UI_DEV_SETUP, &setup) < 0)
            fail("UI_DEV_SETUP");
        if (ioctl(fd.value, UI_DEV_CREATE) < 0)
            fail("UI_DEV_CREATE");
        output_ = std::move(fd);
    }

    void emit_key(int code, bool down) {
        if (!is_keyboard_code(code))
            throw std::invalid_argument("Invalid Linux keyboard scan code");
        if (output_.value < 0)
            open_output();
        input_event events[2]{};
        events[0].type = EV_KEY;
        events[0].code = static_cast<unsigned short>(code);
        events[0].value = down ? 1 : 0;
        events[1].type = EV_SYN;
        events[1].code = SYN_REPORT;
        ssize_t bytes;
        do {
            bytes = write(output_.value, events, sizeof(events));
        } while (bytes < 0 && errno == EINTR);
        if (bytes < 0)
            fail("Write /dev/uinput");
        if (bytes != static_cast<ssize_t>(sizeof(events)))
            throw std::runtime_error("Incomplete keyboard event write to /dev/uinput");
    }

    void tap(const std::vector<int>& codes) {
        std::size_t held = 0;
        try {
            for (const int code : codes) {
                emit_key(code, true);
                ++held;
            }
            while (held) {
                emit_key(codes[held - 1], false);
                --held;
            }
        } catch (...) {
            while (held) {
                try { emit_key(codes[--held], false); } catch (...) {}
            }
            throw;
        }
    }
};

} // namespace Keyboard
