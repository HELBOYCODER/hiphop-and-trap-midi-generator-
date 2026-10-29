#pragma once
#include <juce_core/juce_core.h>
#include <hhmg/Json.h>
#include <functional>
#include <vector>

// Minimal MCP client (streamable HTTP, JSON or SSE replies). The tools of every enabled server reach the AI as
// ext_<server>_<tool>, so it can search datasets / lyrics / music metadata on those sites and feed the result into
// analyze / similar / generate. The default server is Hugging Face's official one (no key needed).
namespace McpHub
{
struct Server
{
    juce::String name, url, token;
    bool enabled = true;
};

struct ExtTool
{
    std::string exposed, real, description;
    Server server;
    hhmg::Json schema;
};

std::vector<Server> defaults();
std::vector<Server> loadFrom (const juce::XmlElement* settings);   // falls back to defaults()
void saveTo (juce::XmlElement& settings, const std::vector<Server>& servers);

// Blocking. Servers that fail are skipped; their error text is appended to `errors`.
std::vector<ExtTool> listTools (const std::vector<Server>& servers, juce::StringArray* errors = nullptr);
hhmg::Json callTool (const ExtTool& tool, const hhmg::Json& args);
hhmg::Json cleanSchema (const hhmg::Json& schema);
}
