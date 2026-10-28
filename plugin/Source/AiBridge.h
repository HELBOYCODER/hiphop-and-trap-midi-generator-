#pragma once
#include <juce_core/juce_core.h>
#include <hhmg/Tools.h>
#include <functional>
#include <optional>

// Text-prompt -> MIDI. The model never writes notes: it calls the same tools the MCP server exposes
// (scale_notes, snap_to_scale, list_progressions, generate_midi ...) and the engine does the writing,
// which keeps the output inside the scale no matter what the model says.
namespace AiBridge
{
enum class Provider { gemini = 0, openAiCompatible = 1 };

struct Settings
{
    Provider provider = Provider::gemini;
    juce::String model = "gemini-2.5-flash";
    juce::String endpoint;  // OpenAI-compatible base URL, e.g. http://localhost:11434/v1
    juce::String apiKey;
};

struct Outcome
{
    bool ok = false;
    juce::String message;                 // model's reply or the error text
    std::optional<hhmg::Result> result;   // last generate_midi result, if any
};

// Blocking: call from a worker thread. `shouldCancel` is polled between network calls.
// `analyzeClip` lets the model inspect the clip loaded in the plugin (analyze_current_clip tool).
Outcome run (const Settings& settings, const juce::String& prompt, const juce::String& currentSettingsJson,
             const hhmg::PatternLibrary& library, const std::function<bool()>& shouldCancel,
             const std::function<hhmg::Json()>& analyzeClip);
}
