[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "GenerateConfigDetails.ps1 is Windows-only. Use GenerateConfigDetails.sh on macOS/Linux."
}

function Show-Usage {
    Write-Host 'Usage: GenerateConfigDetails <path-to-json-file> [path-to-output-header] [--seed <number>] [--out <path>]'
}

if ($Arguments.Count -lt 1) {
    Show-Usage
    throw 'Missing required argument: <path-to-json-file>'
}

$jsonFilePath = $Arguments[0]
$outputHeaderPath = ''
$seedValue = ''

$index = 1
while ($index -lt $Arguments.Count) {
    $token = $Arguments[$index]

    switch ($token) {
        '--help' {
            Show-Usage
            return
        }
        '-h' {
            Show-Usage
            return
        }
        '--seed' {
            if (($index + 1) -ge $Arguments.Count) {
                throw 'Option --seed requires a value.'
            }

            $seedValue = $Arguments[$index + 1]
            $index += 2
            continue
        }
        '--out' {
            if (($index + 1) -ge $Arguments.Count) {
                throw 'Option --out requires a path.'
            }

            $outputHeaderPath = $Arguments[$index + 1]
            $index += 2
            continue
        }
        default {
            if ($token.StartsWith('--')) {
                throw "Unknown option: $token"
            }

            if ([string]::IsNullOrWhiteSpace($outputHeaderPath)) {
                $outputHeaderPath = $token
                $index++
                continue
            }

            throw "Unexpected argument: $token"
        }
    }
}

if (-not (Test-Path -LiteralPath $jsonFilePath -PathType Leaf)) {
    throw "Config JSON not found at $jsonFilePath"
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$moduleDir = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
$binaryPath = Join-Path $scriptDir 'GenerateConfigDetails_windows.exe'

if (-not (Test-Path -LiteralPath $binaryPath -PathType Leaf)) {
    throw "GenerateConfigDetails binary not found at $binaryPath"
}

if ([string]::IsNullOrWhiteSpace($outputHeaderPath)) {
    $outputHeaderPath = Join-Path $moduleDir 'Source\ConfigDetails.generated.h'
}

$resolvedOutputHeaderPath = [System.IO.Path]::GetFullPath($outputHeaderPath)
$outputDirectory = Split-Path -Parent $resolvedOutputHeaderPath
if (-not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$resolvedJsonFilePath = (Resolve-Path -LiteralPath $jsonFilePath).Path

$invokeArgs = @($resolvedJsonFilePath, $resolvedOutputHeaderPath)
if (-not [string]::IsNullOrWhiteSpace($seedValue)) {
    $invokeArgs += @('--seed', $seedValue)
}

& $binaryPath @invokeArgs
if ($LASTEXITCODE -ne 0) {
    throw "GenerateConfigDetails failed with exit code $LASTEXITCODE"
}
