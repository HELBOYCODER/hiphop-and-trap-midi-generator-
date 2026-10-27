# Build the Windows x64 VST3 (run in a "x64 Native Tools" / VS 2022 PowerShell).
#   .\scripts\build.ps1
#   .\scripts\build.ps1 -JucePath C:\dev\JUCE
param([string]$JucePath = "")
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$args = @("-S", ".", "-B", "build", "-A", "x64")
if ($JucePath) { $args += "-DJUCE_PATH=$JucePath" }
cmake @args
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

New-Item -ItemType Directory -Force dist | Out-Null
Remove-Item -Recurse -Force dist\*.vst3 -ErrorAction SilentlyContinue
Copy-Item -Recurse build\plugin\HipHopMidiGenerator_artefacts\Release\VST3\*.vst3 dist\
Copy-Item build\Release\hhmg-cli.exe dist\ -ErrorAction SilentlyContinue
Write-Host "VST3 -> dist\   Install: copy the .vst3 folder to C:\Program Files\Common Files\VST3\"
