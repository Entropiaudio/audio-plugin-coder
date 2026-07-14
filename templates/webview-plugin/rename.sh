#!/usr/bin/env bash
# Clone-and-rename a fresh WebView plugin from this template.
#
#   ./rename.sh <PluginName> [4charCode] [dest-dir]
#
#   PluginName  CamelCase, no spaces (e.g. TapeWidth)
#   4charCode   JUCE PLUGIN_CODE, exactly 4 chars, first uppercase (default: derived)
#   dest-dir    where to write the renamed copy (default: ../../plugins/<PluginName>)
#
# Copies the template, substitutes __PLUGIN__ / __CODE__, and drops rename.sh.
set -euo pipefail

NAME="${1:-}"
[ -z "$NAME" ] && { echo "usage: ./rename.sh <PluginName> [4charCode] [dest-dir]"; exit 1; }

CODE="${2:-}"
if [ -z "$CODE" ]; then
  # derive a 4-char code: first letter upper + next 3 lower of the name, padded
  CODE="$(printf '%s' "$NAME" | tr -cd '[:alnum:]' | cut -c1-4)"
  CODE="$(printf '%s' "$CODE" | awk '{ printf "%s%s", toupper(substr($0,1,1)), tolower(substr($0,2)) }')"
  while [ "${#CODE}" -lt 4 ]; do CODE="${CODE}x"; done
fi
[ "${#CODE}" -ne 4 ] && { echo "error: code must be exactly 4 chars (got '$CODE')"; exit 1; }

SRC="$(cd "$(dirname "$0")" && pwd)"
DEST="${3:-$SRC/../../plugins/$NAME}"

[ -e "$DEST" ] && { echo "error: destination already exists: $DEST"; exit 1; }
mkdir -p "$DEST"
cp -R "$SRC/." "$DEST/"
rm -f "$DEST/rename.sh"

# substitute placeholders in text files (skip binaries/fonts)
find "$DEST" -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.txt" -o -name "*.html" -o -name "*.md" \) -print0 \
  | while IFS= read -r -d '' f; do
      LC_ALL=C sed -i.bak -e "s/__PLUGIN__/$NAME/g" -e "s/__CODE__/$CODE/g" "$f" && rm -f "$f.bak"
    done

echo "Created $DEST"
echo "  plugin name : $NAME"
echo "  plugin code : $CODE"
echo "Next: build from the repo root (the root CMakeLists auto-discovers plugins/*),"
echo "  then find the VST3/AU under build/plugins/$NAME/${NAME}_artefacts/."
