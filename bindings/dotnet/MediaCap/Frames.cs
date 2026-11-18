namespace MediaCap;

public sealed record VideoPlane(ReadOnlyMemory<byte> Data, int Stride);

public sealed record VideoFrame(uint Width, uint Height, PixelFormat PixelFormat, FrameOrientation Orientation,
    long TimestampNs, ulong FrameIndex, IReadOnlyList<VideoPlane> Planes)
{
    internal static unsafe VideoFrame Copy(NativeVideoFrameView* frame)
    {
        if (frame is null || frame->Width == 0 || frame->Height == 0)
            throw new InvalidDataException("Invalid MediaCap video frame.");

        int width = checked((int)frame->Width), height = checked((int)frame->Height);
        int chromaWidth = width / 2 + width % 2, chromaHeight = height / 2 + height % 2;
        VideoPlane[] planes = frame->PixelFormat switch
        {
            PixelFormat.Bgra or PixelFormat.Rgba => [CopyPlane(frame->Data0, frame->Linesize[0], checked(width * 4), height)],
            PixelFormat.Bgr24 or PixelFormat.Rgb24 => [CopyPlane(frame->Data0, frame->Linesize[0], checked(width * 3), height)],
            PixelFormat.Nv12 => [
                CopyPlane(frame->Data0, frame->Linesize[0], width, height),
                CopyPlane(frame->Data1, frame->Linesize[1], checked(chromaWidth * 2), chromaHeight)],
            PixelFormat.I420 => [
                CopyPlane(frame->Data0, frame->Linesize[0], width, height),
                CopyPlane(frame->Data1, frame->Linesize[1], chromaWidth, chromaHeight),
                CopyPlane(frame->Data2, frame->Linesize[2], chromaWidth, chromaHeight)],
            _ => throw new InvalidDataException("Unsupported MediaCap video frame format."),
        };

        return new(frame->Width, frame->Height, frame->PixelFormat, frame->Orientation,
            frame->TimestampNs, frame->FrameIndex, planes);
    }

    private static unsafe VideoPlane CopyPlane(byte* source, int stride, int rowBytes, int rows)
    {
        long distance = Math.Abs((long)stride);
        if (source is null || distance < rowBytes)
            throw new InvalidDataException("Invalid MediaCap video plane.");

        _ = checked((nint)(distance * (rows - 1) + rowBytes));
        var destination = GC.AllocateUninitializedArray<byte>(checked(rowBytes * rows));
        for (int row = 0; row < rows; row++)
        {
            var offset = checked((nint)((long)row * stride));
            new ReadOnlySpan<byte>(source + offset, rowBytes).CopyTo(destination.AsSpan(row * rowBytes, rowBytes));
        }
        return new VideoPlane(destination, rowBytes);
    }
}

public sealed record AudioFrame(ReadOnlyMemory<byte> Data, uint NumSamples, AudioFormat Format, long TimestampNs)
{
    internal static unsafe AudioFrame Copy(NativeAudioFrameView* frame)
    {
        if (frame is null || frame->Data is null || frame->Format.SampleRate == 0 || frame->Format.Channels == 0)
            throw new InvalidDataException("Invalid MediaCap audio frame.");

        int sampleBytes = frame->Format.Format switch
        {
            SampleFormat.U8 => 1,
            SampleFormat.S16 => 2,
            SampleFormat.S32 or SampleFormat.Flt => 4,
            SampleFormat.Dbl => 8,
            _ => throw new InvalidDataException("Unsupported MediaCap interleaved audio frame format."),
        };
        int size = checked(checked((int)frame->NumSamples) * frame->Format.Channels * sampleBytes);
        var samples = new ReadOnlySpan<byte>(frame->Data, size).ToArray();
        return new AudioFrame(samples, frame->NumSamples, frame->Format, frame->TimestampNs);
    }
}
