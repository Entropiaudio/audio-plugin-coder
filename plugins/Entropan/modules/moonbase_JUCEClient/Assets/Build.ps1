[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "Build.ps1 is Windows-only. Use Build.sh on macOS/Linux."
}

$baseDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$binaryBuilder = Join-Path $baseDir 'binaryBuilder.exe'

if (-not (Test-Path -LiteralPath $binaryBuilder -PathType Leaf)) {
    throw "Binary builder not found at $binaryBuilder"
}

Write-Host 'Running asset binary builder...'

$isVerboseAssetBuild = $env:MOONBASE_VERBOSE_ASSET_BUILD -eq '1'
if ($isVerboseAssetBuild) {
    & $binaryBuilder (Join-Path $baseDir 'Source') $baseDir 'MoonbaseBinary'
}
else {
    & $binaryBuilder (Join-Path $baseDir 'Source') $baseDir 'MoonbaseBinary' 2>$null | Out-Null
}

if ($LASTEXITCODE -ne 0) {
    throw "binaryBuilder failed with exit code $LASTEXITCODE"
}

$binaryIncludes = Join-Path $baseDir 'BinaryIncludes.cpp'

$includeLines = New-Object System.Collections.Generic.List[string]
$cppFiles = Get-ChildItem -LiteralPath $baseDir -Filter '*.cpp' -File | Sort-Object -Property Name
foreach ($file in $cppFiles) {
    if (-not [string]::Equals($file.FullName, $binaryIncludes, [System.StringComparison]::OrdinalIgnoreCase)) {
        [void]$includeLines.Add("#include `"$($file.Name)`"")
    }
}

$content = ''
if ($includeLines.Count -gt 0) {
    $content = ($includeLines -join "`n") + "`n"
}

[System.IO.File]::WriteAllText($binaryIncludes, $content, [System.Text.Encoding]::ASCII)
