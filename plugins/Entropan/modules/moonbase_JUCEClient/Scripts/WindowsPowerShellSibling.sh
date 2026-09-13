#!/bin/bash

moonbase_is_windows_shell() {
    case "${OSTYPE:-}" in
        msys*|cygwin*|win32*) return 0 ;;
    esac

    case "$(uname -s 2>/dev/null || true)" in
        CYGWIN*|MINGW*|MSYS*) return 0 ;;
    esac

    [ "${OS:-}" = "Windows_NT" ]
}

moonbase_is_script_sourced() {
    local script_path="$1"

    if [ -n "${BASH_SOURCE:-}" ] && [ "$script_path" != "$0" ]; then
        return 0
    fi

    return 1
}

moonbase_run_windows_powershell_sibling() {
    local script_path="$1"
    shift

    local script_dir script_name script_base ps_script ps_cmd

    script_dir="$(cd -- "$(dirname -- "$script_path")" && pwd)"
    script_name="$(basename -- "$script_path")"
    script_base="${script_name%.*}"
    ps_script="$script_dir/$script_base.ps1"

    if [ ! -f "$ps_script" ]; then
        echo "Error: PowerShell sibling script not found at $ps_script"
        exit 1
    fi

    if command -v cygpath >/dev/null 2>&1; then
        ps_script="$(cygpath -w "$ps_script")"
    fi

    if command -v powershell.exe >/dev/null 2>&1; then
        ps_cmd="powershell.exe"
    elif command -v powershell >/dev/null 2>&1; then
        ps_cmd="powershell"
    elif command -v pwsh.exe >/dev/null 2>&1; then
        ps_cmd="pwsh.exe"
    elif command -v pwsh >/dev/null 2>&1; then
        ps_cmd="pwsh"
    else
        echo "Error: PowerShell is required on Windows."
        exit 1
    fi

    "$ps_cmd" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$ps_script" "$@"
    exit $?
}

moonbase_delegate_to_windows_powershell_if_needed() {
    local script_path="$1"
    local skip_if_sourced="$2"
    shift 2

    if [ "${MOONBASE_FORCE_BASH:-0}" = "1" ]; then
        return 0
    fi

    if ! moonbase_is_windows_shell; then
        return 0
    fi

    if [ "$skip_if_sourced" = "1" ] && moonbase_is_script_sourced "$script_path"; then
        return 0
    fi

    moonbase_run_windows_powershell_sibling "$script_path" "$@"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    moonbase_delegate_to_windows_powershell_if_needed "${BASH_SOURCE[0]}" 1 "$@"
fi
