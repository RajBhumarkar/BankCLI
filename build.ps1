# Run from PowerShell: .\build.ps1 -ConnectorRoot 'C:\dev\your-connector-folder'
param([Parameter(Mandatory = $true)][string]$ConnectorRoot)
$ErrorActionPreference = 'Stop'

$connector = (Resolve-Path -LiteralPath $ConnectorRoot).Path
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    # CMake installed by Visual Studio Build Tools is not always on PATH.
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'Install CMake and Microsoft C++ Build Tools. See README.md.'
    }
    $visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $visualStudio) { throw 'Install the Desktop development with C++ workload.' }
    $cmake = Join-Path $visualStudio 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path -LiteralPath $cmake)) { throw 'Install the C++ CMake tools component.' }
}

& $cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -A x64 "-DMYSQL_CONNECTOR_ROOT=$connector" -DBANKCLI_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'Configuration failed. Read the error above.' }
& $cmake --build "$PSScriptRoot\build" --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed. Read the error above.' }
& (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir "$PSScriptRoot\build" -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Account tests failed.' }
Write-Host 'Built successfully. Next: .\run.ps1'
