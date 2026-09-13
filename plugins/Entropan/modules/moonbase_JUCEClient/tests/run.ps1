[CmdletBinding()]
param(
    [string]$MatrixJUCE,
    [string]$MatrixCpp,
    [string]$RunsOnGithub,
    [string]$DiagnosticMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "run.ps1 is Windows-only. Use run.sh on macOS/Linux."
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [string[]]$Args,
        [string]$FailureLabel
    )

    if ($Args) {
        & $Command @Args
    }
    else {
        & $Command
    }

    if ($LASTEXITCODE -ne 0) {
        if ([string]::IsNullOrWhiteSpace($FailureLabel)) {
            $FailureLabel = $Command
        }

        throw "$FailureLabel failed with exit code $LASTEXITCODE"
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $repoRoot

$runValidationBenchmark = $false

if (-not [string]::IsNullOrEmpty($RunsOnGithub)) {
    if ([string]::IsNullOrWhiteSpace($env:SEED)) {
        $env:SEED = '1337'
    }
}

$vcvarsAllScript = Join-Path $PSScriptRoot 'msvc-init\vcvarsall.ps1'
& $vcvarsAllScript -Apply x64

$testConfigPath = Join-Path $repoRoot 'tests\moonbase_api_config.json'
if (-not (Test-Path -LiteralPath $testConfigPath -PathType Leaf)) {
    throw "Error: test config json not found at $testConfigPath"
}

$resolvedTestConfigPath = (Resolve-Path -LiteralPath $testConfigPath).Path

if (-not [string]::IsNullOrEmpty($RunsOnGithub)) {
    $parityScript = Join-Path $PSScriptRoot 'prebuild-parity.ps1'
    if (-not (Test-Path -LiteralPath $parityScript -PathType Leaf)) {
        throw "Prebuild parity script not found at $parityScript"
    }

    & $parityScript $resolvedTestConfigPath
}

& (Join-Path $repoRoot 'PreBuild.ps1') $resolvedTestConfigPath

$cxxDefines = '-DMB_CATCH2_TESTING -DJUCE_MODAL_LOOPS_PERMITTED=1'

$sanitizerFlags = ''
if ($DiagnosticMode -eq 'sanitizers') {
    $sanitizerFlags = '-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all'

    if ([string]::IsNullOrWhiteSpace($env:ASAN_OPTIONS)) {
        $env:ASAN_OPTIONS = 'abort_on_error=1:symbolize=1'
    }

    if ([string]::IsNullOrWhiteSpace($env:UBSAN_OPTIONS)) {
        $env:UBSAN_OPTIONS = 'print_stacktrace=1:halt_on_error=1'
    }
}

if ($runValidationBenchmark) {
    $cxxDefines += ' -DRUN_VALIDATION_BENCHMARK'
}

$cFlags = ''
$cxxFlags = $cxxDefines
$exeLinkerFlags = ''
$sharedLinkerFlags = ''
if (-not [string]::IsNullOrWhiteSpace($sanitizerFlags)) {
    $cFlags = $sanitizerFlags
    $cxxFlags += " $sanitizerFlags"
    $exeLinkerFlags = $sanitizerFlags
    $sharedLinkerFlags = $sanitizerFlags
}

$cmakeArgs = @(
    '-B', 'Builds',
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_C_COMPILER_LAUNCHER=sccache',
    '-DCMAKE_CXX_COMPILER_LAUNCHER=sccache',
    '-DCMAKE_OBJC_COMPILER_LAUNCHER=sccache',
    '-DCMAKE_OBJCXX_COMPILER_LAUNCHER=sccache',
    "-DCMAKE_C_FLAGS=$cFlags",
    "-DCMAKE_CXX_FLAGS=$cxxFlags",
    "-DCMAKE_EXE_LINKER_FLAGS=$exeLinkerFlags",
    "-DCMAKE_SHARED_LINKER_FLAGS=$sharedLinkerFlags"
)

if ($MatrixJUCE -eq 'JUCE7') {
    $cmakeArgs += '-DJUCE7=ON'
}

if ($MatrixCpp -eq '20') {
    $cmakeArgs += '-DCPP20=ON'
}

$cmakeArgs += '.'

if ([string]::IsNullOrWhiteSpace($env:NPROC)) {
    $env:numCpusCores = [string][Environment]::ProcessorCount
}
else {
    $env:numCpusCores = $env:NPROC
}

$ctestParallelLevel = '1'
if (-not [string]::IsNullOrWhiteSpace($env:CTEST_PARALLEL_LEVEL)) {
    $ctestParallelLevel = $env:CTEST_PARALLEL_LEVEL
}

Invoke-NativeCommand -Command 'cmake' -Args $cmakeArgs -FailureLabel 'cmake configure'
Invoke-NativeCommand -Command 'cmake' -Args @('--build', 'Builds') -FailureLabel 'cmake build'

$ctestArgs = @('--test-dir', 'Builds', '--parallel', $ctestParallelLevel, '--output-on-failure')

if (-not [string]::IsNullOrWhiteSpace($env:CTEST_EXTRA_ARGS)) {
    $ctestArgs += ($env:CTEST_EXTRA_ARGS -split '\s+')
}

if ($runValidationBenchmark) {
    $ctestArgs += '-V'
}

$numTests = 1
for ($i = 1; $i -le $numTests; $i++) {
    Write-Host "Running tests - iteration $i"
    Invoke-NativeCommand -Command 'ctest' -Args $ctestArgs -FailureLabel 'ctest'
}

if (Get-Command -Name sccache -ErrorAction SilentlyContinue) {
    try {
        & sccache --show-stats
    }
    catch {
    }
}
