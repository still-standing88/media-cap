#pragma once

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/hidsystem/IOLLEvent.h>

namespace Keyboard {

class DarwinKeyboardManager final : public KeyboardManager {
public:
    bool supports_suppression() const noexcept override { return true; }
    void press(int code) override { post_key(code, true); }
    void release(int code) override { post_key(code, false); }

    void type_unicode(unsigned long point) override {
        const auto units = detail::utf16(point);
        check_post_access();
        auto down = make_event(0, true);
        auto up = make_event(0, false);
        for (auto event : {down.get(), up.get()}) {
            CGEventKeyboardSetUnicodeString(event, units.size(), units.data());
            CGEventPost(kCGSessionEventTap, event);
        }
    }

protected:
    KeyMappings lookup_name(const std::wstring& name) override {
        Layout layout;
        KeyMappings result;
        for (unsigned flags = 0; flags < 4; ++flags) {
            std::vector<std::wstring> modifiers;
            if (flags & 1) modifiers.emplace_back(L"shift");
            if (flags & 2) modifiers.emplace_back(L"alt");
            for (int code = 0; code < 128; ++code) {
                const auto candidate = key_name(code, layout, flags & 1, flags & 2);
                if ((!candidate.empty() && detail::matches_name(name, candidate)) ||
                    (keypad(code) && name == L"keypad " + candidate)) {
                    if (std::none_of(result.begin(), result.end(), [code](const auto& entry) {
                        return entry.scan_code == code;
                    }))
                        detail::add_mapping(result, code, modifiers);
                }
            }
        }
        return result;
    }

    void capture(const EventHandler& callback, const std::atomic<bool>& stop,
                 const std::function<void()>& ready, bool suppress) override {
        if (suppress && !AXIsProcessTrusted())
            throw std::runtime_error("Allow Accessibility access for the calling app");
        if (!suppress && !CGPreflightListenEventAccess() && !AXIsProcessTrusted())
            throw std::runtime_error("Allow Input Monitoring access for the calling app");
        TapContext context{callback, stop, suppress};
        for (const auto& modifier : modifier_keys_)
            if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, modifier.code))
                context.held.insert(modifier.code);

