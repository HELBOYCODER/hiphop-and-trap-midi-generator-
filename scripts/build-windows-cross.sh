#!/usr/bin/env bash
# Cross-compile the Windows x64 VST3 on Linux (clang-cl + lld + the Windows SDK/CRT fetched by xwin).
# Needs: clang-18 + lld-18 + llvm-18 (apt), cargo (for xwin), cmake, a JUCE 8 checkout.
# Running xwin means accepting Microsoft's SDK licence. The result was not run on real Windows by the author.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${JUCE_PATH:?set JUCE_PATH to a JUCE 8.0.x checkout}"
XWIN=${XWIN:-$PWD/build-win-sdk}
JUCE_NATIVE=${JUCE_NATIVE:-$PWD/build-juce-native}

command -v xwin >/dev/null || cargo install xwin --locked
[ -d "$XWIN/crt" ] || xwin --accept-license --arch x86_64 --variant desktop splat --output "$XWIN"
# the SDK ships lower/upper-case file names; JUCE includes them with other casing
python3 - "$XWIN" <<'PY'
import os, re, sys, glob
x = sys.argv[1]
for sub in ("sdk/lib/um/x86_64", "sdk/lib/ucrt/x86_64"):
    d = os.path.join(x, sub)
    for n in ("DbgHelp", "Dwrite", "D2d1", "DComp", "Dxgi", "D3d11", "Dwmapi", "Shlwapi", "Wininet", "Winmm", "Version", "Comdlg32", "Comctl32", "Imm32", "Ole32", "Oleaut32"):
        want = n + ".lib"
        have = {f.lower(): f for f in os.listdir(d)} if os.path.isdir(d) else {}
        if want.lower() in have and want not in os.listdir(d):
            os.symlink(have[want.lower()], os.path.join(d, want))
PY

# JUCE's own host tool (juceaide) is needed for cross builds
cmake -S "$JUCE_PATH" -B "$JUCE_NATIVE" -DJUCE_BUILD_EXTRAS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$JUCE_NATIVE/install" >/dev/null
cmake --install "$JUCE_NATIVE" >/dev/null

# the VST3 manifest helper is a Windows exe and cannot run on Linux; moduleinfo.json is optional
STUB=$(mktemp -d); printf '#!/bin/sh\nexit 0\n' > "$STUB/juce_vst3_helper"; chmod +x "$STUB/juce_vst3_helper"

cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/clang-cl-x86_64-windows.cmake -DXWIN="$XWIN" \
  -DJUCE_PATH="$JUCE_PATH" -DJUCE_TOOL_INSTALL_DIR="$JUCE_NATIVE/install/bin/JUCE-8.0.6" -DCMAKE_BUILD_TYPE=Release -DHHMG_BUILD_TESTS=OFF
PATH="$STUB:$PATH" cmake --build build-win --target HipHopMidiGenerator_VST3 --parallel
echo "VST3 -> build-win/plugin/HipHopMidiGenerator_artefacts/Release/VST3/"
