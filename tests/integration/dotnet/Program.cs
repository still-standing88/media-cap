using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using MediaCap;

internal static class Program
{
    private static int Main(string[] args)
    {
        if (Array.IndexOf(args, "--abi") is int index && index >= 0)
            VerifyAbi(args[index + 1]);
        VerifyMediaCopies();
        VerifyFormatPreflight();
        VerifyRequirements();
        VerifySession();
        VerifyInFlightDiagnostics();
        if (args.Contains("--screen"))
            VerifyScreen();
        Console.WriteLine("MediaCap .NET checks passed.");
        return 0;
    }

    private static void VerifyAbi(string executable)
    {
        using var probe = Process.Start(new ProcessStartInfo(executable) { RedirectStandardOutput = true })!;
        string output = probe.StandardOutput.ReadToEnd();
        probe.WaitForExit();
        Check(probe.ExitCode == 0, "Native ABI probe failed.");
        var assembly = typeof(CaptureSession).Assembly;
        foreach (string line in output.Split('\n', StringSplitOptions.RemoveEmptyEntries))
        {
            var row = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
            string name = row[0]["MediaCap_".Length..];
            var type = assembly.GetType($"MediaCap.Native{name}") ?? assembly.GetType($"MediaCap.{name}", true)!;
            int actual = row.Length == 2
                ? Marshal.SizeOf(type)
                : Marshal.OffsetOf(type, NativeFieldName(name, row[1])).ToInt32();
            Check(actual == int.Parse(row[^1]), $"ABI mismatch for {line.Trim()}: managed {actual}.");
        }
    }

    private static string NativeFieldName(string type, string field) =>
        type == "VideoFrameView" && field == "data" ? "Data0" :
        string.Concat(field.Split('_').Select(part => char.ToUpperInvariant(part[0]) + part[1..]));

