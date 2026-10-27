// hhmg-cli: command line + stdio MCP server around the generator core.
//   hhmg-cli tools                       print tool schemas
//   hhmg-cli call <tool> [--patterns f]  read JSON args on stdin, print JSON result
//   hhmg-cli mcp [--patterns f]          Model Context Protocol server over stdio
#include "hhmg/Tools.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace hhmg;

static PatternLibrary loadLibrary(int argc, char** argv) {
    PatternLibrary lib = PatternLibrary::builtin();
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) != "--patterns") continue;
        std::ifstream f(argv[i + 1]);
        if (!f) { std::cerr << "cannot open patterns file " << argv[i + 1] << "\n"; continue; }
        std::stringstream ss; ss << f.rdbuf();
        try { lib.mergeJson(Json::parse(ss.str())); }
        catch (const std::exception& e) { std::cerr << "patterns file ignored: " << e.what() << "\n"; }
    }
    return lib;
}

static Json rpcResult(const Json& id, Json result) {
    Json r = Json::object();
    r.set("jsonrpc", "2.0"); r.set("id", id); r.set("result", std::move(result));
    return r;
}
static Json rpcError(const Json& id, int code, const std::string& msg) {
    Json e = Json::object(); e.set("code", code); e.set("message", msg);
    Json r = Json::object(); r.set("jsonrpc", "2.0"); r.set("id", id); r.set("error", e);
    return r;
}

static int serveMcp(const PatternLibrary& lib) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        Json req, reply;
        try { req = Json::parse(line); }
        catch (const std::exception&) {
            std::cout << rpcError(Json(), -32700, "parse error").dump() << "\n" << std::flush;
            continue;
        }
        const std::string method = req["method"].str("");
        const Json& id = req["id"];
        bool isNotification = !req.has("id");
        if (method == "initialize") {
            Json res = Json::object();
            res.set("protocolVersion", req["params"]["protocolVersion"].str("2025-03-26"));
            Json caps = Json::object(); caps.set("tools", Json::object());
            res.set("capabilities", caps);
            Json info = Json::object(); info.set("name", "hhmg-midi-generator"); info.set("version", "1.0.0");
            res.set("serverInfo", info);
            res.set("instructions", "Generate R&B / trap / hip-hop chords, melodies and 808 lines that are guaranteed to stay in the chosen scale. Call generate_midi with output_path to write a .mid file.");
            reply = rpcResult(id, res);
        } else if (method == "ping") {
            reply = rpcResult(id, Json::object());
        } else if (method == "tools/list") {
            Json res = Json::object(); res.set("tools", toolSchemas());
            reply = rpcResult(id, res);
        } else if (method == "tools/call") {
            Json out = callTool(req["params"]["name"].str(""), req["params"]["arguments"], lib);
            Json content = Json::array();
            Json part = Json::object(); part.set("type", "text"); part.set("text", out.dump(true));
            content.push(part);
            Json res = Json::object();
            res.set("content", content);
            res.set("isError", !out["ok"].boolean(false));
            reply = rpcResult(id, res);
        } else if (isNotification) {
            continue; // notifications/initialized etc.
        } else {
            reply = rpcError(id, -32601, "method not found: " + method);
        }
        if (!isNotification) std::cout << reply.dump() << "\n" << std::flush;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: hhmg-cli tools | call <tool> | mcp  [--patterns patterns.json]\n";
        return 2;
    }
    std::string cmd = argv[1];
    PatternLibrary lib = loadLibrary(argc, argv);
    if (cmd == "tools") { std::cout << toolSchemas().dump(true) << "\n"; return 0; }
    if (cmd == "mcp") return serveMcp(lib);
    if (cmd == "call" && argc >= 3) {
        std::stringstream ss; ss << std::cin.rdbuf();
        Json args;
        try { args = ss.str().find_first_not_of(" \t\r\n") == std::string::npos ? Json::object() : Json::parse(ss.str()); }
        catch (const std::exception& e) { std::cerr << "bad JSON: " << e.what() << "\n"; return 2; }
        Json out = callTool(argv[2], args, lib);
        std::cout << out.dump(true) << "\n";
        return out["ok"].boolean(false) ? 0 : 1;
    }
    std::cerr << "unknown command\n";
    return 2;
}
