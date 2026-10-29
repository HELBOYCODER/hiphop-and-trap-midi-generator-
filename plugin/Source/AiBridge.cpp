#include "AiBridge.h"

namespace AiBridge
{
namespace
{
const char* kSystemPrompt =
    "You are the composition guide inside a MIDI plugin for R&B, trap and hip-hop producers (trap and hip-hop first). "
    "You cannot write notes yourself: you MUST use the provided tools; the engine writes the notes and keeps every note in the scale. "
    "You have access to the plugin's learned library of real producer MIDI loops and to MIDI analysis. Know the elements: 13 scales "
    "(major, minor, harmonic_minor, melodic_minor, dorian, phrygian, lydian, mixolydian, locrian, phrygian_dominant, pentatonic_major, "
    "pentatonic_minor, blues), a style blend (rnb/trap/hiphop 0-1), complexity, humanize, swing, velocity_variation, bars, bpm, "
    "chord/melody/808 parts, an optional chord progression of degrees 1-7, and reference ids for progression and rhythm. "
    "Workflow: (1) If the user mentions a MIDI file, a recording, a reference, 'this clip' or has imported/recorded/generated something, call analyze_current_clip "
    "(or analyze_midi / analyze_audio with a path) to recognise its style: key, mode, BPM, chord degrees, swing, looseness and R&B/Trap/Hip-Hop scores. "
    "When they want something like their own clip, call similar_to_current_clip (it draws on the learned reference dataset). "
    "When they name an artist or vibe ('Metro Boomin type beat', 'dark drill'), call find_type_beat_style for the aggregate BPM/key/tag statistics of that style and use its suggested settings; say how large its sample_size is. "
    "(2) Call style_guide with the user's words (any language) for matching recipes; call find_references to find real loops with a similar "
    "key, tempo and style. (3) To combine things, call generate_midi once, taking the progression from one reference (reference_progression), "
    "the melody rhythm from another (reference_rhythm), and the sound settings from the analysis or recipe. Explicit user values always win. "
    "(4) Reply in the user's language in 3-6 short lines: the style you recognised and why, what you combined (name the references), key/scale/BPM/chords, "
    "and 1-2 tips for tuning (which knob to move). Never pass output_path. Do not invent tool names, ids or settings; only use ids returned by find_references.";

hhmg::Json toolsForGemini()
{
    using hhmg::Json;
    Json decls = Json::array();
    const Json schemas = hhmg::toolSchemas();
    for (const auto& t : schemas.items())
    {
        Json d = Json::object();
        d.set ("name", t["name"]);
        d.set ("description", t["description"]);
        const Json& s = t["inputSchema"];
        if (s["properties"].size() > 0)
        {
            Json params = Json::object();
            params.set ("type", "object");
            params.set ("properties", s["properties"]);
            if (s["required"].size() > 0) params.set ("required", s["required"]);
            d.set ("parameters", params);
        }
        decls.push (d);
    }
    Json tool = Json::object();
    tool.set ("functionDeclarations", decls);
    Json arr = Json::array();
    arr.push (tool);
    return arr;
}

hhmg::Json toolsForOpenAi()
{
    using hhmg::Json;
    Json arr = Json::array();
    for (const auto& t : hhmg::toolSchemas().items())
    {
        Json fn = Json::object();
        fn.set ("name", t["name"]);
        fn.set ("description", t["description"]);
        fn.set ("parameters", t["inputSchema"]);
        Json w = Json::object();
        w.set ("type", "function");
        w.set ("function", fn);
        arr.push (w);
    }
    return arr;
}

// Runs one tool for the model. generate_midi is intercepted so nothing is written to disk.
hhmg::Json execTool (const std::string& name, hhmg::Json args, const hhmg::PatternLibrary& lib, Outcome& out,
                     const ClipTools& clipTools)
{
    if (name == "generate_midi")
    {
        hhmg::Request rq = hhmg::requestFromJson (args);
        std::string refError, refNote = hhmg::applyReferences (rq, args, lib, refError);
        if (! refError.empty())
        {
            hhmg::Json e = hhmg::Json::object(); e.set ("ok", false); e.set ("error", refError);
            return e;
        }
        hhmg::Result r = hhmg::generate (rq, lib);
        out.result = r;
        hhmg::Json j = hhmg::resultToJson (r, false);
        j.set ("ok", true);
        if (! refNote.empty()) j.set ("used_references", refNote);
        return j;
    }
    if (name == "analyze_current_clip" && clipTools.analyze) return clipTools.analyze();
    if (name == "similar_to_current_clip" && clipTools.similar) return clipTools.similar (args);
    args.set ("output_path", "");   // a model must never write files on the user's computer
    return hhmg::callTool (name, args, lib);
}

struct HttpReply { int status = 0; std::string body; };

HttpReply postJson (const juce::String& url, const juce::String& headers, const hhmg::Json& body)
{
    HttpReply reply;
    int status = 0;
    juce::URL u = juce::URL (url).withPOSTData (juce::String (body.dump()));
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                       .withConnectionTimeoutMs (45000)
                       .withExtraHeaders ("Content-Type: application/json\r\n" + headers)
                       .withHttpRequestCmd ("POST")
                       .withNumRedirectsToFollow (3)
                       .withStatusCode (&status);
    if (auto stream = u.createInputStream (options))
        reply.body = stream->readEntireStreamAsString().toStdString();
    reply.status = status;
    return reply;
}

juce::String errorText (const HttpReply& r)
{
    if (r.status == 0) return "Could not reach the AI service (check internet / endpoint).";
    juce::String detail;
    try
    {
        auto j = hhmg::Json::parse (r.body);
        detail = juce::String (j["error"]["message"].str (""));
        if (detail.isEmpty()) detail = juce::String (j["error"].str (""));
    }
    catch (...) {}
    return "AI service error (HTTP " + juce::String (r.status) + ")" + (detail.isNotEmpty() ? ": " + detail : "");
}

Outcome runGemini (const Settings& s, const juce::String& prompt, const juce::String& ctx,
                   const hhmg::PatternLibrary& lib, const std::function<bool()>& cancel,
                   const ClipTools& clipTools)
{
    using hhmg::Json;
    Outcome out;
    const juce::String url = "https://generativelanguage.googleapis.com/v1beta/models/" + s.model.trim() + ":generateContent";
    const juce::String headers = "x-goog-api-key: " + s.apiKey.trim();

    Json contents = Json::array();
    {
        Json u = Json::object(); u.set ("role", "user");
        Json parts = Json::array(); Json p = Json::object();
        p.set ("text", (prompt + "\n\nCurrent plugin settings (use as defaults): " + ctx).toStdString());
        parts.push (p); u.set ("parts", parts);
        contents.push (u);
    }

    for (int turn = 0; turn < 6; ++turn)
    {
        if (cancel()) { out.message = "Cancelled."; return out; }
        Json body = Json::object();
        Json sys = Json::object(); Json sp = Json::array(); Json spt = Json::object();
        spt.set ("text", kSystemPrompt); sp.push (spt); sys.set ("parts", sp);
        body.set ("systemInstruction", sys);
        body.set ("contents", contents);
        body.set ("tools", toolsForGemini());

        auto reply = postJson (url, headers, body);
        if (reply.status < 200 || reply.status >= 300) { out.message = errorText (reply); return out; }

        Json j;
        try { j = Json::parse (reply.body); } catch (...) { out.message = "Unreadable AI response."; return out; }
        const Json& cand = j["candidates"].size() > 0 ? j["candidates"].at (0) : j["candidates"];
        const Json& content = cand["content"];
        if (! content.isObject())
        {
            out.message = "The model returned no content" + (cand["finishReason"].isString() ? juce::String (" (") + cand["finishReason"].str() + ")" : juce::String());
            return out;
        }

        Json responses = Json::array();
        std::string text;
        for (const auto& part : content["parts"].items())
        {
            if (part["text"].isString()) text += part["text"].str();
            if (part["functionCall"].isObject())
            {
                const Json& fc = part["functionCall"];
                Json result = execTool (fc["name"].str(), fc["args"].isObject() ? fc["args"] : Json::object(), lib, out, clipTools);
                Json fr = Json::object(); fr.set ("name", fc["name"]); fr.set ("response", result);
                Json rp = Json::object(); rp.set ("functionResponse", fr);
                responses.push (rp);
            }
        }
        contents.push (content); // echoed verbatim (keeps thought signatures intact)
        if (responses.size() == 0)
        {
            out.ok = out.result.has_value();
            out.message = juce::String::fromUTF8 (text.c_str());
            if (! out.ok && out.message.isEmpty()) out.message = "The model did not generate any MIDI.";
            return out;
        }
        Json ur = Json::object(); ur.set ("role", "user"); ur.set ("parts", responses);
        contents.push (ur);
    }
    out.ok = out.result.has_value();
    if (! out.ok) out.message = "The model kept calling tools without producing MIDI.";
    return out;
}

Outcome runOpenAi (const Settings& s, const juce::String& prompt, const juce::String& ctx,
                   const hhmg::PatternLibrary& lib, const std::function<bool()>& cancel,
                   const ClipTools& clipTools)
{
    using hhmg::Json;
    Outcome out;
    juce::String base = s.endpoint.trim();
    if (base.isEmpty()) { out.message = "Set an endpoint URL (e.g. http://localhost:11434/v1)."; return out; }
    while (base.endsWithChar ('/')) base = base.dropLastCharacters (1);
    const juce::String url = base + "/chat/completions";
    const juce::String headers = s.apiKey.isNotEmpty() ? "Authorization: Bearer " + s.apiKey.trim() : juce::String();

    Json messages = Json::array();
    auto addMsg = [&messages] (const char* role, const std::string& text)
    {
        Json m = Json::object(); m.set ("role", role); m.set ("content", text); messages.push (m);
    };
    addMsg ("system", kSystemPrompt);
    addMsg ("user", (prompt + "\n\nCurrent plugin settings (use as defaults): " + ctx).toStdString());

    for (int turn = 0; turn < 6; ++turn)
    {
        if (cancel()) { out.message = "Cancelled."; return out; }
        Json body = Json::object();
        body.set ("model", s.model.trim().toStdString());
        body.set ("messages", messages);
        body.set ("tools", toolsForOpenAi());

        auto reply = postJson (url, headers, body);
        if (reply.status < 200 || reply.status >= 300) { out.message = errorText (reply); return out; }

        Json j;
        try { j = Json::parse (reply.body); } catch (...) { out.message = "Unreadable AI response."; return out; }
        const Json& msg = j["choices"].size() > 0 ? j["choices"].at (0)["message"] : j["choices"];
        if (! msg.isObject()) { out.message = "The model returned no message."; return out; }
        messages.push (msg);

        if (msg["tool_calls"].size() == 0)
        {
            out.ok = out.result.has_value();
            out.message = juce::String::fromUTF8 (msg["content"].str ("").c_str());
            if (! out.ok && out.message.isEmpty()) out.message = "The model did not generate any MIDI.";
            return out;
        }
        for (const auto& call : msg["tool_calls"].items())
        {
            Json args = Json::object();
            try { args = Json::parse (call["function"]["arguments"].str ("{}")); } catch (...) {}
            Json result = execTool (call["function"]["name"].str(), args, lib, out, clipTools);
            Json m = Json::object();
            m.set ("role", "tool"); m.set ("tool_call_id", call["id"]); m.set ("content", result.dump());
            messages.push (m);
        }
    }
    out.ok = out.result.has_value();
    if (! out.ok) out.message = "The model kept calling tools without producing MIDI.";
    return out;
}
} // namespace

Outcome run (const Settings& settings, const juce::String& prompt, const juce::String& ctx,
             const hhmg::PatternLibrary& lib, const std::function<bool()>& cancel,
             const ClipTools& clipTools)
{
    if (prompt.trim().isEmpty()) return { false, "Type a prompt first.", std::nullopt };
    if (settings.provider == Provider::gemini && settings.apiKey.trim().isEmpty())
        return { false, "Enter your Gemini API key (or set the GEMINI_API_KEY environment variable).", std::nullopt };
    try
    {
        return settings.provider == Provider::gemini ? runGemini (settings, prompt, ctx, lib, cancel, clipTools)
                                                     : runOpenAi (settings, prompt, ctx, lib, cancel, clipTools);
    }
    catch (const std::exception& e)
    {
        return { false, juce::String ("AI error: ") + e.what(), std::nullopt };
    }
}
} // namespace AiBridge
