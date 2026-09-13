[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$ConfigJsonPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "PreBuild.ps1 is Windows-only. Use PreBuild.sh on macOS/Linux."
}

if (-not (Test-Path -LiteralPath $ConfigJsonPath -PathType Leaf)) {
    Write-Host "Error: Config file not found at $ConfigJsonPath"
    Write-Host "Usage: $($MyInvocation.MyCommand.Name) <path_to_config.json>"
    throw "Missing config JSON file."
}

$moduleDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$helpersScript = Join-Path $moduleDir 'Scripts\PreBuildCacheHelpers.ps1'
if (-not (Test-Path -LiteralPath $helpersScript -PathType Leaf)) {
    throw "Prebuild cache helper script not found at $helpersScript"
}

. $helpersScript

$resolvedConfigJsonPath = (Resolve-Path -LiteralPath $ConfigJsonPath).Path
Invoke-MoonbasePreBuildCached -ModuleDir $moduleDir -ConfigFile $resolvedConfigJsonPath
