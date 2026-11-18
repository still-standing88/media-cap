#include "DXFrameProcessor.h"
#include "GDIFrameProcessor.h"
#include "GDIMouseProcessor.h"
#include "InputDesktop.h"
#include "ScreenCapture.h"
#include "internal/ThreadManager.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <memory>
#include <string>

namespace SL {
namespace Screen_Capture {

    template <class T> void ProcessExit(DUPL_RETURN Ret, T *TData)
    {
        if (Ret != DUPL_RETURN_SUCCESS) {
            if (Ret == DUPL_RETURN_ERROR_EXPECTED) {
                // The system is in a transition state so request the duplication be restarted
                TData->CommonData_.ExpectedErrorEvent = true;
            }
            else {
                // Unexpected error so exit the application
                TData->CommonData_.UnexpectedErrorEvent = true;
            }
        }
    }

    void RequestScreenCapture() {}
    bool CanRequestScreenCapture() { return false; }

    bool IsScreenCaptureEnabled()
    {
        InputDesktop desktop;
        return static_cast<bool>(desktop);
    }
    template <class T> bool SwitchToInputDesktop(const InputDesktop& desktop, const std::shared_ptr<T> data)
    {
        if (!desktop) {
            // We do not have access to the desktop so request a retry
            data->CommonData_.failure(true, "Attach input desktop", desktop.error());
            ProcessExit(DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED, data.get());
            return false;
        }

        return true;
    }
    void RunCaptureMouse(std::shared_ptr<Thread_Data> data)
    {
        InputDesktop desktop;
        if (!SwitchToInputDesktop(desktop, data))
            return;
        if (!TryCaptureMouse<GDIMouseProcessor>(data))
            data->CommonData_.failure(true, "Initialize GDI mouse capture", GetLastError());
    }
    void RunCaptureMonitor(std::shared_ptr<Thread_Data> data, Monitor monitor)
    {
        // need to switch to the input desktop for capturing...
        InputDesktop desktop;
        if (!SwitchToInputDesktop(desktop, data))
            return;
#if defined _DEBUG || !defined NDEBUG
        std::cout << "Starting to Capture on Monitor " << Name(monitor) << std::endl;
        std::cout << "Trying DirectX Desktop Duplication " << std::endl;
#endif
        if (!TryCaptureMonitor<DXFrameProcessor>(data, monitor)) { // if DX is not supported, fallback to GDI capture
#if defined _DEBUG || !defined NDEBUG
            std::cout << "DirectX Desktop Duplication not supported, falling back to GDI Capturing . . ." << std::endl;
#endif
            if (!TryCaptureMonitor<GDIFrameProcessor>(data, monitor))
                data->CommonData_.ExpectedErrorEvent = true;
        }
    }

    void RunCaptureWindow(std::shared_ptr<Thread_Data> data, Window wnd)
    {
        // need to switch to the input desktop for capturing...
        InputDesktop desktop;
        if (!SwitchToInputDesktop(desktop, data))
            return;
        if (!TryCaptureWindow<GDIFrameProcessor>(data, wnd))
            data->CommonData_.ExpectedErrorEvent = true;
    }
} // namespace Screen_Capture
} // namespace SL
