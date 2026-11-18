namespace MediaCap;

public sealed class Monitor
{
    internal readonly NativeMonitor NativeValue;

    internal unsafe Monitor(NativeMonitor value)
    {
        NativeValue = value;
        Name = Utf8.Decode(value.Name, 128);
    }

    public int Id => NativeValue.Id;
    public int Index => NativeValue.Index;
    public int OffsetX => NativeValue.OffsetX;
    public int OffsetY => NativeValue.OffsetY;
    public uint Width => NativeValue.Width;
    public uint Height => NativeValue.Height;
    public float Scaling => NativeValue.Scaling;
    public string Name { get; }
}

public sealed class Window
{
    internal readonly NativeWindow NativeValue;

    internal unsafe Window(NativeWindow value)
    {
        NativeValue = value;
        Name = Utf8.Decode(value.Name, 256);
    }

    public ulong Handle => NativeValue.Handle;
    public int OffsetX => NativeValue.OffsetX;
    public int OffsetY => NativeValue.OffsetY;
    public uint Width => NativeValue.Width;
    public uint Height => NativeValue.Height;
    public string Name { get; }
}

public sealed class Camera
{
    internal readonly NativeCamera NativeValue;

    internal unsafe Camera(NativeCamera value)
    {
        NativeValue = value;
        Name = Utf8.Decode(value.Name, 256);
        UniqueId = Utf8.Decode(value.UniqueId, 256);
    }

    public int Index => NativeValue.Index;
    public string Name { get; }
    public string UniqueId { get; }
}

public sealed class AudioDevice
{
    internal readonly NativeAudioDevice NativeValue;

    internal unsafe AudioDevice(NativeAudioDevice value)
    {
        NativeValue = value;
        Name = Utf8.Decode(value.Name, 256);
        Id = new ReadOnlySpan<byte>(value.Id, 256).ToArray();
    }

    public ReadOnlyMemory<byte> Id { get; }
    public string Name { get; }
    public AudioDeviceType Type => NativeValue.Type;
    public bool IsDefault => NativeValue.IsDefault != 0;
}

public sealed record CameraDetail(Camera Camera, IReadOnlyList<Resolution> SupportedResolutions,
    IReadOnlyList<PixelFormat> SupportedPixelFormats);

public sealed record AudioCaptureDevice(string Id, string Name, AudioCaptureKind Kind, uint SampleRate,
    ushort Channels, bool IsDefault);

public sealed record AudioProcess(uint Pid, string Name, string? Executable, bool? OutputActive);

public sealed record AudioCapabilities(bool BackendEnabled, AudioAvailability Microphone, AudioAvailability System,
    AudioAvailability Process, AudioAvailability Mix, AudioAvailability SystemExclusions,
    AudioPidScope ProcessScope, AudioPidScope ExclusionScope, bool ExclusionHonorsDevice, uint MaxExcludedPids,
    string ProcessDetail, string SystemDetail);

public sealed record RecordingConfig(string OutputFilename)
{
    public ContainerFormat Container { get; init; }
    public VideoEncoderConfig? VideoConfig { get; init; }
    public AudioEncoderConfig? AudioConfig { get; init; }

    /// <summary>Checks the container and encoder settings without opening devices or files.</summary>
    public unsafe bool ValidateFormat() => WithNative(Native.ValidateRecordingFormat);

    internal unsafe delegate int RecordingCall(NativeRecordingConfig* config);

    internal unsafe bool WithNative(RecordingCall call)
    {
        ArgumentNullException.ThrowIfNull(OutputFilename);
        var video = VideoConfig.GetValueOrDefault();
        var audio = AudioConfig.GetValueOrDefault();
        fixed (byte* filename = Utf8.Encode(OutputFilename))
        {
            var settings = new NativeRecordingConfig
            {
                OutputFilename = filename, Container = Container,
                VideoConfig = VideoConfig.HasValue ? &video : null,
                AudioConfig = AudioConfig.HasValue ? &audio : null,
            };
            return call(&settings) != 0;
        }
    }
}

public sealed record AudioSourceConfig
{
    public AudioCaptureKind Kind { get; init; }
    public string? DeviceId { get; init; }
    public uint ProcessId { get; init; }
    public ProcessAudioMode ProcessMode { get; init; }
    public bool ExcludeSelf { get; init; }
    public ReadOnlyMemory<uint> ExcludePids { get; init; }
    public string? MixMicDeviceId { get; init; }
    public string? MixSystemDeviceId { get; init; }
    public float Gain { get; init; } = 1;
    public float MixMicGain { get; init; } = 1;
    public float MixSystemGain { get; init; } = 1;
    public uint SampleRate { get; init; }
    public ushort Channels { get; init; }
    public nuint RingCapacityChunks { get; init; }

    public static AudioSourceConfig CreateDefault()
    {
        var config = Native.DefaultAudioSourceConfig();
        return new AudioSourceConfig
        {
            Kind = config.Kind,
            ProcessId = config.ProcessId,
            ProcessMode = config.ProcessMode,
            ExcludeSelf = config.ExcludeSelf != 0,
            Gain = config.Gain,
            MixMicGain = config.MixMicGain,
            MixSystemGain = config.MixSystemGain,
            SampleRate = config.SampleRate,
            Channels = config.Channels,
            RingCapacityChunks = config.RingCapacityChunks,
        };
    }
}
