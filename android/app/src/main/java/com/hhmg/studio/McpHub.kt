package com.hhmg.studio

import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

/** An external MCP server (streamable HTTP). The default one is Hugging Face's official server: no key needed. */
data class McpServer(val name: String, val url: String, val token: String = "", val enabled: Boolean = true)

/**
 * Minimal MCP client. Tools of every enabled server are shown to the AI as `ext_<server>_<tool>`, next to the
 * built-in tools, so the model can search datasets / lyrics / music metadata on those sites and feed the result
 * into analyze / similar / generate.
 */
object McpHub {
    val DEFAULTS = listOf(McpServer("Hugging Face", "https://huggingface.co/mcp"))

    class ExtTool(val exposed: String, val server: McpServer, val real: String, val description: String, val schema: JSONObject)

    private val sessions = HashMap<String, String>()
    private val cache = HashMap<String, Pair<Long, List<ExtTool>>>()

    fun parse(json: String?): List<McpServer> {
        if (json.isNullOrBlank()) return DEFAULTS
        return try {
            val a = JSONArray(json)
            (0 until a.length()).map { val o = a.getJSONObject(it); McpServer(o.getString("name"), o.getString("url"), o.optString("token"), o.optBoolean("enabled", true)) }
        } catch (_: Exception) { DEFAULTS }
    }

    fun serialize(list: List<McpServer>): String =
        JSONArray(list.map { JSONObject().put("name", it.name).put("url", it.url).put("token", it.token).put("enabled", it.enabled) }).toString()

    private fun slug(s: String) = s.lowercase().replace(Regex("[^a-z0-9]+"), "_").trim('_').take(16)

    private fun rpc(server: McpServer, id: Int?, method: String, params: JSONObject?): JSONObject? {
        val body = JSONObject().put("jsonrpc", "2.0").put("method", method)
        if (id != null) body.put("id", id)
        if (params != null) body.put("params", params)
        val conn = URL(server.url).openConnection() as HttpURLConnection
        try {
            conn.requestMethod = "POST"; conn.connectTimeout = 20000; conn.readTimeout = 45000; conn.doOutput = true
            conn.setRequestProperty("Content-Type", "application/json")
            conn.setRequestProperty("Accept", "application/json, text/event-stream")
            if (server.token.isNotBlank()) conn.setRequestProperty("Authorization", "Bearer ${server.token.trim()}")
            sessions[server.url]?.let { conn.setRequestProperty("Mcp-Session-Id", it) }
            conn.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }
            val code = conn.responseCode
            conn.getHeaderField("Mcp-Session-Id")?.let { sessions[server.url] = it }
            if (id == null) return null
            val text = (if (code in 200..299) conn.inputStream else conn.errorStream)?.bufferedReader(Charsets.UTF_8)?.use { it.readText() } ?: ""
            if (code !in 200..299) throw java.io.IOException("HTTP $code ${text.take(120)}")
            val ct = conn.contentType ?: ""
            if (ct.contains("event-stream")) {
                for (line in text.lines()) if (line.startsWith("data:")) {
                    val o = try { JSONObject(line.substring(5).trim()) } catch (_: Exception) { continue }
                    if (o.optInt("id", -1) == id) return o
                }
                throw java.io.IOException("no reply in event stream")
            }
            return JSONObject(text)
        } finally { conn.disconnect() }
    }

    private fun connect(server: McpServer) {
        sessions.remove(server.url)
        rpc(server, 1, "initialize", JSONObject().put("protocolVersion", "2025-03-26").put("capabilities", JSONObject())
            .put("clientInfo", JSONObject().put("name", "hhmg-studio").put("version", "1")))
        rpc(server, null, "notifications/initialized", null)
    }

    /** Tools of the enabled servers (cached for 5 minutes). Servers that fail are skipped; errors are returned in `errors`. */
    fun tools(servers: List<McpServer>, errors: MutableList<String>? = null): List<ExtTool> {
        val all = ArrayList<ExtTool>()
        for (s in servers.filter { it.enabled }) {
            val hit = cache[s.url]
            if (hit != null && System.currentTimeMillis() - hit.first < 300_000) { all += hit.second; continue }
            try {
                connect(s)
                val arr = rpc(s, 2, "tools/list", JSONObject())?.getJSONObject("result")?.getJSONArray("tools") ?: continue
                val list = (0 until arr.length()).map { i ->
                    val t = arr.getJSONObject(i)
                    ExtTool(("ext_" + slug(s.name) + "_" + t.getString("name")).replace(Regex("[^A-Za-z0-9_-]"), "_").take(60), s, t.getString("name"),
                        t.optString("description", ""), t.optJSONObject("inputSchema") ?: JSONObject())
                }
                cache[s.url] = System.currentTimeMillis() to list
                all += list
            } catch (e: Exception) { errors?.add("${s.name}: ${e.message}") }
        }
        return all
    }

    /** Calls a tool; the result is trimmed so it fits a model's context. */
    fun call(tool: ExtTool, args: JSONObject): JSONObject {
        return try {
            var reply = try { rpc(tool.server, 3, "tools/call", JSONObject().put("name", tool.real).put("arguments", args)) }
            catch (e: java.io.IOException) { connect(tool.server); rpc(tool.server, 3, "tools/call", JSONObject().put("name", tool.real).put("arguments", args)) }
            val result = reply?.optJSONObject("result")
            val err = reply?.optJSONObject("error")
            if (err != null) return JSONObject().put("ok", false).put("error", err.optString("message", "error"))
            val sb = StringBuilder()
            val parts = result?.optJSONArray("content") ?: JSONArray()
            for (i in 0 until parts.length()) parts.getJSONObject(i).optString("text").takeIf { it.isNotEmpty() }?.let { sb.append(it).append('\n') }
            JSONObject().put("ok", !(result?.optBoolean("isError", false) ?: false)).put("server", tool.server.name)
                .put("content", sb.toString().take(6000)).put("truncated", sb.length > 6000)
        } catch (e: Exception) { JSONObject().put("ok", false).put("error", "${tool.server.name}: ${e.message}") }
    }

    /** JSON-Schema cut down to what Gemini / OpenAI function declarations accept. */
    fun cleanSchema(o: JSONObject): JSONObject {
        val out = JSONObject()
        var type = o.opt("type")
        if (type is JSONArray) type = (0 until type.length()).map { type.getString(it) }.firstOrNull { it != "null" } ?: "string"
        out.put("type", (type as? String) ?: "object")
        o.optString("description").takeIf { it.isNotEmpty() }?.let { out.put("description", it.take(300)) }
        o.optJSONArray("enum")?.let { if (type == "string") out.put("enum", it) }
        o.optJSONObject("properties")?.let { p ->
            val np = JSONObject(); for (k in p.keys()) np.put(k, cleanSchema(p.getJSONObject(k))); out.put("properties", np)
        }
        o.optJSONArray("required")?.let { out.put("required", it) }
        o.optJSONObject("items")?.let { out.put("items", cleanSchema(it)) }
        if (out.getString("type") == "array" && !out.has("items")) out.put("items", JSONObject().put("type", "string"))
        return out
    }
}
