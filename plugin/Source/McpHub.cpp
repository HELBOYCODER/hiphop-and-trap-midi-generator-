#include "McpHub.h"
#include <map>
#include <mutex>

namespace McpHub
{
namespace
{
std::mutex gLock;
std::map<std::string, std::string> gSessions;                        // url -> Mcp-Session-Id
std::map<std::string, std::pair<juce::int64, std::vector<ExtTool>>> gCache;

juce::String slug (const juce::String& s)
{
    juce::String out;
    for (auto c : s.toLowerCase()) out += juce::CharacterFunctions::isLetterOrDigit (c) && c < 128 ? juce::String::charToString (c) : "_";
    return out.substring (0, 16);
}

// Returns the JSON-RPC reply for `id`; id < 0 sends a notification and returns null.
hhmg::Json rpc (const Server& s, int id, const char* method, const hhmg::Json& params)
{
    using hhmg::Json;
    Json body = Json::object();
    body.set ("jsonrpc", "2.0");
    body.set ("method", method);
    if (id >= 0) body.set ("id", id);
    body.set ("params", params);

    juce::String headers = "Content-Type: application/json\r\nAccept: application/json, text/event-stream\r\n";
    if (s.token.isNotEmpty()) headers << "Authorization: Bearer " << s.token.trim() << "\r\n";
    { std::lock_guard<std::mutex> g (gLock); auto it = gSessions.find (s.url.toStdString()); if (it != gSessions.end()) headers << "Mcp-Session-Id: " << juce::String (it->second) << "\r\n"; }

    int status = 0;
    juce::StringPairArray responseHeaders;
    juce::URL u = juce::URL (s.url).withPOSTData (juce::String (body.dump()));
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                       .withConnectionTimeoutMs (30000)
                       .withExtraHeaders (headers)
                       .withHttpRequestCmd ("POST")
                       .withNumRedirectsToFollow (3)
                       .withStatusCode (&status)
                       .withResponseHeaders (&responseHeaders);
    std::string text;
    if (auto stream = u.createInputStream (options)) text = stream->readEntireStreamAsString().toStdString();
    if (status == 0) throw std::runtime_error ("cannot reach " + s.url.toStdString());
    const auto sid = responseHeaders.getValue ("Mcp-Session-Id", responseHeaders.getValue ("mcp-session-id", ""));
    if (sid.isNotEmpty()) { std::lock_guard<std::mutex> g (gLock); gSessions[s.url.toStdString()] = sid.toStdString(); }
    if (id < 0) return Json();
    if (status < 200 || status >= 300) throw std::runtime_error ("HTTP " + std::to_string (status) + " " + text.substr (0, 120));

    if (responseHeaders.getValue ("Content-Type", responseHeaders.getValue ("content-type", "")).containsIgnoreCase ("event-stream"))
    {
        for (auto line : juce::StringArray::fromLines (juce::String (text)))
        {
            if (! line.startsWith ("data:")) continue;
            try
            {
                Json o = Json::parse (line.substring (5).trim().toStdString());
                if (o["id"].isNumber() && o["id"].integer() == id) return o;
            }
            catch (...) {}
        }
        throw std::runtime_error ("no reply in event stream");
    }
    return Json::parse (text);
}

void connect (const Server& s)
{
    { std::lock_guard<std::mutex> g (gLock); gSessions.erase (s.url.toStdString()); }
    using hhmg::Json;
    Json p = Json::object();
    p.set ("protocolVersion", "2025-03-26");
    p.set ("capabilities", Json::object());
    Json ci = Json::object(); ci.set ("name", "hhmg-plugin"); ci.set ("version", "1");
    p.set ("clientInfo", ci);
    rpc (s, 1, "initialize", p);
    rpc (s, -1, "notifications/initialized", Json::object());
}
} // namespace

std::vector<Server> defaults() { return { { "Hugging Face", "https://huggingface.co/mcp", {}, true } }; }

std::vector<Server> loadFrom (const juce::XmlElement* x)
{
    if (x == nullptr || x->getChildByName ("Mcp") == nullptr) return defaults();
    std::vector<Server> out;
    for (auto* e : x->getChildWithTagNameIterator ("Mcp"))
        if (e->getStringAttribute ("url").isNotEmpty())
            out.push_back ({ e->getStringAttribute ("name"), e->getStringAttribute ("url"), e->getStringAttribute ("token"), e->getBoolAttribute ("enabled", true) });
    return out;
}

void saveTo (juce::XmlElement& x, const std::vector<Server>& servers)
{
    for (auto& s : servers)
    {
        auto* e = x.createNewChildElement ("Mcp");
        e->setAttribute ("name", s.name); e->setAttribute ("url", s.url); e->setAttribute ("token", s.token); e->setAttribute ("enabled", s.enabled);
    }
    if (servers.empty()) x.createNewChildElement ("Mcp")->setAttribute ("url", "");   // marker: user removed all servers
}

std::vector<ExtTool> listTools (const std::vector<Server>& servers, juce::StringArray* errors)
{
    using hhmg::Json;
    std::vector<ExtTool> all;
    for (auto& s : servers)
    {
        if (! s.enabled || s.url.isEmpty()) continue;
        {
            std::lock_guard<std::mutex> g (gLock);
            auto it = gCache.find (s.url.toStdString());
            if (it != gCache.end() && juce::Time::currentTimeMillis() - it->second.first < 300000)
            {
                all.insert (all.end(), it->second.second.begin(), it->second.second.end());
                continue;
            }
        }
        try
        {
            connect (s);
            Json r = rpc (s, 2, "tools/list", Json::object());
            std::vector<ExtTool> list;
            for (const auto& t : r["result"]["tools"].items())
            {
                ExtTool e;
                e.real = t["name"].str();
                juce::String ex = "ext_" + slug (s.name) + "_" + juce::String (e.real);
                e.exposed = ex.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-").substring (0, 60).toStdString();
                e.description = "[" + s.name.toStdString() + "] " + t["description"].str ("");
                e.server = s;
                e.schema = t["inputSchema"];
                list.push_back (std::move (e));
            }
            { std::lock_guard<std::mutex> g (gLock); gCache[s.url.toStdString()] = { juce::Time::currentTimeMillis(), list }; }
            all.insert (all.end(), list.begin(), list.end());
        }
        catch (const std::exception& e) { if (errors) errors->add (s.name + ": " + e.what()); }
    }
    return all;
}

hhmg::Json callTool (const ExtTool& tool, const hhmg::Json& args)
{
    using hhmg::Json;
    Json out = Json::object();
    try
    {
        Json p = Json::object(); p.set ("name", tool.real); p.set ("arguments", args);
        Json reply;
        try { reply = rpc (tool.server, 3, "tools/call", p); }
        catch (const std::exception&) { connect (tool.server); reply = rpc (tool.server, 3, "tools/call", p); }
        if (reply["error"].isObject()) { out.set ("ok", false); out.set ("error", reply["error"]["message"].str ("error")); return out; }
        std::string text;
        for (const auto& c : reply["result"]["content"].items()) if (c["text"].isString()) text += c["text"].str() + "\n";
        const bool truncated = text.size() > 6000;
        out.set ("ok", ! reply["result"]["isError"].boolean (false));
        out.set ("server", tool.server.name.toStdString());
        out.set ("content", text.substr (0, 6000));
        out.set ("truncated", truncated);
    }
    catch (const std::exception& e) { out.set ("ok", false); out.set ("error", tool.server.name.toStdString() + ": " + e.what()); }
    return out;
}

hhmg::Json cleanSchema (const hhmg::Json& o)
{
    using hhmg::Json;
    Json out = Json::object();
    std::string type = "object";
    if (o["type"].isString()) type = o["type"].str();
    else if (o["type"].isArray()) { type = "string"; for (const auto& t : o["type"].items()) if (t.str() != "null") { type = t.str(); break; } }
    out.set ("type", type);
    if (o["description"].isString()) out.set ("description", o["description"].str().substr (0, 300));
    if (type == "string" && o["enum"].isArray()) out.set ("enum", o["enum"]);
    if (o["properties"].isObject())
    {
        Json np = Json::object();
        for (size_t i = 0; i < o["properties"].keys().size(); ++i) np.set (o["properties"].keys()[i], cleanSchema (o["properties"].valueAt (i)));
        out.set ("properties", np);
    }
    if (o["required"].isArray()) out.set ("required", o["required"]);
    if (o["items"].isObject()) out.set ("items", cleanSchema (o["items"]));
    else if (type == "array") { Json it = Json::object(); it.set ("type", "string"); out.set ("items", it); }
    return out;
}
} // namespace McpHub