        const CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                                 CGEventMaskBit(kCGEventFlagsChanged);
        CFRef<CFMachPortRef> tap(CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
            suppress ? kCGEventTapOptionDefault : kCGEventTapOptionListenOnly,
            mask, on_event, &context));
        if (!tap)
            throw std::runtime_error("CGEventTapCreate could not open the keyboard tap");
        context.tap = tap.get();

        CFRef<CFRunLoopSourceRef> source(CFMachPortCreateRunLoopSource(nullptr, tap.get(), 0));
        if (!source)
            throw std::runtime_error("Could not attach the keyboard tap to a run loop");
        auto run_loop = CFRunLoopGetCurrent();
        CFRunLoopAddSource(run_loop, source.get(), kCFRunLoopDefaultMode);
        CGEventTapEnable(tap.get(), true);
        const auto cleanup = [&] {
            CGEventTapEnable(tap.get(), false);
            CFRunLoopRemoveSource(run_loop, source.get(), kCFRunLoopDefaultMode);
            CFMachPortInvalidate(tap.get());
        };
        try {
            ready();
            while (!stop.load() && !context.failure) {
                const auto status = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
                if (status == kCFRunLoopRunFinished)
                    throw std::runtime_error("The keyboard tap run loop finished");
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
    struct CFReleaseObject {
        void operator()(const void* value) const noexcept { CFRelease(value); }
    };

    template<typename Reference>
    using CFRef = std::unique_ptr<std::remove_pointer_t<Reference>, CFReleaseObject>;

    struct Layout {
        CFRef<TISInputSourceRef> source{TISCopyCurrentKeyboardLayoutInputSource()};

        Layout() {
            if (!data())
                source.reset(TISCopyCurrentASCIICapableKeyboardLayoutInputSource());
        }

        const UCKeyboardLayout* data() const {
            if (!source)
                return nullptr;
            const auto data = static_cast<CFDataRef>(TISGetInputSourceProperty(
                source.get(), kTISPropertyUnicodeKeyLayoutData));
            return data ? reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data)) : nullptr;
        }
    };

    struct TapContext {
        const EventHandler& callback;
        const std::atomic<bool>& stop;
        bool suppress;
        CFMachPortRef tap = nullptr;
        std::unordered_set<int> held{};
        std::exception_ptr failure{};
    };

    struct Modifier {
        CGKeyCode code;
        CGEventFlags flag;
        CGEventFlags device_flag;
        const wchar_t* name;
    };

    inline static constexpr Modifier modifier_keys_[] = {
        {kVK_Shift, kCGEventFlagMaskShift, NX_DEVICELSHIFTKEYMASK, L"left shift"},
        {kVK_RightShift, kCGEventFlagMaskShift, NX_DEVICERSHIFTKEYMASK, L"right shift"},
        {kVK_Control, kCGEventFlagMaskControl, NX_DEVICELCTLKEYMASK, L"left ctrl"},
        {kVK_RightControl, kCGEventFlagMaskControl, NX_DEVICERCTLKEYMASK, L"right ctrl"},
        {kVK_Option, kCGEventFlagMaskAlternate, NX_DEVICELALTKEYMASK, L"left alt"},
        {kVK_RightOption, kCGEventFlagMaskAlternate, NX_DEVICERALTKEYMASK, L"right alt"},
        {kVK_Command, kCGEventFlagMaskCommand, NX_DEVICELCMDKEYMASK, L"left command"},
        {kVK_RightCommand, kCGEventFlagMaskCommand, NX_DEVICERCMDKEYMASK, L"right command"},
        {kVK_CapsLock, kCGEventFlagMaskAlphaShift, 0, L"caps lock"},
        {kVK_Function, kCGEventFlagMaskSecondaryFn, 0, L"fn"}
    };

    std::mutex output_mutex_;
    std::unordered_set<int> output_modifiers_;

    inline static constexpr int64_t input_tag_ = 0x4b425050;

    static const Modifier* modifier_key(int code) {
        for (const auto& modifier : modifier_keys_)
            if (modifier.code == code)
                return &modifier;
        return nullptr;
    }

    static const wchar_t* keypad_name(int code) {
        switch (code) {
        case kVK_ANSI_Keypad0: return L"0";
        case kVK_ANSI_Keypad1: return L"1";
        case kVK_ANSI_Keypad2: return L"2";
        case kVK_ANSI_Keypad3: return L"3";
        case kVK_ANSI_Keypad4: return L"4";
        case kVK_ANSI_Keypad5: return L"5";
        case kVK_ANSI_Keypad6: return L"6";
        case kVK_ANSI_Keypad7: return L"7";
        case kVK_ANSI_Keypad8: return L"8";
        case kVK_ANSI_Keypad9: return L"9";
        case kVK_ANSI_KeypadDecimal: return L".";
        case kVK_ANSI_KeypadMultiply: return L"*";
        case kVK_ANSI_KeypadPlus: return L"+";
        case kVK_ANSI_KeypadClear: return L"clear";
        case kVK_ANSI_KeypadDivide: return L"/";
        case kVK_ANSI_KeypadEnter: return L"enter";
        case kVK_ANSI_KeypadMinus: return L"-";
        case kVK_ANSI_KeypadEquals: return L"=";
        case kVK_JIS_KeypadComma: return L",";
        default: return nullptr;
        }
    }

    static bool keypad(int code) { return keypad_name(code) != nullptr; }

    static const wchar_t* fixed_name(int code) {
        if (const auto modifier = modifier_key(code))
            return modifier->name;
        if (const auto name = keypad_name(code))
            return name;
        switch (code) {
        case kVK_Return: return L"enter";
        case kVK_Tab: return L"tab";
        case kVK_Space: return L"space";
        case kVK_Delete: return L"backspace";
        case kVK_Escape: return L"esc";
        case kVK_Help: return L"help";
        case kVK_Home: return L"home";
        case kVK_End: return L"end";
        case kVK_PageUp: return L"page up";
        case kVK_PageDown: return L"page down";
        case kVK_ForwardDelete: return L"delete";
        case kVK_LeftArrow: return L"left";
        case kVK_RightArrow: return L"right";
        case kVK_UpArrow: return L"up";
        case kVK_DownArrow: return L"down";
        case kVK_F1: return L"f1";
        case kVK_F2: return L"f2";
        case kVK_F3: return L"f3";
        case kVK_F4: return L"f4";
        case kVK_F5: return L"f5";
        case kVK_F6: return L"f6";
        case kVK_F7: return L"f7";
        case kVK_F8: return L"f8";
        case kVK_F9: return L"f9";
        case kVK_F10: return L"f10";
        case kVK_F11: return L"f11";
        case kVK_F12: return L"f12";
        case kVK_F13: return L"f13";
        case kVK_F14: return L"f14";
        case kVK_F15: return L"f15";
        case kVK_F16: return L"f16";
        case kVK_F17: return L"f17";
        case kVK_F18: return L"f18";
        case kVK_F19: return L"f19";
        case kVK_F20: return L"f20";
        case kVK_VolumeUp: return L"volume up";
        case kVK_VolumeDown: return L"volume down";
        case kVK_Mute: return L"volume mute";
        default: return nullptr;
        }
    }

    static std::wstring key_name(int code, const Layout& layout, bool shift, bool option,
                                 bool caps = false) {
        if (const auto name = fixed_name(code))
            return name;
        const auto data = layout.data();
        if (!data)
            return {};
        UInt32 dead = 0;
        UniChar chars[8]{};
        UniCharCount length = 0;
        const UInt32 flags = ((shift ? shiftKey : 0) | (option ? optionKey : 0) |
                              (caps ? alphaLock : 0)) >> 8;
        const auto result = UCKeyTranslate(data, static_cast<UInt16>(code), kUCKeyActionDown,
                                           flags, LMGetKbdType(), kUCKeyTranslateNoDeadKeysMask,
                                           &dead, 8, &length, chars);
        if (result != noErr || length == 0 || chars[0] < 32 || chars[0] == 127)
            return {};
        std::wstring name;
        for (UniCharCount i = 0; i < length; ++i) {
            unsigned point = chars[i];
            if (point >= 0xd800 && point <= 0xdbff && i + 1 < length &&
                chars[i + 1] >= 0xdc00 && chars[i + 1] <= 0xdfff) {
                point = 0x10000 + ((point - 0xd800) << 10) + chars[++i] - 0xdc00;
            }
            name.push_back(static_cast<wchar_t>(point));
        }
        return get_name(std::move(name));
    }

    static void check_post_access() {
        if (!CGPreflightPostEventAccess())
            throw std::runtime_error("Allow Accessibility access to send keyboard input");
    }

    static CFRef<CGEventRef> make_event(int code, bool down) {
        CFRef<CGEventRef> event(CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(code), down));
        if (!event)
            throw std::runtime_error("CGEventCreateKeyboardEvent failed");
        CGEventSetIntegerValueField(event.get(), kCGEventSourceUserData, input_tag_);
        return event;
    }

    void post_key(int code, bool down) {
        if (code < 0 || code >= 128)
            throw std::invalid_argument("Invalid macOS keyboard scan code");
        check_post_access();
        auto event = make_event(code, down);
        std::lock_guard<std::mutex> lock(output_mutex_);
        auto flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
        if (const auto modifier = modifier_key(code)) {
            if (down) {
                output_modifiers_.insert(code);
            } else {
                output_modifiers_.erase(code);
                flags &= ~modifier->device_flag;
                bool other_side_down = false;
                for (const auto& other : modifier_keys_)
                    if (other.code != code && other.flag == modifier->flag &&
                        CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, other.code))
                        other_side_down = true;
                if (!other_side_down) flags &= ~modifier->flag;
            }
        }
        for (const auto& modifier : modifier_keys_)
            if (output_modifiers_.count(modifier.code))
                flags |= modifier.flag | modifier.device_flag;
        CGEventSetFlags(event.get(), flags);
        CGEventPost(kCGHIDEventTap, event.get());
    }

    static CGEventRef on_event(CGEventTapProxy, CGEventType type, CGEventRef event, void* user) noexcept {
        auto& context = *static_cast<TapContext*>(user);
        if (context.stop.load() || context.failure)
            return event;
        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            CGEventTapEnable(context.tap, true);
            return event;
        }
        if (!event || CGEventGetIntegerValueField(event, kCGEventSourceUserData) == input_tag_)
            return event;

        try {
            const int code = static_cast<int>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            const auto flags = CGEventGetFlags(event);
            bool down = type == kCGEventKeyDown;
            if (type == kCGEventFlagsChanged) {
                const auto modifier = modifier_key(code);
                if (!modifier)
                    return event;
                down = (flags & modifier->flag) != 0;
                if (down && modifier->device_flag)
                    down = (flags & modifier->device_flag) || !context.held.count(code);
            }
            Layout layout;
            auto name = key_name(code, layout, flags & kCGEventFlagMaskShift,
                                 flags & kCGEventFlagMaskAlternate, flags & kCGEventFlagMaskAlphaShift);
            KeyboardEvent input(down ? EventType::Down : EventType::Up, code,
                                name.empty() ? L"unknown" : std::move(name), {}, keypad(code));
            for (const auto& modifier : modifier_keys_)
                if (modifier.device_flag && context.held.count(modifier.code))
                    input.modifiers.emplace_back(modifier.name);
            if (down) input.is_repeat = !context.held.insert(code).second;
            else context.held.erase(code);
            const bool forward = context.callback(input);
            if (context.suppress && !forward)
                return nullptr;
        } catch (...) {
            context.failure = std::current_exception();
        }
        return event;
    }
};

} // namespace Keyboard
