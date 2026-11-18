#include "GDIFrameProcessor.h"
#include <Dwmapi.h>

#ifndef PW_RENDERFULLCONTENT
    #define PW_RENDERFULLCONTENT 2
#endif

namespace SL {
namespace Screen_Capture {

    static DUPL_RETURN gdi_failure(const std::shared_ptr<Thread_Data>& data, const char* operation) {
        const DWORD error = GetLastError();
        data->CommonData_.failure(true, operation, error);
        return DUPL_RETURN_ERROR_EXPECTED;
    }

    DUPL_RETURN GDIFrameProcessor::Init(std::shared_ptr<Thread_Data> data, const Monitor &monitor)
    {
        Data = data;
        SelectedMonitor = monitor;
        MonitorDC.DC = CreateDCA(Name(SelectedMonitor), NULL, NULL, NULL);
        if (!MonitorDC.DC) return gdi_failure(data, "CreateDC(monitor)");
        CaptureDC.DC = CreateCompatibleDC(MonitorDC.DC);
        if (!CaptureDC.DC) return gdi_failure(data, "CreateCompatibleDC(monitor)");
        CaptureBMP.Bitmap = CreateCompatibleBitmap(MonitorDC.DC, Width(monitor), Height(monitor));
        if (!CaptureBMP.Bitmap) return gdi_failure(data, "CreateCompatibleBitmap(monitor)");
        NewImageBuffer = std::make_unique<unsigned char[]>(ImageBufferSize);
        return DUPL_RETURN_SUCCESS;
    }
    DUPL_RETURN GDIFrameProcessor::Init(std::shared_ptr<Thread_Data> data, const Window &window)
    {
        Data = data;
        SelectedWindow = reinterpret_cast<HWND>(window.Handle);
        NewImageBuffer = std::make_unique<unsigned char[]>(ImageBufferSize);
        MonitorDC.DC = GetWindowDC(SelectedWindow);
        MonitorDC.Window = SelectedWindow;
        MonitorDC.Borrowed = true;
        if (!MonitorDC.DC) return gdi_failure(data, "GetWindowDC");
        CaptureDC.DC = CreateCompatibleDC(MonitorDC.DC);
        if (!CaptureDC.DC) return gdi_failure(data, "CreateCompatibleDC(window)");
        CaptureBMP.Bitmap = CreateCompatibleBitmap(MonitorDC.DC, window.Size.x, window.Size.y);
        if (!CaptureBMP.Bitmap) return gdi_failure(data, "CreateCompatibleBitmap(window)");
        return DUPL_RETURN_SUCCESS;
    }
    DUPL_RETURN GDIFrameProcessor::ProcessFrame(const Monitor &currentmonitorinfo)
    {
        auto Ret = DUPL_RETURN_SUCCESS;

        ImageRect ret;
        ret.left = ret.top = 0;
        ret.bottom = Height(SelectedMonitor);
        ret.right = Width(SelectedMonitor);

        // Selecting an object into the specified DC
        auto originalBmp = SelectObject(CaptureDC.DC, CaptureBMP.Bitmap);

        if (BitBlt(CaptureDC.DC, 0, 0, ret.right, ret.bottom, MonitorDC.DC, 0, 0, SRCCOPY | CAPTUREBLT) == FALSE) {
            // if the screen cannot be captured, return
            const auto failure = gdi_failure(Data, "Copy GDI frame");
            SelectObject(CaptureDC.DC, originalBmp);
            return failure;
        }

        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof(bi));

        bi.biSize = sizeof(BITMAPINFOHEADER);

        bi.biWidth = ret.right;
        bi.biHeight = -ret.bottom;
        bi.biPlanes = 1;
        bi.biBitCount = sizeof(ImageBGRA) * 8; // always 32 bits damnit!!!
        bi.biCompression = BI_RGB;
        bi.biSizeImage = ((ret.right * bi.biBitCount + 31) / (sizeof(ImageBGRA) * 8)) * sizeof(ImageBGRA) * ret.bottom;
        SelectObject(CaptureDC.DC, originalBmp);
        SetLastError(ERROR_SUCCESS);
        if (GetDIBits(MonitorDC.DC, CaptureBMP.Bitmap, 0, (UINT)ret.bottom, NewImageBuffer.get(), (BITMAPINFO *)&bi, DIB_RGB_COLORS) != ret.bottom)
            return gdi_failure(Data, "GetDIBits");
        ProcessCapture(Data->ScreenCaptureData, *this, currentmonitorinfo, NewImageBuffer.get(), Width(SelectedMonitor) * sizeof(ImageBGRA));

