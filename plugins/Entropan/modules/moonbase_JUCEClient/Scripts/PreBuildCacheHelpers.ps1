Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "PreBuildCacheHelpers.ps1 is Windows-only. Use PreBuildCacheHelpers.sh on macOS/Linux."
}

$script:PREBUILD_CACHE_RESULT_CONTINUE = 0
$script:PREBUILD_CACHE_RESULT_SKIP = 10

$script:PrebuildCacheEnabled = $false
$script:PrebuildCacheDir = ''
$script:PrebuildCacheKey = ''
$script:PrebuildFingerprintFile = ''
$script:PrebuildFingerprintCurrent = ''

function Normalize-BuildConfigName {
    param([string]$ConfigName)

    if ($null -eq $ConfigName) {
        return ''
    }

    return (($ConfigName.ToLowerInvariant()) -replace '\s+', '')
}

function Get-FirstNonEmptyEnvValue {
    param([string[]]$VariableNames)

    foreach ($name in $VariableNames) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value
        }
    }

    return $null
}

function Test-ShouldEnablePreBuildCache {
    $detectedConfig = Get-FirstNonEmptyEnvValue -VariableNames @(
        'BUILD_CONFIG',
        'CMAKE_BUILD_TYPE',
        'CONFIGURATION',
        'Configuration',
        'CONFIG',
        'Config',
        'BUILD_TYPE',
        'CMAKE_CONFIG_TYPE',
        'CMAKE_CONFIGURATION'
    )

    if ([string]::IsNullOrWhiteSpace($detectedConfig)) {
        return $false
    }

    return (Normalize-BuildConfigName -ConfigName $detectedConfig) -eq 'debug'
}

function Get-HashFileSha1 {
    param([string]$FilePath)

    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "File not found for hashing: $FilePath"
    }

    return (Get-FileHash -LiteralPath $FilePath -Algorithm SHA1).Hash.ToLowerInvariant()
}

function Get-HashTextSha1 {
    param([string]$Text)

    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $hashBytes = $sha1.ComputeHash($bytes)
    }
    finally {
        $sha1.Dispose()
    }

    $builder = New-Object System.Text.StringBuilder
    foreach ($b in $hashBytes) {
        [void]$builder.AppendFormat('{0:x2}', $b)
    }

    return $builder.ToString()
}

function Get-CanonicalPath {
    param([string]$PathValue)

    return (Resolve-Path -LiteralPath $PathValue).Path
}

function Get-PreBuildFingerprint {
    param(
        [string]$ModuleDir,
        [string]$ConfigFile
    )

    $lines = New-Object System.Collections.Generic.List[string]

    $trackedFiles = @(
        $ConfigFile,
        (Join-Path $ModuleDir 'PreBuild.sh'),
        (Join-Path $ModuleDir 'PreBuild.ps1'),
        (Join-Path $ModuleDir 'Scripts\PreBuildCacheHelpers.sh'),
        (Join-Path $ModuleDir 'Scripts\PreBuildCacheHelpers.ps1'),
        (Join-Path $ModuleDir 'Assets\Build.sh'),
        (Join-Path $ModuleDir 'Assets\Build.ps1'),
        (Join-Path $ModuleDir 'KeyIntegrity\IntegrityCheck.sh'),
        (Join-Path $ModuleDir 'KeyIntegrity\IntegrityCheck.ps1'),
        (Join-Path $ModuleDir 'GenerateConfigDetails\GenerateConfigDetails.sh'),
        (Join-Path $ModuleDir 'GenerateConfigDetails\GenerateConfigDetails.ps1'),
        (Join-Path $ModuleDir 'GenerateConfigDetails\GenerateConfigDetails_macOS'),
        (Join-Path $ModuleDir 'GenerateConfigDetails\GenerateConfigDetails_linux'),
        (Join-Path $ModuleDir 'GenerateConfigDetails\GenerateConfigDetails_windows.exe')
    )

    foreach ($tracked in $trackedFiles) {
        if (-not (Test-Path -LiteralPath $tracked -PathType Leaf)) {
            continue
        }

        $canonicalTracked = Get-CanonicalPath -PathValue $tracked
        [void]$lines.Add("$(Get-HashFileSha1 -FilePath $canonicalTracked) $canonicalTracked")
    }

    $extraFiles = New-Object System.Collections.Generic.List[string]

    $assetsSourceDir = Join-Path $ModuleDir 'Assets\Source'
    if (Test-Path -LiteralPath $assetsSourceDir -PathType Container) {
        foreach ($file in (Get-ChildItem -LiteralPath $assetsSourceDir -File -Recurse)) {
            [void]$extraFiles.Add($file.FullName)
        }
    }

    $keyIntegrityDir = Join-Path $ModuleDir 'KeyIntegrity'
    if (Test-Path -LiteralPath $keyIntegrityDir -PathType Container) {
        foreach ($file in (Get-ChildItem -LiteralPath $keyIntegrityDir -File -Recurse)) {
            [void]$extraFiles.Add($file.FullName)
        }
    }

    $generateConfigDetailsDir = Join-Path $ModuleDir 'GenerateConfigDetails'
    if (Test-Path -LiteralPath $generateConfigDetailsDir -PathType Container) {
        foreach ($file in (Get-ChildItem -LiteralPath $generateConfigDetailsDir -File -Recurse)) {
            [void]$extraFiles.Add($file.FullName)
        }
    }

    $assetsDir = Join-Path $ModuleDir 'Assets'
    if (Test-Path -LiteralPath $assetsDir -PathType Container) {
        foreach ($file in (Get-ChildItem -LiteralPath $assetsDir -File -Filter 'binaryBuilder*')) {
            [void]$extraFiles.Add($file.FullName)
        }
    }

    $sortedExtraFiles = $extraFiles | Sort-Object
    foreach ($filePath in $sortedExtraFiles) {
        $canonicalFilePath = Get-CanonicalPath -PathValue $filePath
        [void]$lines.Add("$(Get-HashFileSha1 -FilePath $canonicalFilePath) $canonicalFilePath")
    }

    $fingerprintText = ($lines -join "`n") + "`n"
    return Get-HashTextSha1 -Text $fingerprintText
}

