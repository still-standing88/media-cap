using System.Runtime.InteropServices;

namespace MediaCap;

public enum Platform { Unknown, Windows, MacOS, Linux }

public enum CaptureFeature
{
    Screen, Window, Camera, Microphone, SystemAudio, ProcessAudio, MixAudio, AudioExclusion, ClassicAudio,
}

[Flags]
public enum Prerequisite : uint
{
    None = 0,
    DisplaySession = 1u << 0,
    X11 = 1u << 1,
    CameraDevice = 1u << 2,
    AudioInput = 1u << 3,
    AudioOutput = 1u << 4,
    PipeWire = 1u << 5,
    ScreenPermission = 1u << 6,
    CameraPermission = 1u << 7,
    MicrophonePermission = 1u << 8,
    SystemAudioPermission = 1u << 9,
    ProcessId = 1u << 10,
    FlexAudio = 1u << 11,
}

public sealed record FeatureRequirements(Platform Platform, bool IncludedInBuild, Prerequisite Prerequisites,
    uint MinimumOsMajor, uint MinimumOsMinor, uint MinimumOsBuild, string Backend);

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeFeatureRequirements
{
    public Platform Platform;
    public int IncludedInBuild;
    public Prerequisite Prerequisites;
    public uint MinimumOsMajor;
    public uint MinimumOsMinor;
    public uint MinimumOsBuild;
    public fixed byte Backend[32];
}
