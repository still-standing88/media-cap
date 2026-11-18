using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace MediaCap;

public sealed unsafe class CaptureSession : IDisposable
{
    private readonly object _mutations = new();
    private readonly SessionHandle _handle = new();
    private bool _disposed;

    private SessionHandle OpenHandle
    {
        get
        {
            ObjectDisposedException.ThrowIf(Volatile.Read(ref _disposed), this);
            return _handle;
        }
    }

    public ulong Id => Native.GetId(OpenHandle);
    public bool IsCapturing => Native.IsCapturing(OpenHandle) != 0;
    public bool IsRecording => Native.IsRecording(OpenHandle) != 0;
    public bool IsPaused => Native.IsPaused(OpenHandle) != 0;
    public bool HasError => Native.HasError(OpenHandle) != 0;

    public CaptureError? GetError()
    {
        NativeError error = default;
        return Native.GetError(OpenHandle, &error) != 0 ? CaptureError.Copy(error) : null;
    }

    public void Dispose()
    {
        CallbackScope.CheckMutation();
        lock (_mutations)
        {
            if (_disposed)
                return;
            Volatile.Write(ref _disposed, true);
            _handle.Dispose();
        }
    }

    public void AddSource(Monitor source, ScreenCaptureConfig config)
    {
        ArgumentNullException.ThrowIfNull(source);
        CallbackScope.CheckMutation();
        var value = source.NativeValue;
        lock (_mutations)
            Native.AddMonitorSource(OpenHandle, &value, &config);
    }

    public void AddSource(Window source, WindowCaptureConfig config)
    {
        ArgumentNullException.ThrowIfNull(source);
        CallbackScope.CheckMutation();
        var value = source.NativeValue;
        lock (_mutations)
            Native.AddWindowSource(OpenHandle, &value, &config);
    }

    public void AddSource(Camera source, CameraCaptureConfig config)
    {
        ArgumentNullException.ThrowIfNull(source);
        CallbackScope.CheckMutation();
        var value = source.NativeValue;
        lock (_mutations)
            Native.AddCameraSource(OpenHandle, &value, &config);
    }

    public void AddSource(AudioDevice source, AudioFormat config)
    {
        ArgumentNullException.ThrowIfNull(source);
        CallbackScope.CheckMutation();
        var value = source.NativeValue;
        lock (_mutations)
            Native.AddAudioSource(OpenHandle, &value, &config);
    }

    public bool AddAudioSource(AudioSourceConfig config)
    {
        ArgumentNullException.ThrowIfNull(config);
        CallbackScope.CheckMutation();
        var pids = config.ExcludePids.ToArray();
        fixed (byte* device = Utf8.Encode(config.DeviceId),
                     mic = Utf8.Encode(config.MixMicDeviceId),
                     system = Utf8.Encode(config.MixSystemDeviceId))
        fixed (uint* exclusions = pids)
        {
            var settings = new NativeAudioSourceConfig
            {
                Kind = config.Kind, DeviceId = device, ProcessId = config.ProcessId, ProcessMode = config.ProcessMode,
                ExcludeSelf = config.ExcludeSelf ? 1 : 0, ExcludePids = exclusions, ExcludePidCount = (nuint)pids.Length,
                MixMicDeviceId = mic, MixSystemDeviceId = system,
                Gain = config.Gain, MixMicGain = config.MixMicGain, MixSystemGain = config.MixSystemGain,
                SampleRate = config.SampleRate, Channels = config.Channels, RingCapacityChunks = config.RingCapacityChunks,
            };
            lock (_mutations)
                return Native.AddAudioCaptureSource(OpenHandle, &settings) != 0;
        }
    }

    public bool StartRecording(RecordingConfig config, long? durationMs = null)
    {
        ArgumentNullException.ThrowIfNull(config);
        CallbackScope.CheckMutation();
        return config.WithNative(settings =>
        {
            lock (_mutations)
                return durationMs.HasValue
                    ? Native.StartRecordingForDuration(OpenHandle, settings, durationMs.Value)
                    : Native.StartRecording(OpenHandle, settings);
        });
    }

    public bool StartCapture()
    {
        CallbackScope.CheckMutation();
        lock (_mutations)
            return Native.StartCapture(OpenHandle) != 0;
    }

    public void StopCapture()
    {
        CallbackScope.CheckMutation();
        lock (_mutations)
            Native.StopCapture(OpenHandle);
    }

    public void StopRecording()
    {
        CallbackScope.CheckMutation();
        lock (_mutations)
            Native.StopRecording(OpenHandle);
    }

    public void SetPaused(bool paused)
    {
        CallbackScope.CheckMutation();
        lock (_mutations)
            Native.SetPaused(OpenHandle, paused ? 1 : 0);
    }

    /// <summary>Sets handlers invoked on native capture threads, including during startup.</summary>
    public bool SetFrameCallbacks(Action<nuint, SourceType, AudioFrame>? audio = null,
        Action<nuint, SourceType, VideoFrame>? video = null)
    {
        CallbackScope.CheckMutation();
        var registration = new FrameCallbacks.Registration(audio, video);
        lock (_mutations)
        {
            var handle = OpenHandle;
            if (Native.SetFrameCallbacks(handle, audio is null ? null : &AudioReceived,
                    video is null ? null : &VideoReceived, _handle.CallbackToken) == 0)
                return false;
            Volatile.Write(ref _handle.Callbacks.Current, registration);
            return true;
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void AudioReceived(nuint source, SourceType type, NativeAudioFrameView* frame, nint context)
    {
        using var scope = new CallbackScope();
        try
        {
            if (GCHandle.FromIntPtr(context).Target is FrameCallbacks state)
                Volatile.Read(ref state.Current)?.Audio?.Invoke(source, type, AudioFrame.Copy(frame));
        }
        catch (Exception failure) { CallbackScope.Report(failure); }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void VideoReceived(nuint source, SourceType type, NativeVideoFrameView* frame, nint context)
    {
        using var scope = new CallbackScope();
        try
        {
            if (GCHandle.FromIntPtr(context).Target is FrameCallbacks state)
                Volatile.Read(ref state.Current)?.Video?.Invoke(source, type, VideoFrame.Copy(frame));
        }
        catch (Exception failure) { CallbackScope.Report(failure); }
    }
}
