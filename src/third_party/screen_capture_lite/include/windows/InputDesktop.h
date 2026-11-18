#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace SL {
namespace Screen_Capture {
    // CloseDesktop fails with ERROR_BUSY while this thread is attached to the
    // opened handle. Restore the borrowed original desktop before closing it.
    // Keep this guard outside the capture processor's lifetime so its GDI
    // resources have already been released when the thread switches back.
    class InputDesktop {
        HDESK Original = nullptr;
        HDESK Opened = nullptr;
        bool Attached = false;
        DWORD Error = ERROR_SUCCESS;
    public:
        InputDesktop() {
            Original = GetThreadDesktop(GetCurrentThreadId());
            if (Original) {
                Opened = OpenInputDesktop(0, FALSE, GENERIC_ALL);
                Attached = Opened && SetThreadDesktop(Opened) != FALSE;
                if (!Attached) Error = GetLastError();
            } else Error = GetLastError();
        }
        DWORD error() const { return Error; }
        InputDesktop(const InputDesktop&) = delete;
        InputDesktop& operator=(const InputDesktop&) = delete;
        ~InputDesktop() {
            if (Opened) {
                if (Attached) SetThreadDesktop(Original);
                CloseDesktop(Opened);
            }
        }
        explicit operator bool() const { return Attached; }
    };
}
}
