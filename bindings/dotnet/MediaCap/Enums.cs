namespace MediaCap;

public enum SourceType
{
    Unknown = 0,
    Monitor = 1,
    Window = 2,
    Camera = 3,
    AudioInput = 4,
    AudioOutput = 5,
    AudioProcess = 6,
    AudioMix = 7,
}

public enum ErrorCode
{
    None = 0,
    InvalidArgument = 1,
    DeviceUnavailable = 2,
    DeviceLost = 3,
    Timeout = 4,
    BufferOverflow = 5,
    CaptureFailed = 6,
    EncodingFailed = 7,
    Io = 8,
    Internal = 9,
    Unsupported = 10,
    PermissionDenied = 11,
}

public enum ErrorComponent
{
    Api = 0,
    Discovery = 1,
    Session = 2,
    Capture = 3,
    Mixer = 4,
    Encoder = 5,
}

public enum LogLevel
{
    None = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4,
    Verbose = 5,
}

public enum PixelFormat
{
    Unknown = -1,
    Bgra = 28,
    Bgr24 = 3,
    Rgba = 26,
    Rgb24 = 2,
    Nv12 = 23,
    I420 = 0,
}

public enum FrameOrientation
{
    TopDown = 0,
    BottomUp = 1,
}

public enum AudioDeviceType
{
    Unknown = 0,
    Input = 1,
    Output = 2,
    Loopback = 3,
}

public enum SampleFormat
{
    Unknown = -1,
    U8 = 0,
    S16 = 1,
    S32 = 2,
    Flt = 3,
    Dbl = 4,
    U8p = 5,
    S16p = 6,
    S32p = 7,
    Fltp = 8,
    Dblp = 9,
}

public enum ContainerFormat
{
    Default = 0,
    Mp4 = 1,
    Mkv = 2,
    Webm = 3,
    M4a = 4,
    Flac = 5,
    Mp3 = 6,
    Wav = 7,
    Opus = 8,
}

public enum VideoCodec
{
    Default = 0,
    H264 = 1,
    Vp9 = 2,
    Av1 = 3,
}

public enum AudioCodec
{
    Default = 0,
    Aac = 1,
    Opus = 2,
    Vorbis = 3,
    Flac = 4,
    Mp3 = 5,
    Pcm = 6,
}

public enum AudioCaptureKind
{
    Mic = 0,
    System = 1,
    Process = 2,
    Mix = 3,
}

public enum ProcessAudioMode
{
    Include = 0,
    Exclude = 1,
}

public enum AudioAvailability
{
    Unavailable = 0,
    Available = 1,
    AvailabilityUnknown = 2,
}

public enum AudioPidScope
{
    None = 0,
    Exact = 1,
    Tree = 2,
}
