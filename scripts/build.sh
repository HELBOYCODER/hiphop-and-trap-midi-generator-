#!/usr/bin/env bash
# Build the VST3 (and run the tests) on macOS or Linux.
#   ./scripts/build.sh            -> build/ + dist/
#   JUCE_PATH=~/JUCE ./scripts/build.sh   (use a local JUCE checkout instead of downloading)
set -euo pipefail
cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release ${JUCE_PATH:+-DJUCE_PATH="$JUCE_PATH"}
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

mkdir -p dist
rm -rf dist/*.vst3
cp -R build/plugin/HipHopMidiGenerator_artefacts/Release/VST3/*.vst3 dist/
cp build/hhmg-cli dist/ 2>/dev/null || cp build/Release/hhmg-cli dist/ 2>/dev/null || true
echo "VST3 -> dist/"
case "$(uname)" in
  Darwin) echo "Install: cp -R dist/*.vst3 ~/Library/Audio/Plug-Ins/VST3/" ;;
  *)      echo "Install: cp -R dist/*.vst3 ~/.vst3/" ;;
esac
