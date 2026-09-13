[CmdletBinding()]
param(
    [switch]$Apply,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$VcVarsAllArgs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw 'vcvarsall.ps1 is Windows-only.'
}

if ($VcVarsAllArgs.Count -eq 0) {
    Write-Host 'Usage: vcvarsall.ps1 [vcvarsall.bat arguments]'
    Write-Host 'Usage: vcvarsall.ps1 -Apply [vcvarsall.bat arguments]'
    throw 'Missing vcvarsall.bat arguments.'
}

function Get-VsWherePath {
    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:VSWHEREPATH)) {
        $candidates += $env:VSWHEREPATH
    }
    $candidates += 'vswhere.exe'
    $candidates += 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'

    foreach ($candidate in $candidates) {
        $command = Get-Command -Name $candidate -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }

        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw 'Unable to locate vswhere.exe. Install Visual Studio 2022 with C++ tools.'
}

function Get-VcVarsAllBatchPath {
    $vswherePath = Get-VsWherePath
    $installationPath = (& $vswherePath -latest -products '*' -requires 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64' -property installationPath | Select-Object -First 1).Trim()

    if ([string]::IsNullOrWhiteSpace($installationPath)) {
        $installationPath = (& $vswherePath -latest -products '*' -property installationPath | Select-Object -First 1).Trim()
    }

    if (-not [string]::IsNullOrWhiteSpace($installationPath)) {
        $vcvarsAllPath = Join-Path $installationPath 'VC\Auxiliary\Build\vcvarsall.bat'
        if (Test-Path -LiteralPath $vcvarsAllPath -PathType Leaf) {
            return $vcvarsAllPath
        }
    }

    $vsRoot = 'C:\Program Files\Microsoft Visual Studio\2022'
    foreach ($edition in @('Community', 'Professional', 'Enterprise', 'BuildTools')) {
        $candidate = Join-Path $vsRoot "$edition\VC\Auxiliary\Build\vcvarsall.bat"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw 'vcvarsall.bat not found. Install Visual Studio 2022 with C++ tools.'
}

function Quote-CmdArgument {
    param([string]$Argument)

    if ($Argument -match '[\s"]') {
        return '"' + ($Argument -replace '"', '""') + '"'
    }

    return $Argument
}

function Convert-ToBashExportLine {
    param(
        [string]$Name,
        [string]$Value
    )

    $escapedValue = $Value.Replace("'", "'\\''")
    return "export $Name='$escapedValue'"
}

function Convert-WindowsPathToUnix {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return $null
    }

    if ($PathValue -match '^([A-Za-z]):\\(.*)$') {
        $drive = $matches[1].ToLowerInvariant()
        $rest = $matches[2] -replace '\\', '/'
        return "/$drive/$rest"
    }

    if ($PathValue -match '^\\\\(.*)$') {
        return '//' + ($matches[1] -replace '\\', '/')
    }

    return ($PathValue -replace '\\', '/')
}

function Convert-WindowsPathListToUnix {
    param([string]$PathList)

    $parts = $PathList -split ';'
    $converted = New-Object System.Collections.Generic.List[string]
    foreach ($part in $parts) {
        $convertedPart = Convert-WindowsPathToUnix -PathValue $part
        if (-not [string]::IsNullOrWhiteSpace($convertedPart)) {
            [void]$converted.Add($convertedPart)
        }
    }

    return ($converted -join ':')
}

function Test-IsRelevantVcVar {
    param([string]$Name)

    if ($Name -eq 'PATH') {
        return $true
    }

    switch -Regex ($Name) {
        '^(LIB|LIBPATH|INCLUDE|EXTERNAL_INCLUDE|COMMANDPROMPTTYPE|DEVENVDIR|EXTENSIONSDKDIR|PLATFORM|PREFERREDTOOLARCHITECTURE)$' { return $true }
        '^FRAMEWORK' { return $true }
        '^UCRT' { return $true }
        '^UNIVERSALCRTSDK' { return $true }
        '^VCIDE' { return $true }
        '^VCINSTALL' { return $true }
        '^VCPKG' { return $true }
        '^VCTOOLS' { return $true }
        '^VSCMD' { return $true }
        '^VSINSTALL' { return $true }
        '^VS[0-9]' { return $true }
        '^VISUALSTUDIO' { return $true }
        '^WINDOWSLIB' { return $true }
        '^WINDOWSSDK' { return $true }
        default { return $false }
    }
}

$vcvarsAllPath = Get-VcVarsAllBatchPath
$quotedArgs = @()
foreach ($arg in $VcVarsAllArgs) {
    $quotedArgs += Quote-CmdArgument -Argument $arg
}

$argsText = $quotedArgs -join ' '
$cmdStatement = "call `"$vcvarsAllPath`" $argsText && set"
$envLines = & cmd.exe /d /c $cmdStatement
if ($LASTEXITCODE -ne 0) {
    throw "vcvarsall.bat failed with exit code $LASTEXITCODE"
}

$envMap = @{}
foreach ($line in $envLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        $name = $matches[1].ToUpperInvariant()
        $value = $matches[2]
        if ($name -match '^[A-Z0-9_]+$' -and -not $envMap.ContainsKey($name)) {
            $envMap[$name] = $value
        }
    }
}

if ($envMap.Count -eq 0) {
    throw 'No environment variables were captured from vcvarsall.bat.'
}

if ($Apply) {
    foreach ($name in ($envMap.Keys | Sort-Object)) {
        if (Test-IsRelevantVcVar -Name $name) {
            Set-Item -Path ("Env:$name") -Value $envMap[$name]
        }
    }
    return
}

foreach ($name in ($envMap.Keys | Sort-Object)) {
    if (-not (Test-IsRelevantVcVar -Name $name)) {
        continue
    }

    if ($name -eq 'PATH') {
        Write-Output (Convert-ToBashExportLine -Name 'WINDOWS_PATH' -Value $envMap[$name])
        Write-Output (Convert-ToBashExportLine -Name 'PATH' -Value (Convert-WindowsPathListToUnix -PathList $envMap[$name]))
        continue
    }

    Write-Output (Convert-ToBashExportLine -Name $name -Value $envMap[$name])
}