function Test-PreBuildOutputsExist {
    param([string]$ModuleDir)

    $requiredOutputs = @(
        (Join-Path $ModuleDir 'Assets\MoonbaseBinary.h'),
        (Join-Path $ModuleDir 'Assets\BinaryIncludes.cpp'),
        (Join-Path $ModuleDir 'Source\Implementations\IntegrityCheck.generated.h'),
        (Join-Path $ModuleDir 'Source\ConfigDetails.generated.h')
    )

    foreach ($outputPath in $requiredOutputs) {
        if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
            return $false
        }
    }

    $moonbaseBinaryCpp = Get-ChildItem -LiteralPath (Join-Path $ModuleDir 'Assets') -File -Filter 'MoonbaseBinary*.cpp' -ErrorAction SilentlyContinue
    if (-not $moonbaseBinaryCpp) {
        return $false
    }

    return $true
}

function Prepare-PreBuildCache {
    param(
        [string]$ModuleDir,
        [string]$ConfigFile
    )

    $canonicalModuleDir = Get-CanonicalPath -PathValue $ModuleDir
    $canonicalConfigFile = Get-CanonicalPath -PathValue $ConfigFile

    $script:PrebuildCacheEnabled = $false
    $script:PrebuildCacheDir = ''
    $script:PrebuildCacheKey = ''
    $script:PrebuildFingerprintFile = ''
    $script:PrebuildFingerprintCurrent = ''

    if (-not (Test-ShouldEnablePreBuildCache)) {
        return $script:PREBUILD_CACHE_RESULT_CONTINUE
    }

    $script:PrebuildCacheEnabled = $true
    $script:PrebuildCacheDir = Join-Path $canonicalModuleDir '.moonbase-prebuild-cache'
    $script:PrebuildCacheKey = Get-HashTextSha1 -Text $canonicalModuleDir
    $script:PrebuildFingerprintFile = Join-Path $script:PrebuildCacheDir ("moonbase_prebuild_{0}.fingerprint" -f $script:PrebuildCacheKey)

    if (-not (Test-Path -LiteralPath $script:PrebuildCacheDir -PathType Container)) {
        New-Item -ItemType Directory -Path $script:PrebuildCacheDir -Force | Out-Null
    }

    $script:PrebuildFingerprintCurrent = Get-PreBuildFingerprint -ModuleDir $canonicalModuleDir -ConfigFile $canonicalConfigFile

    $previousFingerprint = ''
    if (Test-Path -LiteralPath $script:PrebuildFingerprintFile -PathType Leaf) {
        $previousFingerprint = (Get-Content -LiteralPath $script:PrebuildFingerprintFile -Raw).Trim()
    }

    if (($script:PrebuildFingerprintCurrent -eq $previousFingerprint) -and (Test-PreBuildOutputsExist -ModuleDir $canonicalModuleDir)) {
        return $script:PREBUILD_CACHE_RESULT_SKIP
    }

    return $script:PREBUILD_CACHE_RESULT_CONTINUE
}

function Finalize-PreBuildCache {
    if (-not $script:PrebuildCacheEnabled) {
        return
    }

    if ([string]::IsNullOrWhiteSpace($script:PrebuildFingerprintFile) -or [string]::IsNullOrWhiteSpace($script:PrebuildFingerprintCurrent)) {
        throw 'Prebuild cache fingerprint state is incomplete.'
    }

    [System.IO.File]::WriteAllText(
        $script:PrebuildFingerprintFile,
        $script:PrebuildFingerprintCurrent + [Environment]::NewLine,
        [System.Text.Encoding]::ASCII
    )
}

