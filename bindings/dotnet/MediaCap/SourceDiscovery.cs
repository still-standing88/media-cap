namespace MediaCap;

public static unsafe class SourceDiscovery
{
    public static FeatureRequirements GetFeatureRequirements(CaptureFeature feature)
    {
        NativeFeatureRequirements result = default;
        if (Native.GetFeatureRequirements(feature, &result) == 0)
            throw new ArgumentOutOfRangeException(nameof(feature));
        return new FeatureRequirements(result.Platform, result.IncludedInBuild != 0, result.Prerequisites,
            result.MinimumOsMajor, result.MinimumOsMinor, result.MinimumOsBuild, Utf8.Decode(result.Backend, 32));
    }

    private delegate int Query<T>(T* buffer, int* count) where T : unmanaged;
    private delegate void Cleanup<T>(T* item) where T : unmanaged;
    private delegate int AllocatedQuery<T>(T** buffer, nuint* count) where T : unmanaged;
    private delegate void AllocatedCleanup<T>(T* buffer, nuint count) where T : unmanaged;

    public static IReadOnlyList<Monitor> GetMonitors() =>
        Read<NativeMonitor, Monitor>(Native.GetMonitors, value => new Monitor(value));

    public static IReadOnlyList<Window> GetWindows() =>
        Read<NativeWindow, Window>(Native.GetWindows, value => new Window(value));

    public static IReadOnlyList<Camera> GetCameraDevices() =>
        Read<NativeCamera, Camera>(Native.GetCameraDevices, value => new Camera(value));

    public static IReadOnlyList<AudioDevice> GetAudioDevices() =>
        Read<NativeAudioDevice, AudioDevice>(Native.GetAudioDevices, value => new AudioDevice(value));

    public static IReadOnlyList<CameraDetail> GetCameras() =>
        Read<NativeCameraInfo, CameraDetail>(Native.GetCameras, value => new CameraDetail(
            new Camera(value.Camera), Copy(value.SupportedResolutions, value.NumSupportedResolutions),
            Copy(value.SupportedPixelFormats, value.NumSupportedPixelFormats)), Native.FreeCameraInfoContents);

    public static IReadOnlyList<AudioCaptureDevice> GetAudioCaptureDevices() =>
        ReadAllocated<NativeAudioCaptureDevice, AudioCaptureDevice>(
            Native.GetAudioCaptureDevices, Native.FreeAudioCaptureDevices,
            value => new AudioCaptureDevice(Utf8.DecodeId(value.Id), Utf8.Decode(value.Name) ?? "",
                value.Kind, value.SampleRate, value.Channels, value.IsDefault != 0));

    public static IReadOnlyList<AudioProcess> GetAudioProcesses() =>
        ReadAllocated<NativeAudioProcess, AudioProcess>(Native.GetAudioProcesses, Native.FreeAudioProcesses,
            value => new AudioProcess(value.Pid, Utf8.Decode(value.Name) ?? "", Utf8.Decode(value.Executable),
                value.OutputActive < 0 ? null : value.OutputActive != 0));

    public static AudioCapabilities GetAudioCapabilities()
    {
        NativeAudioCapabilities value = default;
        if (Native.GetAudioCapabilities(&value) == 0)
            throw QueryFailed();
        return new AudioCapabilities(value.BackendEnabled != 0, value.Microphone, value.System, value.Process,
            value.Mix, value.SystemExclusions, value.ProcessScope, value.ExclusionScope,
            value.ExclusionHonorsDevice != 0, value.MaxExcludedPids, Utf8.Decode(value.ProcessDetail, 256),
            Utf8.Decode(value.SystemDetail, 256));
    }

    private static TResult[] Read<T, TResult>(Query<T> query, Func<T, TResult> project, Cleanup<T>? free = null)
        where T : unmanaged
    {
        int capacity = 0;
        if (query(null, &capacity) != 0 || capacity < 0)
            throw QueryFailed();
        if (capacity == 0)
            return [];

        for (int attempt = 0; ; attempt++)
        {
            var items = new T[capacity];
            fixed (T* ptr = items)
            {
                try
                {
                    int count = capacity;
                    if (query(ptr, &count) != 0 || count < 0 || count > capacity)
                        throw QueryFailed();

                    if (count == capacity && attempt < 2)
                    {
                        int latest = 0;
                        if (query(null, &latest) != 0)
                            throw QueryFailed();
                        if (latest > capacity)
                        {
                            capacity = latest;
                            continue;
                        }
                    }

                    var result = new TResult[count];
                    for (int index = 0; index < count; index++)
                        result[index] = project(items[index]);
                    return result;
                }
                finally
                {
                    if (free is not null)
                        for (int index = 0; index < items.Length; index++)
                            free(ptr + index);
                }
            }
        }
    }

    private static TResult[] ReadAllocated<T, TResult>(AllocatedQuery<T> query, AllocatedCleanup<T> free,
        Func<T, TResult> project) where T : unmanaged
    {
        T* items = null;
        nuint count = 0;
        try
        {
            if (query(&items, &count) == 0)
            {
                if (!GetAudioCapabilities().BackendEnabled)
                    return [];
                throw QueryFailed();
            }
            if (items is null && count != 0)
                throw QueryFailed();

            var result = new TResult[checked((int)count)];
            for (int index = 0; index < result.Length; index++)
                result[index] = project(items[index]);
            return result;
        }
        finally
        {
            free(items, count);
        }
    }

    private static T[] Copy<T>(T* source, nuint count) where T : unmanaged
    {
        if (source is null && count != 0)
            throw QueryFailed();
        return new ReadOnlySpan<T>(source, checked((int)count)).ToArray();
    }

    private static InvalidOperationException QueryFailed() =>
        new("MediaCap discovery failed. Read diagnostics for the reported native error.");
}
