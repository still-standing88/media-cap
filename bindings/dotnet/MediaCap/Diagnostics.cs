using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace MediaCap;

public sealed record CaptureError(ErrorCode Code, ErrorComponent Component, SourceType SourceType,
    int SourceIndex, ulong SessionId, long NativeCode, string Message)
{
    internal static unsafe CaptureError Copy(NativeError error) =>
        new(error.Code, error.Component, error.SourceType, error.SourceIndex, error.SessionId,
            error.NativeCode, Utf8.Decode(error.Message, 512));
}

internal readonly struct CallbackScope : IDisposable
{
    [ThreadStatic]
    private static int _depth;

    public CallbackScope() => ++_depth;
    public void Dispose() => --_depth;

    internal static void CheckMutation()
    {
        if (_depth != 0)
            throw new InvalidOperationException("MediaCap session mutations are unavailable inside native callbacks.");
    }

    internal static void Report(Exception exception)
    {
        try { Trace.TraceError("MediaCap callback failed: {0}", exception); }
        catch { /* A trace listener must not throw through the native callback. */ }
    }
}

public static unsafe class Diagnostics
{
    private static readonly object RegistrationLock = new();
    private static Action<CaptureError>? _callback;

    public static CaptureError? GetLastError()
    {
        NativeError error = default;
        return Native.GetLastError(&error) != 0 ? CaptureError.Copy(error) : null;
    }

    public static void ClearLastError() => Native.ClearLastError();

    /// <summary>Sets the process-wide listener. In-flight invocations can finish after replacement or removal.</summary>
    public static void SetErrorCallback(Action<CaptureError>? callback)
    {
        lock (RegistrationLock)
        {
            // The native slot stores only a static function pointer. The target lives in managed memory.
            Volatile.Write(ref _callback, callback);
            Native.SetErrorCallback(callback is null ? null : &ReportError, 0);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void ReportError(NativeError* error, nint context)
    {
        using var scope = new CallbackScope();
        try
        {
            var callback = Volatile.Read(ref _callback);
            if (error is not null && callback is not null)
                callback(CaptureError.Copy(*error));
        }
        catch (Exception failure) { CallbackScope.Report(failure); }
    }
}
