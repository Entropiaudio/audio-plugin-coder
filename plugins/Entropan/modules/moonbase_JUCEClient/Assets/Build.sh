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

BASEDIR="$(realpath "$(dirname "$0")")"
cd "$BASEDIR"

if [ $currentos = Mac ]; then
    binaryBuilder="$BASEDIR/binaryBuilder"
elif [ $currentos = Win ]; then
    binaryBuilder="$BASEDIR/binaryBuilder.exe"
else
    echo "Unsupported OS: $currentos"
    echo "Won't be able to rebuild binary assets and using committed version..."
    exit 0
fi

echo "Running asset binary builder..."

if [ "${MOONBASE_VERBOSE_ASSET_BUILD:-0}" = "1" ]; then
    "$binaryBuilder" "$BASEDIR/Source" "$BASEDIR" "MoonbaseBinary"
    returnVal=$?
else
    "$binaryBuilder" "$BASEDIR/Source" "$BASEDIR" "MoonbaseBinary" >/dev/null 2>&1
    returnVal=$?
fi

if [ $returnVal -ne 0 ]; then
    echo "Error: binaryBuilder failed with exit code $returnVal"
    exit $returnVal
fi

binaryIncludes="$BASEDIR/BinaryIncludes.cpp"
if [ -f "$binaryIncludes" ]; then
    rm "$binaryIncludes"
fi
touch "$binaryIncludes"

for file in ./*.cpp; do
    if [ "$(realpath "$file")" != "$binaryIncludes" ]; then
        echo "#include \"${file:2}\"" >> "$binaryIncludes"
    fi
done

exit 0
