# Convenience commands for the portable installation prepared in this workspace.
# Credentials are Windows-encrypted XML files outside the repository.
param(
    [ValidateSet('Start','Run','Stop','Test','Build')]
    [string]$Action = 'Run',
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '..\..\work')
)
$ErrorActionPreference = 'Stop'
$workPath = (Resolve-Path -LiteralPath $WorkDirectory).Path
$serverRoot = Join-Path $workPath 'dependencies\mysql-8.4.11-winx64'
$connector = Join-Path $workPath 'dependencies\mysql-connector-c++-26.7.0-winx64'
$mysql = Join-Path $serverRoot 'bin\mysql.exe'
$mysqladmin = Join-Path $serverRoot 'bin\mysqladmin.exe'
$common = @('--no-defaults','--host=127.0.0.1','--port=33307','--user=root')

if ($Action -eq 'Start') {
    # Foreground server: leave this terminal open; use a second terminal for Run.
    & (Join-Path $serverRoot 'bin\mysqld.exe') --no-defaults "--datadir=$workPath\mysql-data" --port=33307 --bind-address=127.0.0.1 --mysqlx=OFF --skip-log-bin --console
    exit $LASTEXITCODE
}
if ($Action -eq 'Build') {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Install Microsoft C++ Build Tools first.' }
    $cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    & $cmake -S $PSScriptRoot -B "$PSScriptRoot\build" "-DMYSQL_CONNECTOR_ROOT=$connector" -DBANKCLI_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build "$PSScriptRoot\build" --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    exit 0
}
$keys = @('BANKCLI_DB_HOST','BANKCLI_DB_NAME','BANKCLI_DB_USER','BANKCLI_DB_PASSWORD','MYSQL_PWD')
$saved = @{}
foreach ($key in $keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }
try {
    $env:BANKCLI_DB_HOST = 'tcp://127.0.0.1:33307'
    if ($Action -eq 'Run') {
        $credential = Import-Clixml (Join-Path $workPath 'app-credential.xml')
        $env:BANKCLI_DB_USER = $credential.UserName
        $env:BANKCLI_DB_PASSWORD = $credential.GetNetworkCredential().Password
        $env:BANKCLI_DB_NAME = 'bankcli'
        & "$PSScriptRoot\build\Release\BankCLI.exe"
        if ($LASTEXITCODE -ne 0) { throw 'BankCLI stopped. Read the preceding message.' }
    } else {
        $credential = Import-Clixml (Join-Path $workPath 'root-credential.xml')
        $env:MYSQL_PWD = $credential.GetNetworkCredential().Password
        if ($Action -eq 'Stop') {
            & $mysqladmin @common shutdown
            if ($LASTEXITCODE -ne 0) { throw 'Could not stop the workspace MySQL server.' }
        } else {
            # Every run gets a NEW database; existing data is never dropped.
            $testName = 'bankcli_' + [guid]::NewGuid().ToString('N') + '_test'
            "CREATE DATABASE $testName CHARACTER SET utf8mb4;" | & $mysql @common
            if ($LASTEXITCODE -ne 0) { throw 'Could not create test database.' }
            Get-Content (Join-Path $PSScriptRoot 'schema.sql') -Raw | & $mysql @common $testName
            if ($LASTEXITCODE -ne 0) { throw 'Could not create test schema.' }
            $env:BANKCLI_DB_NAME = $testName
            $env:BANKCLI_DB_USER = 'root'
            $env:BANKCLI_DB_PASSWORD = $env:MYSQL_PWD
            & "$PSScriptRoot\build\Release\BankCLITests.exe" --integration
            if ($LASTEXITCODE -ne 0) { throw 'Integration tests failed.' }
            Write-Host "Test data retained in $testName for inspection."
        }
    }
} finally {
    foreach ($key in $keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
