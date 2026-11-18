using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace MediaCap;

internal static unsafe partial class Native
{
    private const string Library = "MediaCap.Native";

    [LibraryImport(Library, EntryPoint = "MediaCap_GetFeatureRequirements")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetFeatureRequirements(CaptureFeature feature, NativeFeatureRequirements* result);

    [LibraryImport(Library, EntryPoint = "MediaCap_DefaultAudioSourceConfig")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial NativeAudioSourceConfig DefaultAudioSourceConfig();

    [LibraryImport(Library, EntryPoint = "MediaCap_GetAudioCapabilities")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetAudioCapabilities(NativeAudioCapabilities* result);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetAudioCaptureDevices")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetAudioCaptureDevices(NativeAudioCaptureDevice** result, nuint* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_FreeAudioCaptureDevices")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void FreeAudioCaptureDevices(NativeAudioCaptureDevice* devices, nuint count);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetAudioProcesses")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetAudioProcesses(NativeAudioProcess** result, nuint* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_FreeAudioProcesses")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void FreeAudioProcesses(NativeAudioProcess* processes, nuint count);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_AddAudioCaptureSource")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int AddAudioCaptureSource(SessionHandle session, NativeAudioSourceConfig* config);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_Create")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial nint CreateSession();

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_Destroy")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void DestroySession(nint handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetMonitors")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetMonitors(NativeMonitor* buffer, int* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetWindows")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetWindows(NativeWindow* buffer, int* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetCameraDevices")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetCameraDevices(NativeCamera* buffer, int* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetCameras")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetCameras(NativeCameraInfo* buffer, int* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_FreeCameraInfoContents")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void FreeCameraInfoContents(NativeCameraInfo* info);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetAudioDevices")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetAudioDevices(NativeAudioDevice* buffer, int* count);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_AddMonitorSource")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void AddMonitorSource(SessionHandle handle, NativeMonitor* monitor, ScreenCaptureConfig* config);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_AddWindowSource")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void AddWindowSource(SessionHandle handle, NativeWindow* window, WindowCaptureConfig* config);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_AddCameraSource")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void AddCameraSource(SessionHandle handle, NativeCamera* camera, CameraCaptureConfig* config);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_AddAudioSource")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void AddAudioSource(SessionHandle handle, NativeAudioDevice* device, AudioFormat* format);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_SetFrameCallbacks")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int SetFrameCallbacks(SessionHandle handle, delegate* unmanaged[Cdecl]<nuint, SourceType, NativeAudioFrameView*, nint, void> audio_callback, delegate* unmanaged[Cdecl]<nuint, SourceType, NativeVideoFrameView*, nint, void> video_callback, nint user_data);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_StartCapture")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int StartCapture(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_StopCapture")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void StopCapture(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_IsCapturing")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int IsCapturing(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_CaptureMonitorScreenshot")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int CaptureMonitorScreenshot(NativeMonitor* monitor, ScreenshotConfig* config, delegate* unmanaged[Cdecl]<NativeVideoFrameView*, nint, void> callback, nint user_data);

    [LibraryImport(Library, EntryPoint = "MediaCap_CaptureWindowScreenshot")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int CaptureWindowScreenshot(NativeWindow* window, ScreenshotConfig* config, delegate* unmanaged[Cdecl]<NativeVideoFrameView*, nint, void> callback, nint user_data);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_StartRecording")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int StartRecording(SessionHandle handle, NativeRecordingConfig* config);

    [LibraryImport(Library, EntryPoint = "MediaCap_ValidateRecordingFormat")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int ValidateRecordingFormat(NativeRecordingConfig* config);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_StartRecordingForDuration")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int StartRecordingForDuration(SessionHandle handle, NativeRecordingConfig* config, long duration_ms);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_StopRecording")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void StopRecording(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_SetPaused")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void SetPaused(SessionHandle handle, int is_paused);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_IsRecording")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int IsRecording(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_IsPaused")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int IsPaused(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_SetErrorCallback")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void SetErrorCallback(delegate* unmanaged[Cdecl]<NativeError*, nint, void> callback, nint user_data);

    [LibraryImport(Library, EntryPoint = "MediaCap_GetLastError")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetLastError(NativeError* error);

    [LibraryImport(Library, EntryPoint = "MediaCap_ClearLastError")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial void ClearLastError();

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_GetId")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial ulong GetId(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_HasError")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int HasError(SessionHandle handle);

    [LibraryImport(Library, EntryPoint = "MediaCap_Session_GetError")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static partial int GetError(SessionHandle handle, NativeError* error);
}
