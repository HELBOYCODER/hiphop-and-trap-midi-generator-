// Built-in domain knowledge: what each scale feels like, what each knob does, and ready-made
// style recipes. Used by the `style_guide` tool so an LLM (and through it, the user) knows exactly
// which key/scale/settings lead to a given sound.
#pragma once
#include "Json.h"
#include <string>
#include <vector>

namespace hhmg {

struct Recipe {
    std::string id, label_en, label_fa;
    std::vector<std::string> keywords;     // lower-case, English + Persian
    double rnb, trap, hiphop;
    std::string scale;
    int bpmMin, bpmMax;
    double complexity, humanize, swing, velocityVariation;
    bool bass;
    int bars;
    std::vector<int> progression;          // 1-based degrees
    std::string why_en, why_fa;
};

const std::vector<Recipe>& recipes();
// args: {query?: string, style?: rnb|trap|hiphop, mood?: string, include_help?: bool}
Json styleGuide(const Json& args);

} // namespace hhmg
