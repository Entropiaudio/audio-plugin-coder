[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$InputConfigJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "IntegrityCheck.ps1 is Windows-only. Use IntegrityCheck.sh on macOS/Linux."
}

$currentOs = 'Win'

if (-not (Test-Path -LiteralPath $InputConfigJson -PathType Leaf)) {
    Write-Host "Usage: $($MyInvocation.MyCommand.Name) <path_to_config_json>"
    throw "Config JSON not found at $InputConfigJson"
}

$resolvedInputConfigJson = (Resolve-Path -LiteralPath $InputConfigJson).Path

try {
    Get-Content -LiteralPath $resolvedInputConfigJson -Raw | ConvertFrom-Json | Out-Null
}
catch {
    Write-Host "Invalid JSON format in $resolvedInputConfigJson"
    throw
}

$thisFileDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$codemod = Join-Path $thisFileDir 'integrity-check-codemod-win-amd64.exe'
if (-not (Test-Path -LiteralPath $codemod -PathType Leaf)) {
    throw "Integrity codemod not found at $codemod"
}

Write-Host "Code mod: $codemod"

$checkHeader = Join-Path $thisFileDir '..\Source\Implementations\IntegrityCheck.generated.h'

& $codemod --config $resolvedInputConfigJson --check $checkHeader
if ($LASTEXITCODE -ne 0) {
    throw "Integrity codemod failed with exit code $LASTEXITCODE"
}
