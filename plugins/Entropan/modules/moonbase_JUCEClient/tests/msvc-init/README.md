# vcvars PowerShell helpers

Use the Microsoft C++ toolset setup from PowerShell.

## vcvarsall.ps1

Loads MSVC environment variables using `vcvarsall.bat`.

- Output mode (default): writes `export ...` lines for Bash-style callers
- Apply mode: sets variables into the current PowerShell process

Examples:

```powershell
# Emit Bash exports
./vcvarsall.ps1 x64

# Apply environment directly to current PowerShell session
./vcvarsall.ps1 -Apply x64
```

## vcvarsrun.ps1

Applies `vcvarsall` env and runs a command.

Example:

```powershell
./vcvarsrun.ps1 x64 -- cl /nologo /EHsc /Fe:hello.exe hello.cpp
```

## Notes

- Windows-only scripts
- `vswhere.exe` is used to locate Visual Studio automatically
- For `vcvarsall.bat` arguments, see Microsoft docs:
  https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170#vcvarsall-syntax
