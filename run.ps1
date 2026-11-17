# Prompts for a password and restores the original environment when finished.
param(
    [string]$Database = 'bankcli',
    [string]$User = 'bankcli',
    [string]$Server = '127.0.0.1',
    [ValidateRange(1, 65535)][int]$Port = 3306,
    [switch]$Integration
)
$ErrorActionPreference = 'Stop'
$program = if ($Integration) { 'BankCLITests.exe' } else { 'BankCLI.exe' }
$executable = Join-Path $PSScriptRoot "build\Release\$program"
if (-not (Test-Path -LiteralPath $executable)) { throw 'Build first with .\build.ps1. See README.md.' }
if ($Integration -and -not $Database.EndsWith('_test')) { throw 'Use a fresh database ending in _test.' }

$keys = @('BANKCLI_DB_HOST', 'BANKCLI_DB_NAME', 'BANKCLI_DB_USER', 'BANKCLI_DB_PASSWORD')
$saved = @{}
foreach ($key in $keys) { $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process') }

try {
    $secret = Read-Host "MySQL password for $User" -AsSecureString
    $credential = [pscredential]::new($User, $secret)
    $env:BANKCLI_DB_HOST = "tcp://${Server}:$Port"
    $env:BANKCLI_DB_NAME = $Database
    $env:BANKCLI_DB_USER = $User
    $env:BANKCLI_DB_PASSWORD = $credential.GetNetworkCredential().Password

    $programArguments = if ($Integration) { @('--integration') } else { @() }
    if ($MyInvocation.ExpectingInput) { $input | & $executable @programArguments }
    else { & $executable @programArguments }
    if ($LASTEXITCODE -ne 0) { throw 'Program stopped with an error. Read the message above.' }
} finally {
    foreach ($key in $keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