    private static unsafe void VerifyMediaCopies()
    {
        byte* packed = stackalloc byte[] { 7, 8, 9, 44, 1, 2, 3, 55 };
        var view = new NativeVideoFrameView
        {
            Data0 = packed + 4, Width = 1, Height = 2, PixelFormat = PixelFormat.Rgb24,
            Orientation = FrameOrientation.BottomUp, FrameIndex = 7,
        };
        view.Linesize[0] = -4;
        var frame = VideoFrame.Copy(&view);
        packed[4] = 99;
        Check(frame.Planes[0].Stride == 3 && frame.Planes[0].Data.Span.SequenceEqual(new byte[] { 1, 2, 3, 7, 8, 9 })
              && frame.Orientation == FrameOrientation.BottomUp && frame.FrameIndex == 7, "Packed video ownership or stride.");

        byte* luma = stackalloc byte[] { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        byte* chroma = stackalloc byte[] { 11, 12, 13, 14, 15, 16, 17, 18 };
        view = new NativeVideoFrameView { Data0 = luma, Data1 = chroma, Width = 3, Height = 3, PixelFormat = PixelFormat.Nv12 };
        view.Linesize[0] = 3;
        view.Linesize[1] = 4;
        frame = VideoFrame.Copy(&view);
        Check(frame.Planes[1].Stride == 4 && frame.Planes[1].Data.Length == 8, "Odd NV12 chroma layout.");
        view.Linesize[1] = 3;
        bool invalid = false;
        try { VideoFrame.Copy(&view); }
        catch (InvalidDataException) { invalid = true; }
        Check(invalid, "Invalid video stride was accepted.");

        byte* audio = stackalloc byte[] { 2, 3, 4, 5 };
        var audioView = new NativeAudioFrameView
        {
            Data = audio, NumSamples = 1,
            Format = new AudioFormat { Format = SampleFormat.S16, Channels = 2, SampleRate = 48000 },
        };
        var samples = AudioFrame.Copy(&audioView);
        audio[0] = 77;
        Check(samples.Data.Span.SequenceEqual(new byte[] { 2, 3, 4, 5 }), "Audio frame ownership.");
        Check(Utf8.Encode("écran-画面")![^1] == 0, "Missing UTF-8 terminator.");
        Expect<ArgumentException>(() => Utf8.Encode("bad\0path"));
    }

    private static void VerifyRequirements()
    {
        var screen = SourceDiscovery.GetFeatureRequirements(CaptureFeature.Screen);
        var process = SourceDiscovery.GetFeatureRequirements(CaptureFeature.ProcessAudio);
        Check(screen.IncludedInBuild && screen.Prerequisites.HasFlag(Prerequisite.DisplaySession), "Screen prerequisites.");
        Check(process.Platform == screen.Platform && process.Prerequisites.HasFlag(Prerequisite.FlexAudio), "Process prerequisites.");
        if (process.Platform == Platform.Windows && process.IncludedInBuild)
            Check(process.MinimumOsBuild == 20348, "Windows process audio minimum build.");
        Expect<ArgumentOutOfRangeException>(() => SourceDiscovery.GetFeatureRequirements((CaptureFeature)99));
        Diagnostics.ClearLastError();
    }

    private static void VerifySession()
    {
        using var session = new CaptureSession();
        Check(session.Id > 0 && !session.IsCapturing && !session.IsRecording && !session.HasError, "Initial session state.");
        var config = AudioSourceConfig.CreateDefault();
        Check(config.Kind == AudioCaptureKind.Mic && config.Gain == 1 && config.MixSystemGain == 1, "Audio defaults.");
        var caps = SourceDiscovery.GetAudioCapabilities();
        if (!caps.BackendEnabled)
            Check(SourceDiscovery.GetAudioCaptureDevices().Count == 0 && SourceDiscovery.GetAudioProcesses().Count == 0,
                "Disabled FlexAudio discovery.");

        var first = new List<CaptureError>();
        var second = new List<CaptureError>();
        Diagnostics.SetErrorCallback(first.Add);
        try
        {
            Check(!session.StartCapture(), "Capture started without a source.");
            Check(session.GetError()?.Code == ErrorCode.InvalidArgument && first.Count == 1 && first[0].SessionId == session.Id,
                "Session diagnostic was lost.");
            Diagnostics.SetErrorCallback(second.Add);
            Check(!session.StartCapture() && first.Count == 1 && second.Count == 1, "Diagnostic replacement.");
            Diagnostics.ClearLastError();
            Check(Diagnostics.GetLastError() is null && session.HasError, "Clearing the global error changed the session error.");
        }
        finally { Diagnostics.SetErrorCallback(null); }

        Expect<ArgumentException>(() => session.StartRecording(new RecordingConfig("bad\0path")));
        var racing = Task.Run(() =>
        {
            for (int i = 0; i < 500; i++)
            {
                try { Check(!session.IsCapturing, "Unexpected capture state."); }
                catch (ObjectDisposedException) { return; }
            }
        });
        session.Dispose();
        Check(racing.Wait(TimeSpan.FromSeconds(5)), "Status polling did not finish.");
        Expect<ObjectDisposedException>(() => session.StartCapture());

        var cycle = CreateCallbackCycle();
        for (int i = 0; i < 3 && cycle.IsAlive; i++)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
        Check(!cycle.IsAlive, "The native callback token rooted the session.");
    }

    private static void VerifyFormatPreflight()
    {
        var config = new RecordingConfig("écran-画面-🎬.mkv") { VideoConfig = new VideoEncoderConfig() };
        Check(config.ValidateFormat(), "UTF-8 filename could not be validated.");
        Check(!(config with { OutputFilename = "écran-画面-🎬.wav" }).ValidateFormat(), "Video was accepted for WAV.");
        Check(Diagnostics.GetLastError()?.Message.Contains("Video sources") == true, "Missing video format diagnostic.");
        Expect<System.Text.EncoderFallbackException>(() => (config with { OutputFilename = "bad\uDCFF.mkv" }).ValidateFormat());
        Diagnostics.ClearLastError();
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference CreateCallbackCycle()
    {
        var session = new CaptureSession();
        Check(session.SetFrameCallbacks(video: (_, _, _) => _ = session.Id), "Callback registration failed.");
        return new WeakReference(session);
    }

    private static void VerifyInFlightDiagnostics()
    {
        using var first = new CaptureSession();
        using var second = new CaptureSession();
        using var arrived = new ManualResetEventSlim(false);
        using var release = new ManualResetEventSlim(false);
        CaptureError? oldError = null;
        CaptureError? newError = null;
        Diagnostics.SetErrorCallback(error =>
        {
            arrived.Set();
            if (release.Wait(TimeSpan.FromSeconds(10)))
                oldError = error;
        });
        var report = Task.Run(first.StartCapture);
        try
        {
            Check(arrived.Wait(TimeSpan.FromSeconds(5)), "First diagnostic did not arrive.");
            Diagnostics.SetErrorCallback(error => newError = error);
            Check(!second.StartCapture() && newError?.SessionId == second.Id, "Replacement did not receive new error.");
            Diagnostics.SetErrorCallback(null);
        }
        finally
        {
            release.Set();
            report.GetAwaiter().GetResult();
            Diagnostics.SetErrorCallback(null);
        }
        Check(oldError?.SessionId == first.Id, "Unregister invalidated the in-flight diagnostic.");
    }

    private static void VerifyScreen()
    {
        var monitor = SourceDiscovery.GetMonitors().First();
        var screenshot = Screenshots.Capture(monitor, new ScreenshotConfig { OutputWidth = 64, OutputHeight = 32 });
        Check(screenshot is { Width: 64, Height: 32 } && screenshot.Planes[0].Data.Length == 64 * 32 * 4, "Screenshot layout.");

        var config = new ScreenCaptureConfig { CaptureRateMs = 33 };
        using var session = new CaptureSession();
        using var ready = new ManualResetEventSlim(false);
        VideoFrame? retained = null;
        bool rejected = false;
        session.AddSource(monitor, config);
        Check(session.SetFrameCallbacks(video: (index, type, frame) =>
        {
            if (index != 0 || type != SourceType.Monitor || session.Id == 0)
                return;
            retained = frame;
            try { session.StopCapture(); }
            catch (InvalidOperationException) { rejected = true; }
            ready.Set();
        }), "Frame callback registration failed.");
        Check(session.StartCapture() && ready.Wait(TimeSpan.FromSeconds(5)), "Frame callback was not delivered.");
        session.StopCapture();
        Check(retained is not null && retained.Planes[0].Data.Length > 0 && rejected, "Frame retention or callback reentrancy.");
        Check(session.SetFrameCallbacks(), "Could not unregister frame callbacks.");

        string directory = Directory.CreateTempSubdirectory("mediacap-dotnet-").FullName;
        try
        {
            string path = Path.Combine(directory, "écran-画面.mkv");
            session.AddSource(monitor, config);
            Check(session.StartRecording(new RecordingConfig(path)
            {
                Container = ContainerFormat.Mkv,
                VideoConfig = new VideoEncoderConfig { Width = 128, Height = 96, Framerate = 30, BitrateBps = 500000 },
            }, 500), session.GetError()?.Message ?? "Recording did not start.");
            Check(SpinWait.SpinUntil(() => !session.IsRecording, TimeSpan.FromSeconds(10)), "Recording did not stop.");
            session.Dispose();
            Check(new FileInfo(path).Length > 0, "Unicode output was not finalized.");
        }
        finally
        {
            session.Dispose();
            Directory.Delete(directory, recursive: true);
        }
    }

    private static void Expect<T>(Action action) where T : Exception
    {
        try { action(); }
        catch (T) { return; }
        throw new Exception($"Expected {typeof(T).Name}.");
    }

    private static void Check(bool success, string message)
    {
        if (!success)
            throw new Exception(message);
    }
}
