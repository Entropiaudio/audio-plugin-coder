Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw "WindowsPowerShellSibling.ps1 is Windows-only. Use WindowsPowerShellSibling.sh on macOS/Linux."
}

# Helper module; intentionally no-op when executed directly.
