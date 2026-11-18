param(
    [ValidateSet('meson','cmake','scons')][string]$Frontend = 'meson',
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [ValidateSet('shared','static')][string]$Library = 'shared',
    [Parameter(Mandatory=$true)][string]$VcpkgRoot,
    [ValidateSet('x64-windows-static','arm64-windows-static')][string]$Triplet = 'x64-windows-static',
    [string]$BuildRoot = 'build-ci',
    [int]$Jobs = 2,
    [switch]$LibraryOnly,
    [switch]$PythonBindings,
    [switch]$DotNetBindings,
    [switch]$Offline
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Run in a Visual Studio Developer PowerShell for the selected triplet.' }
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    $VcpkgRoot = (Resolve-Path $VcpkgRoot).Path
    $build = [IO.Path]::GetFullPath($BuildRoot)
    $stage = Join-Path $build 'package'
    $env:VCPKG_ROOT = $VcpkgRoot
    $env:CARGO_BUILD_JOBS = "$Jobs"
    $debug = $Configuration -eq 'Debug'
    $shared = $Library -eq 'shared'
    $objectRoot = Join-Path $build 'objects'
    $options = @('--frontend',$Frontend,"--$($Configuration.ToLower())","--$Library",
        '--vcpkg-root',$VcpkgRoot,'--triplet',$Triplet,'--build-dir',$objectRoot,'--install-prefix',$stage,
        '--tests','--jobs',"$Jobs")
    if (-not $LibraryOnly) { $options += '--cli' }
    if ($Offline) { $options += '--offline' }
    if ($PythonBindings) { $options += '--python' }
    if ($DotNetBindings) { $options += '--dotnet' }
    Invoke-Checked python (@('scripts/build_library.py') + $options)
    if ($PythonBindings) {
        $platlib = & python bindings/python/python_config.py platlib
        if ($LASTEXITCODE -ne 0) { throw 'Could not locate the staged Python package' }
        $previousPythonPath = $env:PYTHONPATH
        try {
            $env:PYTHONPATH = Join-Path $stage $platlib
            Invoke-Checked python @('tests/integration/python_bindings.py')
        } finally { $env:PYTHONPATH = $previousPythonPath }
    }
    # Inspect the compiled object to confirm the requested CRT.
    $object = Get-ChildItem -LiteralPath $objectRoot -Recurse -Filter '*Session.cpp.obj' | Select-Object -First 1
    if (-not $object) { throw 'No MediaCap implementation object found for CRT validation.' }
    $directives = (& dumpbin /nologo /directives $object.FullName) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'dumpbin object inspection failed' }
    $crt = if ($debug) { 'LIBCMTD' } else { 'LIBCMT' }
    if ($directives -notmatch "(?i)DEFAULTLIB:[`" ]*$crt(?:[`"\s]|$)" -or
        $directives -match '(?i)DEFAULTLIB:[" ]*(MSVCRT|MSVCRTD)(?:["\s]|$)') { throw "Wrong CRT in $($object.FullName)" }
    $directives | Set-Content (Join-Path $build 'crt-directives.txt')
    $runtimeBinary = if ($shared) { 'bin/MediaCap.dll' } else { 'bin/MediaCap.exe' }
    Invoke-Checked python @('scripts/win/check_runtime.py',(Join-Path $stage $runtimeBinary),
        '--output',(Join-Path $build 'runtime-imports.json'))
    # Consume only installed headers/metadata/libraries from a separate project.
    $consumer = Join-Path $build 'installed-consumer'
    Invoke-Checked cmake @('-S','tests/integration/installed','-B',$consumer,'-G','Ninja',
        "-DCMAKE_BUILD_TYPE=$Configuration","-DMEDIACAP_PACKAGE=$stage")
    Invoke-Checked cmake @('--build',$consumer,'--parallel',"$Jobs")
    Invoke-Checked ctest @('--test-dir',$consumer,'--output-on-failure')
    if (-not $LibraryOnly) {
        Invoke-Checked (Join-Path $stage 'bin/MediaCap.exe') @('--version')
        Invoke-Checked (Join-Path $stage 'bin/MediaCap.exe') @('capabilities','formats','--json')
    }
} finally { Pop-Location }
