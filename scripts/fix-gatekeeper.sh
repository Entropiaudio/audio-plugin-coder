#!/bin/bash
# Remove macOS Gatekeeper quarantine from all installed VST3 plugins
for plugin in ~/Library/Audio/Plug-Ins/VST3/*.vst3; do
    if [ -e "$plugin" ]; then
        xattr -cr "$plugin"
        echo "Cleared quarantine: $(basename "$plugin")"
    fi
done
echo "Done. Rescan plugins in your DAW."
