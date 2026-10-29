// hhmg-cli: command line + stdio MCP server around the generator core.
//   hhmg-cli tools                       print tool schemas
//   hhmg-cli call <tool> [--patterns f]  read JSON args on stdin, print JSON result
//   hhmg-cli mcp [--patterns f] [--allow-index]   Model Context Protocol server over stdio
//   hhmg-cli index <dir> [--out f.json]  index your own MIDI folder into reference profiles (no notes kept)
//   hhmg-cli voice <file.wav> [--mode melody|melody_plus|similar] [--style s] [--seed n] [--out f.mid]
#include "hhmg/Library.h"
#include "hhmg/Tools.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace hhmg;

static void mergeFile(PatternLibrary& lib, const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "cannot open patterns file " << path << "\n"; return; }
    std::stringstream ss; ss << f.rdbuf();
    try { lib.mergeJson(Json::parse(ss.str())); }
    catch (const std::exception& e) { std::cerr << "patterns file ignored: " << e.what() << "\n"; }
}

// built-in seed library + HHMG_PATTERNS (':' or ';' separated files) + every --patterns / --user-library file
static PatternLibrary loadLibrary(int argc, char** argv) {
    PatternLibrary lib = PatternLibrary::builtin();
    if (const char* env = std::getenv("HHMG_PATTERNS")) {
        std::string e = env, cur;
        for (char c : e + ":") { if (c == ':' || c == ';') { if (!cur.empty()) mergeFile(lib, cur); cur.clear(); } else cur += c; }
    }
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--patterns" || std::string(argv[i]) == "--user-library") mergeFile(lib, argv[i + 1]);
    return lib;
}

static bool hasFlag(int argc, char** argv, const char* f) { for (int i = 1; i < argc; ++i) if (std::string(f) == argv[i]) return true; return false; }
static std::string optValue(int argc, char** argv, const char* f, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; ++i) if (std::string(f) == argv[i]) return argv[i + 1];
    return def;
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

static int serveMcp(PatternLibrary& lib, bool allowIndex) {
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
            Json tools = toolSchemas();
            if (allowIndex) {   // opt-in: lets the agent read a folder of your MIDI files (profiles only, notes are not kept)
                Json t = Json::object();
                t.set("name", "index_midi_folder");
                t.set("description", "Index every .mid file under a folder into reference profiles (key, chord degrees, rhythm; no notes) so find_references and similar mode can use them.");
                Json p = Json::object(); Json path = Json::object(); path.set("type", "string"); path.set("description", "Folder path"); p.set("path", path);
                Json sc = Json::object(); sc.set("type", "object"); sc.set("properties", p); Json rq = Json::array(); rq.push("path"); sc.set("required", rq);
                t.set("inputSchema", sc);
                tools.push(t);
            }
            Json res = Json::object(); res.set("tools", tools);
            reply = rpcResult(id, res);
        } else if (method == "tools/call") {
            Json out;
            if (allowIndex && req["params"]["name"].str("") == "index_midi_folder") {
                std::string e;
                int n = indexMidiFolder(req["params"]["arguments"]["path"].str(""), lib, nullptr, 5000, e);
                out = Json::object();
                if (!e.empty() && n == 0) { out.set("ok", false); out.set("error", e); }
                else { out.set("ok", true); out.set("indexed_files", n); out.set("total_references", (int) lib.references.size()); }
            } else {
                out = callTool(req["params"]["name"].str(""), req["params"]["arguments"], lib);
            }
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
        std::cerr << "usage: hhmg-cli tools | call <tool> | mcp [--allow-index] | index <dir> | voice <wav>   [--patterns f.json]\n";
        return 2;
    }
    std::string cmd = argv[1];
    PatternLibrary lib = loadLibrary(argc, argv);
    if (cmd == "tools") { std::cout << toolSchemas().dump(true) << "\n"; return 0; }
    if (cmd == "mcp") return serveMcp(lib, hasFlag(argc, argv, "--allow-index"));
    if (cmd == "index" && argc >= 3) {
        PatternLibrary delta; std::string e;
        int n = indexMidiFolder(argv[2], lib, &delta, 5000, e);
        std::string outPath = optValue(argc, argv, "--out", "user_library.json");
        std::ofstream(outPath) << delta.toJson().dump();
        std::cout << "indexed " << n << " files -> " << outPath << (e.empty() ? "" : "  (" + e + ")") << "\n";
        return n > 0 ? 0 : 1;
    }
    if (cmd == "voice" && argc >= 3) {
        Json args = Json::object();
        args.set("path", argv[2]);
        args.set("mode", optValue(argc, argv, "--mode", "melody"));
        if (!optValue(argc, argv, "--style").empty()) args.set("style", optValue(argc, argv, "--style"));
        if (!optValue(argc, argv, "--seed").empty()) args.set("seed", std::atoi(optValue(argc, argv, "--seed").c_str()));
        if (!optValue(argc, argv, "--bpm").empty()) args.set("bpm", std::atof(optValue(argc, argv, "--bpm").c_str()));
        args.set("output_path", optValue(argc, argv, "--out", "voice.mid"));
        Json out = callTool("voice_to_midi", args, lib);
        std::cout << out.dump(true) << "\n";
        return out["ok"].boolean(false) ? 0 : 1;
    }
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
