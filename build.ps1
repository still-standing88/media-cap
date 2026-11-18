$driver = Join-Path $PSScriptRoot 'scripts/build_library.py'
foreach ($name in @('python3', 'python', 'py')) {
    $command = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $command) { continue }
    $launcherArgs = if ($name -eq 'py') { @('-3') } else { @() }
    & $command.Source @launcherArgs -c 'import sys; sys.exit(sys.version_info < (3, 9))' 2>$null
    if ($LASTEXITCODE -eq 0) {
        & $command.Source @launcherArgs $driver @args
        exit $LASTEXITCODE
    }
}
[Console]::Error.WriteLine('Python 3.9 or newer is required.')
exit 1
