#!/bin/bash

set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]:-$0}"
SCRIPT_DIR="$(cd -- "$(dirname -- "$SCRIPT_PATH")" && pwd)"
MODULE_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

WINDOWS_PS_HELPER="${MODULE_DIR}/Scripts/WindowsPowerShellSibling.sh"
if [[ ! -f "$WINDOWS_PS_HELPER" ]]; then
    echo "Error: Windows PowerShell helper not found at $WINDOWS_PS_HELPER"
    exit 1
fi

source "$WINDOWS_PS_HELPER"
moonbase_delegate_to_windows_powershell_if_needed "$SCRIPT_PATH" 0 "$@"

show_usage () {
    cat <<'EOF'
Usage: GenerateConfigDetails <path-to-json-file> [path-to-output-header] [--seed <number>] [--out <path>]
EOF
}

if [[ $# -lt 1 ]]; then
    show_usage
    exit 1
fi

JSON_FILE="$1"
shift

OUT_FILE=""
SEED_VALUE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --help|-h)
            show_usage
            exit 0
            ;;
        --seed)
            if [[ $# -lt 2 ]]; then
                echo "Error: --seed requires a value."
                show_usage
                exit 1
            fi
            SEED_VALUE="$2"
            shift 2
            ;;
        --out)
            if [[ $# -lt 2 ]]; then
                echo "Error: --out requires a path."
                show_usage
                exit 1
            fi
            OUT_FILE="$2"
            shift 2
            ;;
        --*)
            echo "Error: unknown option: $1"
            show_usage
            exit 1
            ;;
        *)
            if [[ -z "$OUT_FILE" ]]; then
                OUT_FILE="$1"
                shift
            else
                echo "Error: unexpected argument: $1"
                show_usage
                exit 1
            fi
            ;;
    esac
done

if [[ ! -f "$JSON_FILE" ]]; then
    echo "Error: file not found: $JSON_FILE"
    exit 1
fi

if [[ -z "$OUT_FILE" ]]; then
    OUT_FILE="${MODULE_DIR}/Source/ConfigDetails.generated.h"
fi

mkdir -p "$(dirname -- "$OUT_FILE")"

BINARY_PATH=""
case "$(uname -s 2>/dev/null || true)" in
    Darwin*)
        BINARY_PATH="${SCRIPT_DIR}/GenerateConfigDetails_macOS"
        ;;
    Linux*)
        BINARY_PATH="${SCRIPT_DIR}/GenerateConfigDetails_linux"
        ;;
    CYGWIN*|MINGW*|MSYS*)
        BINARY_PATH="${SCRIPT_DIR}/GenerateConfigDetails_windows.exe"
        ;;
    *)
        if moonbase_is_windows_shell; then
            BINARY_PATH="${SCRIPT_DIR}/GenerateConfigDetails_windows.exe"
        else
            echo "Error: unsupported OS for GenerateConfigDetails binary."
            exit 1
        fi
        ;;
esac

if [[ ! -f "$BINARY_PATH" ]]; then
    echo "Error: GenerateConfigDetails binary not found at $BINARY_PATH"
    exit 1
fi

if [[ "$BINARY_PATH" != *.exe ]] && [[ ! -x "$BINARY_PATH" ]]; then
    chmod +x "$BINARY_PATH" 2>/dev/null || true
fi

JSON_ARG="$JSON_FILE"
OUT_ARG="$OUT_FILE"

if moonbase_is_windows_shell && command -v cygpath >/dev/null 2>&1; then
    BINARY_PATH="$(cygpath -w "$BINARY_PATH")"
    JSON_ARG="$(cygpath -w "$JSON_FILE")"
    OUT_ARG="$(cygpath -w "$OUT_FILE")"
fi

if [[ -n "$SEED_VALUE" ]]; then
    "$BINARY_PATH" "$JSON_ARG" "$OUT_ARG" --seed "$SEED_VALUE"
else
    "$BINARY_PATH" "$JSON_ARG" "$OUT_ARG"
fi