function Get-PreBuildStepSummary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StepKey,
        [Parameter(Mandatory = $true)]
        [string]$ModuleDir,
        [Parameter(Mandatory = $true)]
        [object[]]$StepOutput
    )

    switch ($StepKey) {
        'assets' {
            $assetCppFiles = @(Get-ChildItem -LiteralPath (Join-Path $ModuleDir 'Assets') -File -Filter 'MoonbaseBinary*.cpp' -ErrorAction SilentlyContinue)
            return "generated Assets/BinaryIncludes.cpp and $($assetCppFiles.Count) MoonbaseBinary*.cpp file(s)"
        }
        'integrity' {
            $checksAdded = ''
            foreach ($entry in $StepOutput) {
                $line = [string]$entry
                if ($line -match 'added\s+([0-9]+)\s+checks') {
                    $checksAdded = $matches[1]
                }
            }

            if (-not [string]::IsNullOrWhiteSpace($checksAdded)) {
                return "added $checksAdded checks"
            }

            return 'updated Source/Implementations/IntegrityCheck.generated.h'
        }
        'config' {
            $wrotePath = ''
            foreach ($entry in $StepOutput) {
                $line = [string]$entry
                if ($line -match '^Wrote:\s*(.+)$') {
                    $wrotePath = $matches[1].Trim()
                }
            }

            if (-not [string]::IsNullOrWhiteSpace($wrotePath)) {
                $modulePrefix = $ModuleDir.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
                if ($wrotePath.StartsWith($modulePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                    $wrotePath = $wrotePath.Substring($modulePrefix.Length)
                }

                return "wrote $wrotePath"
            }

            return 'wrote Source/ConfigDetails.generated.h'
        }
        default {
            return 'completed'
        }
    }
}

function Invoke-PreBuildStep {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$StepKey,
        [Parameter(Mandatory = $true)]
        [string]$ModuleDir,
        [Parameter(Mandatory = $true)]
        [scriptblock]$StepCommand
    )

    $stepOutput = @()
    try {
        $stepOutput = & $StepCommand *>&1
    }
    catch {
        Write-Host "Moonbase PreBuild [$StepKey]: failed."
        foreach ($entry in $stepOutput) {
            Write-Host ([string]$entry)
        }
        throw
    }

    $stepSummary = Get-PreBuildStepSummary -StepKey $StepKey -ModuleDir $ModuleDir -StepOutput $stepOutput
    Write-Host "Moonbase PreBuild [$StepKey]: ran ($stepSummary)."
}

function Invoke-MoonbasePreBuildCached {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModuleDir,
        [Parameter(Mandatory = $true)]
        [string]$ConfigFile
    )

    $cachePrepareResult = Prepare-PreBuildCache -ModuleDir $ModuleDir -ConfigFile $ConfigFile

    if ($cachePrepareResult -eq $script:PREBUILD_CACHE_RESULT_SKIP) {
        Write-Host 'Moonbase PreBuild [assets]: skipped (cache hit).'
        Write-Host 'Moonbase PreBuild [integrity]: skipped (cache hit).'
        Write-Host 'Moonbase PreBuild [config]: skipped (cache hit).'
        return
    }

    if ($cachePrepareResult -ne $script:PREBUILD_CACHE_RESULT_CONTINUE) {
        throw "Unexpected Moonbase prebuild cache state: $cachePrepareResult"
    }

    $resolvedModuleDir = Get-CanonicalPath -PathValue $ModuleDir
    $resolvedConfigFile = Get-CanonicalPath -PathValue $ConfigFile
    $configHeaderPath = Join-Path $resolvedModuleDir 'Source\ConfigDetails.generated.h'

    Invoke-PreBuildStep -StepKey 'assets' -ModuleDir $resolvedModuleDir -StepCommand {
        & (Join-Path $resolvedModuleDir 'Assets\Build.ps1')
    }

    Invoke-PreBuildStep -StepKey 'integrity' -ModuleDir $resolvedModuleDir -StepCommand {
        & (Join-Path $resolvedModuleDir 'KeyIntegrity\IntegrityCheck.ps1') $resolvedConfigFile
    }

    Invoke-PreBuildStep -StepKey 'config' -ModuleDir $resolvedModuleDir -StepCommand {
        & (Join-Path $resolvedModuleDir 'GenerateConfigDetails\GenerateConfigDetails.ps1') $resolvedConfigFile $configHeaderPath
    }

    Finalize-PreBuildCache
}
