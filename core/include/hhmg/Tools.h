// Tool layer shared by the plugin's AI bridge (Gemini function calling) and the stdio MCP server.
// Every tool takes/returns JSON, so the model never touches plugin internals directly.
#pragma once
#include "Generator.h"
#include "Json.h"
#include "MidiFile.h"

namespace hhmg {

Request requestFromJson(const Json& args); // tolerant: names or numbers, clamps ranges
Json resultToJson(const Result& r, bool includeNotes);

// [{name, description, inputSchema}] - JSON-Schema, usable verbatim as MCP tools/list payload.
Json toolSchemas();
// Returns {"ok":true,...} or {"ok":false,"error":"..."}; never throws.
Json callTool(const std::string& name, const Json& args, const PatternLibrary& lib);

} // namespace hhmg
