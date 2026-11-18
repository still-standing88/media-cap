using System.Runtime.CompilerServices;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;

namespace MediaCap;

public static unsafe class Screenshots
{
    private sealed class Result
    {
        internal VideoFrame? Frame;
        internal ExceptionDispatchInfo? Error;
    }

    public static VideoFrame? Capture(Monitor source, ScreenshotConfig config = default)
    {
        ArgumentNullException.ThrowIfNull(source);
        var value = source.NativeValue;
        var result = new Result();
        var token = GCHandle.Alloc(result);
        try
        {
            int success = Native.CaptureMonitorScreenshot(&value, &config, &Received, GCHandle.ToIntPtr(token));
            result.Error?.Throw();
            return success != 0 ? result.Frame : null;
        }
        finally { token.Free(); }
    }

    public static VideoFrame? Capture(Window source, ScreenshotConfig config = default)
    {
        ArgumentNullException.ThrowIfNull(source);
        var value = source.NativeValue;
        var result = new Result();
        var token = GCHandle.Alloc(result);
        try
        {
            int success = Native.CaptureWindowScreenshot(&value, &config, &Received, GCHandle.ToIntPtr(token));
            result.Error?.Throw();
            return success != 0 ? result.Frame : null;
        }
        finally { token.Free(); }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void Received(NativeVideoFrameView* frame, nint context)
    {
        var result = (Result)GCHandle.FromIntPtr(context).Target!;
        try { result.Frame = VideoFrame.Copy(frame); }
        catch (Exception error) { result.Error = ExceptionDispatchInfo.Capture(error); }
    }
}
