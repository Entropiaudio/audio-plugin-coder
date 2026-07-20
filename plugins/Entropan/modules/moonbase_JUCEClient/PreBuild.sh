#!/bin/bash

SCRIPT_PATH="${BASH_SOURCE[0]:-$0}"
WINDOWS_PS_HELPER="$(cd -- "$(dirname -- "$SCRIPT_PATH")" && pwd)/Scripts/WindowsPowerShellSibling.sh"
if [[ ! -f "$WINDOWS_PS_HELPER" ]]; then
    echo "Error: Windows PowerShell helper not found at $WINDOWS_PS_HELPER"
    exit 1
fi

source "$WINDOWS_PS_HELPER"
moonbase_delegate_to_windows_powershell_if_needed "$SCRIPT_PATH" 0 "$@"

# Checking JQ dependency
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if ! command -v jq >/dev/null 2>&1; then
        echo "Error: jq is required but not installed. Please install jq and try again."
        echo "You can install it using your package manager, e.g., sudo apt install jq"
        exit 1
    fi
fi

# Checking for config file argument
MB_CONFIG_JSON="$1"
if [ ! -f "$MB_CONFIG_JSON" ]; then
    echo "Error: Config file not found at $MB_CONFIG_JSON"
    echo "Usage: $0 <path_to_config.json>"
    exit 1
fi


MB_CLIENT_MODULE_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
PREBUILD_CACHE_HELPERS="${MB_CLIENT_MODULE_DIR}/Scripts/PreBuildCacheHelpers.sh"
if [[ ! -f "${PREBUILD_CACHE_HELPERS}" ]]; then
    echo "Error: prebuild cache helper script not found at ${PREBUILD_CACHE_HELPERS}"
    exit 1
fi
source "${PREBUILD_CACHE_HELPERS}"

run_moonbase_prebuild_cached "${MB_CLIENT_MODULE_DIR}" "${MB_CONFIG_JSON}" || {
    moonbase_prebuild_result=$?
    echo "Error: Moonbase PreBuild failed with code ${moonbase_prebuild_result}."
    exit "${moonbase_prebuild_result}"
}

exit 0
