[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw 'vcvarsrun.ps1 is Windows-only.'
}

if ($Arguments.Count -eq 0) {
    Write-Host 'Usage: vcvarsrun.ps1 [vcvarsall args] -- command [args...]'
    throw 'Missing arguments.'
}

$separatorIndex = [Array]::IndexOf($Arguments, '--')
if ($separatorIndex -lt 0) {
    throw "Missing '--' separator before command."
}

$vcvarsArgs = @()
if ($separatorIndex -gt 0) {
    $vcvarsArgs = $Arguments[0..($separatorIndex - 1)]
}

$commandArgs = @()
if ($separatorIndex -lt ($Arguments.Count - 1)) {
    $commandArgs = $Arguments[($separatorIndex + 1)..($Arguments.Count - 1)]
}

if ($commandArgs.Count -eq 0) {
    throw 'No command specified after -- separator.'
}

$vcvarsAllScript = Join-Path $PSScriptRoot 'vcvarsall.ps1'
& $vcvarsAllScript -Apply @vcvarsArgs

$command = $commandArgs[0]
$commandTail = @()
if ($commandArgs.Count -gt 1) {
    $commandTail = $commandArgs[1..($commandArgs.Count - 1)]
}

& $command @commandTail
$exitCode = 0
if ($null -ne $LASTEXITCODE) {
    $exitCode = [int]$LASTEXITCODE
}

exit $exitCode
