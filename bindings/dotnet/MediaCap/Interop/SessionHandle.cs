using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace MediaCap;

internal sealed class FrameCallbacks
{
    internal sealed record Registration(Action<nuint, SourceType, AudioFrame>? Audio,
        Action<nuint, SourceType, VideoFrame>? Video);

    internal Registration? Current;
}

internal sealed class SessionHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    private GCHandle _token;
    internal FrameCallbacks Callbacks { get; } = new();
    internal nint CallbackToken => GCHandle.ToIntPtr(_token);

    internal SessionHandle() : base(true)
    {
        SetHandle(Native.CreateSession());
        if (IsInvalid)
            throw new InvalidOperationException("MediaCap could not create a session.");

        // Native code holds the token until Destroy completes. A weak target lets finalization
        // reclaim the session even when an installed handler closes over the session itself.
        try { _token = GCHandle.Alloc(Callbacks, GCHandleType.Weak); }
        catch
        {
            Dispose();
            throw;
        }
    }

    protected override bool ReleaseHandle()
    {
        Native.DestroySession(handle);
        if (_token.IsAllocated)
            _token.Free();
        Volatile.Write(ref Callbacks.Current, null);
        return true;
    }
}
