#!/usr/bin/env bash
# Entropan — macOS beta release: universal build → Developer ID sign → signed
# .pkg installer → Apple notarisation → staple → verify.
#
#   ./scripts/macos/release-macos.sh                 # full run
#   ./scripts/macos/release-macos.sh --skip-notarize # sign + pkg only
#   NOTARY_PROFILE=other-name ./scripts/macos/release-macos.sh
#
# The notarisation password is never handled here — it lives in the keychain
# profile created by:
#   xcrun notarytool store-credentials "entropan-notary" \
#       --apple-id <id> --team-id 7CM4GS8JY9
set -euo pipefail

PLUGIN="Entropan"
VERSION="${VERSION:-0.8.0}"
TEAM_ID="7CM4GS8JY9"
APP_CERT="Developer ID Application: noam ben shabat (${TEAM_ID})"
INSTALLER_CERT="Developer ID Installer: noam ben shabat (${TEAM_ID})"
NOTARY_PROFILE="${NOTARY_PROFILE:-entropan-notary}"
BUNDLE_ID="com.entropiaudio.${PLUGIN}"
DEPLOY_TARGET="10.15"
ARCHS="x86_64;arm64"

SKIP_NOTARIZE=0
[[ "${1:-}" == "--skip-notarize" ]] && SKIP_NOTARIZE=1

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
BUILD="build-release"
OUT="dist/${PLUGIN}-${VERSION}-macOS"
ART="${BUILD}/plugins/${PLUGIN}/${PLUGIN}_artefacts/Release"

step() { printf '\n\033[1;36m▶ %s\033[0m\n' "$*"; }
die()  { printf '\n\033[1;31m✗ %s\033[0m\n' "$*" >&2; exit 1; }

# ── 1. universal build ────────────────────────────────────────────────────────
step "Configuring universal build (${ARCHS}, deployment target ${DEPLOY_TARGET})"
cmake -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOY_TARGET}" \
    -DENTROPAN_BUILD_TESTS=OFF >/dev/null

step "Building VST3 + AU"
cmake --build "$BUILD" --target ${PLUGIN}_VST3 ${PLUGIN}_AU --config Release

VST3="${ART}/VST3/${PLUGIN}.vst3"
AU="${ART}/AU/${PLUGIN}.component"
[[ -d "$VST3" && -d "$AU" ]] || die "build artefacts missing"

for b in "$VST3" "$AU"; do
    archs=$(lipo -archs "$b/Contents/MacOS/${PLUGIN}")
    [[ "$archs" == *x86_64* && "$archs" == *arm64* ]] \
        || die "$(basename "$b") is not universal (got: $archs)"
    echo "  $(basename "$b"): $archs"
done

# ── 2. sign the bundles ───────────────────────────────────────────────────────
# --options runtime (hardened runtime) is mandatory for notarisation;
# --timestamp binds a trusted timestamp so the signature outlives the cert.
step "Signing bundles with Developer ID Application"
for b in "$VST3" "$AU"; do
    # macOS stamps com.apple.provenance (and Finder detritus) on files written
    # by quarantined-origin tooling, and codesign hard-fails on any xattr
    # ("resource fork, Finder information, or similar detritus not allowed").
    # Strip before every sign — first hit on the Mac mini's fresh build tree.
    xattr -cr "$b"
    codesign --force --options runtime --timestamp --sign "$APP_CERT" "$b"
    codesign --verify --strict --verbose=2 "$b" 2>&1 | sed 's/^/  /'
done

# ── 3. build the installer ────────────────────────────────────────────────────
step "Building component packages"
rm -rf "$OUT" && mkdir -p "$OUT/_stage/vst3" "$OUT/_stage/au" "$OUT/_pkgs"
cp -R "$VST3" "$OUT/_stage/vst3/"
cp -R "$AU"   "$OUT/_stage/au/"

pkgbuild --root "$OUT/_stage/vst3" --identifier "${BUNDLE_ID}.vst3" \
         --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$OUT/_pkgs/${PLUGIN}-VST3.pkg" >/dev/null
pkgbuild --root "$OUT/_stage/au" --identifier "${BUNDLE_ID}.component" \
         --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" \
         "$OUT/_pkgs/${PLUGIN}-AU.pkg" >/dev/null

cat > "$OUT/_pkgs/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>${PLUGIN} ${VERSION}</title>
    <organization>com.entropiaudio</organization>
    <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3" description="Installs to /Library/Audio/Plug-Ins/VST3">
        <pkg-ref id="${BUNDLE_ID}.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit" description="Installs to /Library/Audio/Plug-Ins/Components">
        <pkg-ref id="${BUNDLE_ID}.component"/>
    </choice>
    <pkg-ref id="${BUNDLE_ID}.vst3" version="${VERSION}">${PLUGIN}-VST3.pkg</pkg-ref>
    <pkg-ref id="${BUNDLE_ID}.component" version="${VERSION}">${PLUGIN}-AU.pkg</pkg-ref>
</installer-gui-script>
XML

PKG="$OUT/${PLUGIN}-${VERSION}.pkg"
step "Building + signing installer"
productbuild --distribution "$OUT/_pkgs/distribution.xml" \
             --package-path "$OUT/_pkgs" \
             --sign "$INSTALLER_CERT" --timestamp \
             "$PKG" >/dev/null
rm -rf "$OUT/_stage" "$OUT/_pkgs"
cp "plugins/${PLUGIN}/Documentation/BETA-README.md" "$OUT/README.md"   # ships next to the pkg

# ── 4. notarise + staple ──────────────────────────────────────────────────────
if [[ $SKIP_NOTARIZE -eq 1 ]]; then
    printf '\n\033[1;33m⚠ Skipped notarisation — testers will hit Gatekeeper.\033[0m\n'
    echo "Installer: $PKG"
    exit 0
fi

step "Submitting to Apple for notarisation (this takes a few minutes)"
xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait \
    || die "notarisation failed — run: xcrun notarytool log <id> --keychain-profile $NOTARY_PROFILE"

step "Stapling ticket"
xcrun stapler staple "$PKG"

# ── 5. verify the way a tester's Mac will ─────────────────────────────────────
step "Verifying"
xcrun stapler validate "$PKG"
spctl -a -vvv -t install "$PKG"

printf '\n\033[1;32m✓ Notarised installer ready:\033[0m %s\n' "$PKG"
