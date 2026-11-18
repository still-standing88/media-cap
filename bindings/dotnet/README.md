# MediaCap for .NET

Reference the MediaCap NuGet package from a .NET 10 desktop or console project. The package selects the native library for your runtime identifier.

```csharp
using MediaCap;

foreach (var monitor in SourceDiscovery.GetMonitors())
    Console.WriteLine($"{monitor.Name} {monitor.Width} × {monitor.Height}");

using var session = new CaptureSession();
Console.WriteLine(session.Id);
```

Use `CaptureSession.Dispose` or a `using` statement to stop capture and release the session. Frame handlers run on native capture threads. Queue work for another thread when it needs to stop or change the session. Screenshots and callback frames own their data after delivery. Video plane strides reflect the packed managed buffers.

## Build a package

Set up the [native dependencies](../../docs/building.rst) and install the .NET 10 SDK. From the repository root, run the common build driver with your selected frontend.

```sh
python scripts/build_library.py --frontend cmake --dotnet --shared --no-examples
```

Use `--frontend meson` or `--frontend scons` to build the native library with another frontend. The build writes the NuGet package under `dotnet/packages` in the selected build directory. The package contains the runtime asset for the build target.

Add `--tests` to compare native and managed layouts and run the managed tests after restoring the package from the local output directory. Set `MEDIACAP_TEST_SCREEN=1` to exercise live screen capture and recording on an accessible display.

To combine assets from several targets, stage their installed native libraries into a common directory, then pack that directory.

```sh
python scripts/build_dotnet.py --prefix stage-windows --rid win-x64 --assets dotnet-assets
python scripts/build_dotnet.py --prefix stage-linux --rid linux-x64 --assets dotnet-assets
python scripts/build_dotnet.py --assets dotnet-assets --output packages
```

The same staging command accepts `win-arm64`, `linux-arm64`, `osx-x64`, and `osx-arm64`. Native backend capabilities and system dependencies follow the MediaCap build for that target.
