#!/bin/bash

WINDOWS_PS_HELPER="$(cd -- "$(dirname -- "$0")" && pwd)/../Scripts/WindowsPowerShellSibling.sh"
if [ ! -f "$WINDOWS_PS_HELPER" ]; then
    echo "Error: Windows PowerShell helper not found at $WINDOWS_PS_HELPER"
    exit 1
fi

. "$WINDOWS_PS_HELPER"
moonbase_delegate_to_windows_powershell_if_needed "$0" 0 "$@"

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    CYGWIN*)    machine=Cygwin;;
    MINGW*)     machine=Win;;
    *)          machine="UNKNOWN:${unameOut}"
esac

currentos=${machine}

input_config_json="$1"
if [ -z "$input_config_json" ]; then
    echo "Usage: $0 <path_to_config_json>"
    exit 1
fi

#verify json format
if ! jq empty "$input_config_json" >/dev/null 2>&1; then
    echo "Invalid JSON format in $input_config_json"
    exit 2
fi

thisFileDir="$(dirname "$0")"

if [ $currentos = Mac ]; then
    codemod="$thisFileDir/integrity-check-codemod-darwin"
elif [ $currentos = Win ]; then
    codemod="$thisFileDir/integrity-check-codemod-win-amd64.exe"
elif [ $currentos = Linux ]; then
    codemod="$thisFileDir/integrity-check-codemod-linux-amd64"
else
    echo "Unsupported OS: $currentos"
    exit 3
fi
echo "Code mod: $codemod"

"$codemod" --config "$input_config_json" --check "$thisFileDir/../Source/Implementations/IntegrityCheck.generated.h" || exit $?

exit 0
