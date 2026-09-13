[CmdletBinding()]
param(
    [string]$ConfigJsonPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "prebuild-parity.ps1 is Windows-only. Use bash scripts directly on macOS/Linux."
}

if ([string]::IsNullOrWhiteSpace($ConfigJsonPath)) {
    $ConfigJsonPath = Join-Path $PSScriptRoot 'moonbase_api_config.json'
}

$bashCommand = Get-Command -Name bash -ErrorAction SilentlyContinue
if (-not $bashCommand) {
    throw 'bash is required to run the baseline PreBuild.sh parity test.'
}

$moduleRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedConfigJsonPath = (Resolve-Path -LiteralPath $ConfigJsonPath).Path

function Convert-ToBashPath {
    param([string]$WindowsPath)

    if ($WindowsPath -match '^([A-Za-z]):\\(.*)$') {
        $driveLetter = $matches[1].ToLowerInvariant()
        $pathRemainder = $matches[2] -replace '\\', '/'
        return "/$driveLetter/$pathRemainder"
    }

    return ($WindowsPath -replace '\\', '/')
}

$configJsonPathForBash = Convert-ToBashPath -WindowsPath $resolvedConfigJsonPath

$filesToCompare = @(
    'Source\ConfigDetails.generated.h',
    'Source\Implementations\IntegrityCheck.generated.h',
    'Assets\BinaryIncludes.cpp'
)

function Get-ConfigDetailsAssertions {
    param([string]$HeaderPath)

    $assertions = @()
    foreach ($line in (Get-Content -LiteralPath $HeaderPath)) {
        if ($line -match 'jassert \(result == "(.*)"\);') {
            $assertions += $matches[1]
        }
    }

    return ,$assertions
}

function Get-NormalizedFileText {
    param([string]$FilePath)

    $text = Get-Content -LiteralPath $FilePath -Raw
    $text = $text.Replace("`r`n", "`n")
    $text = $text.Replace("`r", "`n")
    return $text
}

function Get-IntegrityCheckSummary {
    param([string]$HeaderPath)

    $summary = @{
        BeginMarkers = 0
        EndMarkers = 0
        Bi1Checks = 0
        Bi2Checks = 0
    }

    foreach ($line in (Get-Content -LiteralPath $HeaderPath)) {
        if ($line -match 'MOONBASE_KEY_INTEGRITY_CHECK_BEGIN') {
            $summary.BeginMarkers++
        }

        if ($line -match 'MOONBASE_KEY_INTEGRITY_CHECK_END') {
            $summary.EndMarkers++
        }

        if ($line -match 'const juce::BigInteger bi1 \(') {
            $summary.Bi1Checks++
        }

        if ($line -match 'const juce::BigInteger bi2 \(') {
            $summary.Bi2Checks++
        }
    }

    return $summary
}