        return Ret;
    }

    DUPL_RETURN GDIFrameProcessor::ProcessFrame(Window &selectedwindow)
    {
        auto Ret = DUPL_RETURN_SUCCESS;
        auto windowrect = SL::Screen_Capture::GetWindowRect(SelectedWindow);
        ImageRect ret;
        memset(&ret, 0, sizeof(ret));
        ret.bottom = windowrect.ClientRect.bottom;
        ret.left = windowrect.ClientRect.left;
        ret.right = windowrect.ClientRect.right;
        ret.top = windowrect.ClientRect.top;
        selectedwindow.Position.x = windowrect.ClientRect.left;
        selectedwindow.Position.y = windowrect.ClientRect.top;

        if (!IsWindow(SelectedWindow) || selectedwindow.Size.x != Width(ret) || selectedwindow.Size.y != Height(ret)) {
            return DUPL_RETURN::DUPL_RETURN_ERROR_EXPECTED; // window size changed. This will rebuild everything
        }

        // Selecting an object into the specified DC
        auto originalBmp = SelectObject(CaptureDC.DC, CaptureBMP.Bitmap);
        auto left = -windowrect.ClientBorder.left;
        auto top = -windowrect.ClientBorder.top;

        BOOL result = PrintWindow((HWND)selectedwindow.Handle, CaptureDC.DC, PW_RENDERFULLCONTENT);

        if (!result) {
            result = BitBlt(CaptureDC.DC, left, top, ret.right, ret.bottom, MonitorDC.DC, 0, 0, SRCCOPY | CAPTUREBLT);
        }

        if (!result) {
            // if the screen cannot be captured, return
            const auto failure = gdi_failure(Data, "Copy GDI frame");
            SelectObject(CaptureDC.DC, originalBmp);
            return failure;
        }

        // std::vector<HWND> childrenToComposite = CollectWindowsToComposite((HWND)selectedwindow.Handle);
        //
        //// list is ordered topmost to bottommost, so we visit them in reverse order to let painter's algorithm work
        // for ( auto child = childrenToComposite.rbegin(); child != childrenToComposite.rend(); child++ ) {
        //     auto childRect = SL::Screen_Capture::GetWindowRect( *child );

        //    HDC srcDC = GetWindowDC(*child);

        //    // if this fails we just won't composite this window, so continue with the others to get what we can
        //    BOOL childBlitSuccess = BitBlt(CaptureDC.DC, childRect.ClientRect.left - windowrect.ClientRect.left, childRect.ClientRect.top -
        //    windowrect.ClientRect.top,
        //           childRect.ClientRect.right - childRect.ClientRect.left, childRect.ClientRect.bottom - childRect.ClientRect.top,
        //           srcDC, 0, 0,
        //           SRCCOPY | CAPTUREBLT);
        //    if ( !childBlitSuccess ) {
        //        DWORD err = GetLastError();
        //    }

        //    ReleaseDC(*child, srcDC);
        //}

        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof(bi));
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = Width(ret);
        bi.biHeight = -Height(ret);
        bi.biPlanes = 1;
        bi.biBitCount = sizeof(ImageBGRA) * 8; // always 32 bits damnit!!!
        bi.biCompression = BI_RGB;
        bi.biSizeImage = ((Width(ret) * bi.biBitCount + 31) / (sizeof(ImageBGRA) * 8)) * sizeof(ImageBGRA) * Height(ret);
        SelectObject(CaptureDC.DC, originalBmp);
        SetLastError(ERROR_SUCCESS);
        if (GetDIBits(MonitorDC.DC, CaptureBMP.Bitmap, 0, (UINT)Height(ret), NewImageBuffer.get(), (BITMAPINFO *)&bi, DIB_RGB_COLORS) != Height(ret))
            return gdi_failure(Data, "GetDIBits");
        ProcessCapture(Data->WindowCaptureData, *this, selectedwindow, NewImageBuffer.get(), Width(selectedwindow) * sizeof(ImageBGRA));

        return Ret;
    }
} // namespace Screen_Capture
} // namespace SL
