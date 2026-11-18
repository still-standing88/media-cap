#pragma once

#include <windows.h>
#include <array>
#include <system_error>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#endif

namespace Keyboard {

class WindowsKeyboardManager final : public KeyboardManager {
public:
    bool supports_suppression() const noexcept override { return true; }
    void press(int code) override { inject_key(code, false); }
    void release(int code) override { inject_key(code, true); }

    void type_unicode(unsigned long point) override {
        std::array<INPUT, 4> input{};
        const auto units = detail::utf16(point);
        const auto count = static_cast<UINT>(units.size());
        for (UINT i = 0; i < count; ++i) {
            input[i] = key_input(0, units[i], KEYEVENTF_UNICODE);
            input[count + i] = key_input(0, units[i], KEYEVENTF_UNICODE | KEYEVENTF_KEYUP);
        }
        send_inputs(input.data(), 2 * count);
    }

protected:
    KeyMappings lookup_name(const std::wstring& name) override {
        const auto layout = active_layout();
        KeyMappings result;
        for (const auto prefix : {0u, 0xe000u}) {
            for (UINT scan = 1; scan <= 0xff; ++scan) {
                const UINT native = prefix | scan;
                const auto vk = MapVirtualKeyExW(native, MAPVK_VSC_TO_VK_EX, layout);
                if (vk != 0)
                    match_key(result, name, scan_code(scan, vk, prefix != 0), vk, layout);
            }
        }
        for (UINT vk = 1; vk < 0xff; ++vk) {
            if (!special_name(vk))
                continue;
            const auto native = MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC_EX, layout);
            if (native != 0 || (vk >= VK_BROWSER_BACK && vk <= VK_LAUNCH_APP2)) {
                const auto code = scan_code(native & 0xff, vk, (native & 0xff00) == 0xe000);
                match_key(result, name, code, vk, layout);
            }
        }
        std::stable_sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            const auto is_keypad_code = [](int code) {
                return code >= 0x47 && code <= 0x53;
            };
            return is_keypad_code(left.scan_code) < is_keypad_code(right.scan_code);
        });
        return result;
    }

    void capture(const EventHandler& callback, const std::atomic<bool>& stop,
                 const std::function<void()>& ready, bool suppress) override {
        if (active_hook_)
            throw std::logic_error("This thread already has a keyboard hook");
        HookContext context{callback, stop, suppress};
        context.caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
        MSG message{};
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
        for (const UINT vk : modifier_keys_) {
            if (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000)
                context.modifiers.insert(vk);
        }
        active_hook_ = &context;
        const auto hook = SetWindowsHookExW(WH_KEYBOARD_LL, on_message, GetModuleHandleW(nullptr), 0);
        if (!hook) {
            active_hook_ = nullptr;
            throw_win32("SetWindowsHookExW");
        }
        const auto cleanup = [&] {
            UnhookWindowsHookEx(hook);
            active_hook_ = nullptr;
        };
        try {
            ready();
            while (!stop.load() && !context.failure) {
                const auto status = MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT,
                                                                MWMO_INPUTAVAILABLE);
                if (status == WAIT_FAILED)
                    throw_win32("MsgWaitForMultipleObjectsEx");
                while (!stop.load() && !context.failure &&
                       PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) {
                        cleanup();
                        return;
                    }
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            if (context.failure)
                std::rethrow_exception(context.failure);
        } catch (...) {
            cleanup();
            throw;
        }
        cleanup();
    }