$baselineDir = Join-Path ([System.IO.Path]::GetTempPath()) ('moonbase-prebuild-baseline-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $baselineDir -Force | Out-Null

$originalDir = Join-Path ([System.IO.Path]::GetTempPath()) ('moonbase-prebuild-original-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $originalDir -Force | Out-Null

$fileOriginallyPresent = @{}
foreach ($relativePath in $filesToCompare) {
    $currentPath = Join-Path $moduleRoot $relativePath
    if (Test-Path -LiteralPath $currentPath -PathType Leaf) {
        $originalPath = Join-Path $originalDir $relativePath
        $originalParent = Split-Path -Parent $originalPath
        if (-not (Test-Path -LiteralPath $originalParent -PathType Container)) {
            New-Item -ItemType Directory -Path $originalParent -Force | Out-Null
        }

        Copy-Item -LiteralPath $currentPath -Destination $originalPath -Force
        $fileOriginallyPresent[$relativePath] = $true
    }
    else {
        $fileOriginallyPresent[$relativePath] = $false
    }
}

$originalForceBash = $env:MOONBASE_FORCE_BASH
$originalSeed = $env:SEED

try {
    Set-Location $moduleRoot

    $env:MOONBASE_FORCE_BASH = '1'
    $env:SEED = '1337'
    & bash './PreBuild.sh' $configJsonPathForBash
    if ($LASTEXITCODE -ne 0) {
        throw "Baseline bash PreBuild.sh failed with exit code $LASTEXITCODE"
    }

    foreach ($relativePath in $filesToCompare) {
        $sourcePath = Join-Path $moduleRoot $relativePath
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Baseline output missing: $sourcePath"
        }

        $baselinePath = Join-Path $baselineDir $relativePath
        $baselineParent = Split-Path -Parent $baselinePath
        if (-not (Test-Path -LiteralPath $baselineParent -PathType Container)) {
            New-Item -ItemType Directory -Path $baselineParent -Force | Out-Null
        }

        Copy-Item -LiteralPath $sourcePath -Destination $baselinePath -Force
    }

    foreach ($relativePath in $filesToCompare) {
        $currentPath = Join-Path $moduleRoot $relativePath
        if ($fileOriginallyPresent[$relativePath]) {
            $originalPath = Join-Path $originalDir $relativePath
            Copy-Item -LiteralPath $originalPath -Destination $currentPath -Force
        }
        elseif (Test-Path -LiteralPath $currentPath -PathType Leaf) {
            Remove-Item -LiteralPath $currentPath -Force
        }
    }

    Remove-Item Env:MOONBASE_FORCE_BASH -ErrorAction SilentlyContinue
    $env:SEED = '1337'
    & (Join-Path $moduleRoot 'PreBuild.ps1') $resolvedConfigJsonPath

    foreach ($relativePath in $filesToCompare) {
        $baselinePath = Join-Path $baselineDir $relativePath
        $currentPath = Join-Path $moduleRoot $relativePath

        if (-not (Test-Path -LiteralPath $currentPath -PathType Leaf)) {
            throw "PowerShell output missing: $currentPath"
        }

        if ($relativePath -eq 'Source\ConfigDetails.generated.h') {
            $baselineAssertions = Get-ConfigDetailsAssertions -HeaderPath $baselinePath
            $currentAssertions = Get-ConfigDetailsAssertions -HeaderPath $currentPath

            if ($baselineAssertions.Count -ne $currentAssertions.Count) {
                throw "Mismatch detected for ${relativePath}: assertion count differs. Bash output is the source-of-truth baseline."
            }

            for ($i = 0; $i -lt $baselineAssertions.Count; $i++) {
                if ($baselineAssertions[$i] -ne $currentAssertions[$i]) {
                    throw "Mismatch detected for ${relativePath} at assertion index $i. Bash output is the source-of-truth baseline."
                }
            }
        }
        elseif ($relativePath -eq 'Source\Implementations\IntegrityCheck.generated.h') {
            $baselineSummary = Get-IntegrityCheckSummary -HeaderPath $baselinePath
            $currentSummary = Get-IntegrityCheckSummary -HeaderPath $currentPath

            if ($baselineSummary.BeginMarkers -ne $currentSummary.BeginMarkers -or
                $baselineSummary.EndMarkers -ne $currentSummary.EndMarkers -or
                $baselineSummary.Bi1Checks -ne $currentSummary.Bi1Checks -or
                $baselineSummary.Bi2Checks -ne $currentSummary.Bi2Checks) {
                throw "Mismatch detected for ${relativePath}: integrity check structure differs. Bash output is the source-of-truth baseline."
            }
        }
        else {
            $baselineText = Get-NormalizedFileText -FilePath $baselinePath
            $currentText = Get-NormalizedFileText -FilePath $currentPath

            if ($baselineText -ne $currentText) {
                throw "Mismatch detected for $relativePath. Bash output is the source-of-truth baseline."
            }
        }
    }

    Write-Host 'PreBuild parity test passed: PowerShell output matches bash baseline.'
}
finally {
    if ($null -eq $originalForceBash) {
        Remove-Item Env:MOONBASE_FORCE_BASH -ErrorAction SilentlyContinue
    }
    else {
        $env:MOONBASE_FORCE_BASH = $originalForceBash
    }

    if ($null -eq $originalSeed) {
        Remove-Item Env:SEED -ErrorAction SilentlyContinue
    }
    else {
        $env:SEED = $originalSeed
    }

    foreach ($relativePath in $filesToCompare) {
        $currentPath = Join-Path $moduleRoot $relativePath
        if ($fileOriginallyPresent[$relativePath]) {
            $originalPath = Join-Path $originalDir $relativePath
            if (Test-Path -LiteralPath $originalPath -PathType Leaf) {
                Copy-Item -LiteralPath $originalPath -Destination $currentPath -Force
            }
        }
        elseif (Test-Path -LiteralPath $currentPath -PathType Leaf) {
            Remove-Item -LiteralPath $currentPath -Force
        }
    }

    if (Test-Path -LiteralPath $baselineDir -PathType Container) {
        Remove-Item -LiteralPath $baselineDir -Recurse -Force
    }

    if (Test-Path -LiteralPath $originalDir -PathType Container) {
        Remove-Item -LiteralPath $originalDir -Recurse -Force
    }
}
