using System.Runtime.InteropServices;

namespace MediaCap;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeError
{
    public ErrorCode Code;
    public ErrorComponent Component;
    public SourceType SourceType;
    public int SourceIndex;
    public ulong SessionId;
    public long NativeCode;
    public fixed byte Message[512];
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct Resolution
{
    public uint Width;
    public uint Height;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeMonitor
{
    public int Id;
    public int Index;
    public int OffsetX;
    public int OffsetY;
    public uint Width;
    public uint Height;
    public float Scaling;
    public fixed byte Name[128];
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeWindow
{
    public ulong Handle;
    public int OffsetX;
    public int OffsetY;
    public uint Width;
    public uint Height;
    public fixed byte Name[256];
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeCamera
{
    public int Index;
    public fixed byte Name[256];
    public fixed byte UniqueId[256];
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeCameraInfo
{
    public NativeCamera Camera;
    public Resolution* SupportedResolutions;
    public nuint NumSupportedResolutions;
    public PixelFormat* SupportedPixelFormats;
    public nuint NumSupportedPixelFormats;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CameraCaptureConfig
{
    public uint Width;
    public uint Height;
    public int Framerate;
    public PixelFormat PixelFormat;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScreenCaptureConfig
{
    public int CaptureRateMs;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct WindowCaptureConfig
{
    public int CaptureRateMs;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeAudioDevice
{
    public fixed byte Id[256];
    public fixed byte Name[256];
    public AudioDeviceType Type;
    public int IsDefault;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct AudioFormat
{
    public SampleFormat Format;
    public uint SampleRate;
    public byte Channels;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeVideoFrameView
{
    public byte* Data0;
    public byte* Data1;
    public byte* Data2;
    public byte* Data3;
    public fixed int Linesize[4];
    public uint Width;
    public uint Height;
    public PixelFormat PixelFormat;
    public FrameOrientation Orientation;
    public long TimestampNs;
    public ulong FrameIndex;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeAudioFrameView
{
    public byte* Data;
    public uint NumSamples;
    public AudioFormat Format;
    public long TimestampNs;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct ScreenshotConfig
{
    public uint X;
    public uint Y;
    public uint Width;
    public uint Height;
    public uint OutputWidth;
    public uint OutputHeight;
    public uint TimeoutMs;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct VideoEncoderConfig
{
    public uint Width;
    public uint Height;
    public int Framerate;
    public long BitrateBps;
    public VideoCodec Codec;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct AudioEncoderConfig
{
    public uint SampleRate;
    public byte Channels;
    public long BitrateBps;
    public AudioCodec Codec;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeRecordingConfig
{
    public byte* OutputFilename;
    public ContainerFormat Container;
    public VideoEncoderConfig* VideoConfig;
    public AudioEncoderConfig* AudioConfig;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeAudioSourceConfig
{
    public AudioCaptureKind Kind;
    public byte* DeviceId;
    public uint ProcessId;
    public ProcessAudioMode ProcessMode;
    public int ExcludeSelf;
    public uint* ExcludePids;
    public nuint ExcludePidCount;
    public byte* MixMicDeviceId;
    public byte* MixSystemDeviceId;
    public float Gain;
    public float MixMicGain;
    public float MixSystemGain;
    public uint SampleRate;
    public ushort Channels;
    public nuint RingCapacityChunks;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeAudioCaptureDevice
{
    public byte* Id;
    public byte* Name;
    public AudioCaptureKind Kind;
    public uint SampleRate;
    public ushort Channels;
    public int IsDefault;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeAudioProcess
{
    public uint Pid;
    public byte* Name;
    public byte* Executable;
    public int OutputActive;
}

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeAudioCapabilities
{
    public int BackendEnabled;
    public AudioAvailability Microphone;
    public AudioAvailability System;
    public AudioAvailability Process;
    public AudioAvailability Mix;
    public AudioAvailability SystemExclusions;
    public AudioPidScope ProcessScope;
    public AudioPidScope ExclusionScope;
    public int ExclusionHonorsDevice;
    public uint MaxExcludedPids;
    public fixed byte ProcessDetail[256];
    public fixed byte SystemDetail[256];
}