private:
    struct HookContext {
        const EventHandler& callback;
        const std::atomic<bool>& stop;
        bool suppress;
        bool caps = false;
        bool altgr = false;
        bool paired_alt_forward = true;
        UINT paired_alt_message = 0;
        std::unordered_set<UINT> modifiers{};
        std::unordered_set<int> held{};
        std::exception_ptr failure{};
    };

    inline static thread_local HookContext* active_hook_ = nullptr;
    inline static constexpr ULONG_PTR input_tag_ = 0x4b425050;
    inline static constexpr UINT modifier_keys_[] = {
        VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN
    };

    [[noreturn]] static void throw_win32(const char* operation) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
    }

    static HKL active_layout() {
        return GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
    }

    static INPUT key_input(WORD vk, WORD scan, DWORD flags) {
        INPUT result{};
        result.type = INPUT_KEYBOARD;
        result.ki = {vk, scan, flags, 0, input_tag_};
        return result;
    }

    static void send_inputs(INPUT* inputs, UINT count) {
        SetLastError(ERROR_SUCCESS);
        if (SendInput(count, inputs, sizeof(INPUT)) != count) {
            if (GetLastError() != ERROR_SUCCESS)
                throw_win32("SendInput");
            throw std::runtime_error("SendInput could not deliver keyboard input");
        }
    }

    static void inject_key(int code, bool up) {
        DWORD flags = up ? KEYEVENTF_KEYUP : 0;
        if (code == 541) {
            std::array<INPUT, 2> inputs = up
                ? std::array<INPUT, 2>{key_input(VK_RMENU, 0, flags | KEYEVENTF_EXTENDEDKEY),
                                       key_input(VK_LCONTROL, 0, flags)}
                : std::array<INPUT, 2>{key_input(VK_LCONTROL, 0, flags),
                                       key_input(VK_RMENU, 0, flags | KEYEVENTF_EXTENDEDKEY)};
            send_inputs(inputs.data(), static_cast<UINT>(inputs.size()));
            return;
        }
        INPUT input{};
        if (code < 0 && code >= -0xfe) {
            input = key_input(static_cast<WORD>(-code), 0, flags);
        } else if ((code > 0 && code <= 0xff) || (code >= 0xe001 && code <= 0xe0ff)) {
            flags |= KEYEVENTF_SCANCODE;
            if (code > 0xff)
                flags |= KEYEVENTF_EXTENDEDKEY;
            input = key_input(0, static_cast<WORD>(code & 0xff), flags);
        } else {
            throw std::invalid_argument("Invalid Windows keyboard scan code");
        }
        send_inputs(&input, 1);
    }

    static int scan_code(UINT scan, UINT vk, bool extended) {
        if (scan == 0 || vk == VK_PAUSE || (vk >= VK_BROWSER_BACK && vk <= VK_LAUNCH_APP2))
            return -static_cast<int>(vk);
        return static_cast<int>((scan & 0xff) | (extended ? 0xe000 : 0));
    }

    static bool keypad(UINT scan, UINT vk, bool extended) {
        return (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE) || vk == VK_NUMLOCK ||
               (extended && (vk == VK_RETURN || vk == VK_DIVIDE)) ||
               (!extended && scan >= 0x47 && scan <= 0x53);
    }

    static const wchar_t* special_name(UINT vk) {
        static constexpr const wchar_t* function_keys[] = {
            L"f1", L"f2", L"f3", L"f4", L"f5", L"f6", L"f7", L"f8", L"f9", L"f10", L"f11", L"f12",
            L"f13", L"f14", L"f15", L"f16", L"f17", L"f18", L"f19", L"f20", L"f21", L"f22", L"f23", L"f24"
        };
        static constexpr const wchar_t* keypad_keys[] = {
            L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9",
            L"*", L"+", L"separator", L"-", L".", L"/"
        };
        if (vk >= VK_F1 && vk <= VK_F24) return function_keys[vk - VK_F1];
        if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE) return keypad_keys[vk - VK_NUMPAD0];
        switch (vk) {
        case VK_BACK: return L"backspace";
        case VK_TAB: return L"tab";
        case VK_RETURN: return L"enter";
        case VK_PAUSE: return L"pause";
        case VK_CAPITAL: return L"caps lock";
        case VK_ESCAPE: return L"esc";
        case VK_SPACE: return L"space";
        case VK_PRIOR: return L"page up";
        case VK_NEXT: return L"page down";
        case VK_END: return L"end";
        case VK_HOME: return L"home";
        case VK_LEFT: return L"left";
        case VK_UP: return L"up";
        case VK_RIGHT: return L"right";
        case VK_DOWN: return L"down";
        case VK_SNAPSHOT: return L"print screen";
        case VK_INSERT: return L"insert";
        case VK_DELETE: return L"delete";
        case VK_LWIN: return L"left windows";
        case VK_RWIN: return L"right windows";
        case VK_APPS: return L"menu";
        case VK_NUMLOCK: return L"num lock";
        case VK_SCROLL: return L"scroll lock";
        case VK_LSHIFT: return L"left shift";
        case VK_RSHIFT: return L"right shift";
        case VK_LCONTROL: return L"left ctrl";
        case VK_RCONTROL: return L"right ctrl";
        case VK_LMENU: return L"left alt";
        case VK_RMENU: return L"right alt";
        case VK_BROWSER_BACK: return L"browser back";
        case VK_BROWSER_FORWARD: return L"browser forward";
        case VK_BROWSER_REFRESH: return L"browser refresh";
        case VK_BROWSER_STOP: return L"browser stop";
        case VK_BROWSER_SEARCH: return L"browser search";
        case VK_BROWSER_FAVORITES: return L"browser favorites";
        case VK_BROWSER_HOME: return L"browser home";
        case VK_VOLUME_MUTE: return L"volume mute";
        case VK_VOLUME_DOWN: return L"volume down";
        case VK_VOLUME_UP: return L"volume up";
        case VK_MEDIA_NEXT_TRACK: return L"next track";
        case VK_MEDIA_PREV_TRACK: return L"previous track";
        case VK_MEDIA_STOP: return L"stop media";
        case VK_MEDIA_PLAY_PAUSE: return L"play/pause media";
        case VK_LAUNCH_MAIL: return L"launch mail";
        case VK_LAUNCH_MEDIA_SELECT: return L"select media";
        case VK_LAUNCH_APP1: return L"launch app 1";
        case VK_LAUNCH_APP2: return L"launch app 2";
        default: return nullptr;
        }
    }

    static std::wstring translated_name(UINT scan, UINT vk, HKL layout,
                                        bool shift, bool altgr, bool caps = false) {
        if (const auto name = special_name(vk))
            return name;
        std::array<BYTE, 256> state{};
        state[VK_SHIFT] = shift ? 0x80 : 0;
        state[VK_CONTROL] = altgr ? 0x80 : 0;
        state[VK_MENU] = altgr ? 0x80 : 0;
        state[VK_CAPITAL] = caps ? 1 : 0;
        wchar_t buffer[16]{};
        const int length = ToUnicodeEx(vk, scan, state.data(), buffer, 16, 4, layout);
        if (length > 0)
            return get_name(std::wstring(buffer, length));
        if (length < 0 && buffer[0])
            return get_name(std::wstring(1, buffer[0]));
        return {};
    }

    static void match_key(KeyMappings& result, const std::wstring& name, int code, UINT vk,
                          HKL layout) {
        if (vk == VK_RMENU && name == L"alt gr")
            detail::add_mapping(result, 541, {});
        const auto scan = static_cast<UINT>(code < 0 ? 0 : code & 0xff);
        const bool extended = code >= 0xe000;
        const auto base = translated_name(scan, vk, layout, false, false);
        if (detail::matches_name(name, base) || (keypad(scan, vk, extended) && name == L"keypad " + base))
            detail::add_mapping(result, code, {});
        if (special_name(vk))
            return;
        for (const bool altgr : {false, true}) {
            for (const bool shift : {false, true}) {
                if ((!shift && !altgr) || base == name)
                    continue;
                if (translated_name(scan, vk, layout, shift, altgr) == name) {
                    std::vector<std::wstring> modifiers;
                    if (shift) modifiers.push_back(L"shift");
                    if (altgr) modifiers.push_back(L"alt gr");
                    detail::add_mapping(result, code, std::move(modifiers));
                }
            }
        }
    }

    static LRESULT CALLBACK on_message(int action, WPARAM message, LPARAM data) noexcept {
        auto* context = active_hook_;
        if (action != HC_ACTION || !context || context->stop.load() || context->failure)
            return CallNextHookEx(nullptr, action, message, data);
        const auto& key = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        if (key.vkCode == VK_PACKET || ((key.flags & LLKHF_INJECTED) && key.dwExtraInfo == input_tag_))
            return CallNextHookEx(nullptr, action, message, data);
        try {
            const auto layout = active_layout();
            bool extended = (key.flags & LLKHF_EXTENDED) != 0;
            UINT scan = key.scanCode;
            if (!scan) {
                const auto native = MapVirtualKeyExW(key.vkCode, MAPVK_VK_TO_VSC_EX, layout);
                scan = native & 0xff;
                extended = extended || (native & 0xff00) == 0xe000;
            }
            const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
            const UINT alt_transition = up ? WM_KEYUP : WM_KEYDOWN;
            if (key.vkCode == VK_RMENU && context->paired_alt_message == alt_transition) {
                context->paired_alt_message = 0;
                return context->paired_alt_forward ? CallNextHookEx(nullptr, action, message, data) : 1;
            }

            const bool altgr_key = key.scanCode == 541 && key.vkCode == VK_LCONTROL;
            const int code = altgr_key ? 541 : scan_code(scan, key.vkCode, extended);
            const bool shift = context->modifiers.count(VK_LSHIFT) || context->modifiers.count(VK_RSHIFT);
            const bool altgr = context->altgr || (context->modifiers.count(VK_RMENU) &&
                                                 context->modifiers.count(VK_LCONTROL));
            auto name = altgr_key ? std::wstring(L"alt gr") :
                translated_name(scan, key.vkCode, layout, shift, altgr, context->caps);
            if (name.empty()) {
                wchar_t buffer[64]{};
                const int length = GetKeyNameTextW(static_cast<LONG>((scan << 16) |
                                                  (extended ? 1u << 24 : 0)), buffer, 64);
                name = length > 0 ? get_name(std::wstring(buffer, length)) : L"unknown";
            }
            KeyboardEvent event(up ? EventType::Up : EventType::Down, code, std::move(name),
                                {}, keypad(scan, key.vkCode, extended));
            if (context->altgr)
                event.modifiers.emplace_back(L"alt gr");
            for (const UINT vk : modifier_keys_) {
                if (context->modifiers.count(vk))
                    event.modifiers.emplace_back(special_name(vk));
                if (!altgr_key && vk == key.vkCode) {
                    // Collect the state from before this event, then apply the change.
                    if (up) context->modifiers.erase(vk);
                    else context->modifiers.insert(vk);
                }
            }
            if (up) context->held.erase(code);
            else event.is_repeat = !context->held.insert(code).second;
            if (key.vkCode == VK_CAPITAL && !up && !event.is_repeat)
                context->caps = !context->caps;
            const bool accept = context->callback(event);
            const bool forward = !context->suppress || accept;
            if (altgr_key) {
                context->altgr = !up;
                context->paired_alt_message = alt_transition;
                context->paired_alt_forward = forward;
            }
            if (!forward)
                return 1;
        } catch (...) {
            context->failure = std::current_exception();
        }
        return CallNextHookEx(nullptr, action, message, data);
    }
};

} // namespace Keyboard
