package com.hhmg.studio

import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

/** provider: "gemini" or "openai" (any OpenAI-compatible endpoint: OpenAI, OpenRouter, Ollama, LM Studio, vLLM ...) */
data class AiConfig(val provider: String, val apiKey: String, val model: String, val endpoint: String)

class AiOutcome(val ok: Boolean, val message: String, val clip: JSONObject?)

/** Runs the engine's tools for the model, the same set the VST3 plugin and the MCP server expose. */
class ToolRunner(private val currentClip: () -> JSONObject?, val external: List<McpHub.ExtTool> = emptyList()) {
    var clip: JSONObject? = null; private set

    fun exec(name: String, args: JSONObject): JSONObject = when (name) {
        "generate_midi" -> {
            val r = JSONObject(Native.generateTool(args.toString()))
            r.optJSONObject("clip")?.let { clip = it }
            r.remove("clip"); r
        }
        "analyze_current_clip" -> currentClip()?.let { JSONObject(Native.analyzeClip(it.toString())) }
            ?: JSONObject().put("ok", false).put("error", "no clip yet - ask the user to record or import first")
        "similar_to_current_clip" -> currentClip()?.let { c ->
            val r = JSONObject(Native.similarToClip(c.toString(), args.toString()))
            r.optJSONObject("clip")?.let { clip = it }
            r.remove("clip"); r
        } ?: JSONObject().put("ok", false).put("error", "no clip yet - ask the user to record or import first")
        else -> {
            val ext = external.firstOrNull { it.exposed == name }
            if (ext != null) McpHub.call(ext, args)
            else JSONObject(Native.callTool(name, args.put("output_path", "").toString()))   // never let a model write files
        }
    }
}

object Ai {
    private const val SYSTEM =
        "You are the composition guide inside a voice-to-MIDI app for trap, hip-hop and R&B producers. " +
        "You cannot write notes yourself: you MUST use the tools; the engine writes the notes and keeps every note in the scale. " +
        "The user hums or sings (or imports) a melody; the app transcribes it and its analysis is in the context. " +
        "You can reach the learned reference dataset (real producer loops and the user's own MIDI library) through find_references and through similar_to_current_clip. " +
        "Workflow: (1) analyze_current_clip to recognise the style of what the user made (key, mode, BPM, chord degrees, swing, R&B/Trap/Hip-Hop scores). " +
        "(2) style_guide with the user's words (any language) for matching recipes; find_references for real loops with a similar key/tempo/style. " +
        "When they name an artist or vibe ('Metro Boomin type beat', 'dark drill') call find_type_beat_style for aggregate BPM/key/tag statistics and say how large its sample_size is. " +
        "(3) similar_to_current_clip when they want 'something like this'; generate_midi when they describe a new idea; combine references with reference_progression / reference_rhythm. " +
        "Keep the user's key and tempo unless they ask otherwise; explicit user wishes always win. " +
        "(4) Reply in the user's language in 3-6 short lines: the style you recognised, what you made (key, scale, BPM, chords), which reference inspired it, why, and 1-2 tips. " +
        "You may also have external MCP tools (names start with ext_), e.g. a Hugging Face search that finds MIDI, lyrics and music datasets and shows their licence: use them to recommend sources, always quote the licence, and never suggest using a dataset whose licence forbids it. " +
        "lyrics_to_midi turns lyrics or poetry (any language) into a melody with chords. " +
        "Never pass output_path. Only use ids returned by find_references."

    private fun post(url: String, headers: Map<String, String>, body: JSONObject): Pair<Int, String> {
        val conn = URL(url).openConnection() as HttpURLConnection
        try {
            conn.requestMethod = "POST"; conn.connectTimeout = 30000; conn.readTimeout = 90000; conn.doOutput = true
            conn.setRequestProperty("Content-Type", "application/json")
            headers.forEach { (k, v) -> conn.setRequestProperty(k, v) }
            conn.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }
            val code = conn.responseCode
            val stream = if (code in 200..299) conn.inputStream else conn.errorStream
            return code to (stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() } ?: "")
        } finally { conn.disconnect() }
    }

    private fun retryable(code: Int) = code == 429 || code == 500 || code == 502 || code == 503 || code == 504

    /** Busy / rate-limited / flaky replies are retried with growing pauses (1.5 s, 3 s). */
    private fun postRetry(url: String, headers: Map<String, String>, body: JSONObject, attempts: Int = 3): Pair<Int, String> {
        var last = 0 to ""
        for (i in 0 until attempts) {
            last = try { post(url, headers, body) } catch (e: java.io.IOException) { if (i + 1 == attempts) throw e; 0 to "" }
            if (!retryable(last.first) && last.first != 0 || i + 1 == attempts) break
            Thread.sleep(1500L * (1 shl i))
        }
        return last
    }

    private fun errorText(code: Int, text: String): String {
        val detail = try { JSONObject(text).getJSONObject("error").optString("message") } catch (_: Exception) { "" }
        return "AI service error (HTTP $code)" + if (detail.isNotEmpty()) ": $detail" else ""
    }

    fun run(cfg: AiConfig, prompt: String, context: String, mcp: List<McpServer> = emptyList(), currentClip: () -> JSONObject?): AiOutcome {
        if (prompt.isBlank()) return AiOutcome(false, "Type what you want first.", null)
        if (cfg.provider == "gemini" && cfg.apiKey.isBlank()) return AiOutcome(false, "Enter your Gemini API key first (aistudio.google.com/apikey).", null)
        if (cfg.provider == "openai" && cfg.endpoint.isBlank()) return AiOutcome(false, "Enter the endpoint, e.g. https://api.openai.com/v1 or http://192.168.1.10:11434/v1", null)
        val mcpErrors = ArrayList<String>()
        val runner = ToolRunner(currentClip, McpHub.tools(mcp, mcpErrors))
        val user = "$prompt\n\nWhat the app heard / current settings: $context"
        return try { if (cfg.provider == "gemini") gemini(cfg, user, runner) else openAi(cfg, user, runner) }
        catch (e: Exception) { AiOutcome(false, "No connection to the AI service (${e.message}).", runner.clip) }
    }

    // ---- Gemini function calling -------------------------------------------------------------------------------
    private fun geminiTools(ext: List<McpHub.ExtTool>): JSONArray {
        val decls = JSONArray()
        val schemas = JSONArray(Native.toolSchemas())
        for (i in 0 until schemas.length()) {
            val t = schemas.getJSONObject(i)
            val d = JSONObject().put("name", t.getString("name")).put("description", t.getString("description"))
            val s = t.getJSONObject("inputSchema")
            if (s.getJSONObject("properties").length() > 0) {
                val p = JSONObject().put("type", "object").put("properties", s.getJSONObject("properties"))
                if (s.getJSONArray("required").length() > 0) p.put("required", s.getJSONArray("required"))
                d.put("parameters", p)
            }
            decls.put(d)
        }
        for (e in ext) {   // tools of connected MCP servers
            val d = JSONObject().put("name", e.exposed).put("description", e.description.take(400))
            val sch = McpHub.cleanSchema(e.schema)
            if (sch.optJSONObject("properties")?.length() ?: 0 > 0) d.put("parameters", sch)
            decls.put(d)
        }
        return JSONArray().put(JSONObject().put("functionDeclarations", decls))
    }

    private fun gemini(cfg: AiConfig, user: String, runner: ToolRunner): AiOutcome {
        // when the chosen model is overloaded (503/429), lighter models are tried in turn
        val models = listOf(cfg.model.trim()) + listOf("gemini-2.5-flash", "gemini-2.5-flash-lite", "gemini-2.0-flash").filter { it != cfg.model.trim() }
        var modelIndex = 0
        fun urlFor() = "https://generativelanguage.googleapis.com/v1beta/models/${models[modelIndex]}:generateContent"
        val headers = mapOf("x-goog-api-key" to cfg.apiKey.trim())
        val contents = JSONArray().put(JSONObject().put("role", "user").put("parts", JSONArray().put(JSONObject().put("text", user))))
        for (turn in 0 until 6) {
            val body = JSONObject()
                .put("systemInstruction", JSONObject().put("parts", JSONArray().put(JSONObject().put("text", SYSTEM))))
                .put("contents", contents).put("tools", geminiTools(runner.external))
            var (code, text) = postRetry(urlFor(), headers, body)
            while ((retryable(code) || code == 0) && modelIndex + 1 < models.size) { modelIndex++; val r = postRetry(urlFor(), headers, body, 2); code = r.first; text = r.second }
            if (code !in 200..299) return AiOutcome(false, errorText(code, text) + if (retryable(code)) " — همه‌ی مدل‌ها شلوغ‌اند؛ یک دقیقه بعد دوباره امتحان کنید." else "", runner.clip)
            val content = try { JSONObject(text).getJSONArray("candidates").getJSONObject(0).getJSONObject("content") }
            catch (_: Exception) { return AiOutcome(false, "The model returned no answer.", runner.clip) }
            val parts = content.optJSONArray("parts") ?: JSONArray()
            val responses = JSONArray(); val reply = StringBuilder()
            for (i in 0 until parts.length()) {
                val part = parts.getJSONObject(i)
                if (part.has("text")) reply.append(part.getString("text"))
                val fc = part.optJSONObject("functionCall") ?: continue
                val result = runner.exec(fc.getString("name"), fc.optJSONObject("args") ?: JSONObject())
                responses.put(JSONObject().put("functionResponse", JSONObject().put("name", fc.getString("name")).put("response", result)))
            }
            contents.put(content)   // echoed verbatim (keeps thought signatures)
            if (responses.length() == 0) {
                val c = runner.clip
                val note = if (modelIndex > 0) "\n(پاسخ از ${models[modelIndex]} آمد چون ${models[0]} شلوغ بود)" else ""
                return if (c != null) AiOutcome(true, reply.toString().trim() + note, c) else AiOutcome(false, reply.toString().trim().ifEmpty { "The model did not generate any MIDI." }, null)
            }
            contents.put(JSONObject().put("role", "user").put("parts", responses))
        }
        return AiOutcome(runner.clip != null, if (runner.clip != null) "Done." else "The model kept calling tools without producing MIDI.", runner.clip)
    }

    // ---- OpenAI-compatible chat completions with tools ---------------------------------------------------------------
    private fun openAiTools(ext: List<McpHub.ExtTool>): JSONArray {
        val arr = JSONArray()
        val schemas = JSONArray(Native.toolSchemas())
        for (i in 0 until schemas.length()) {
            val t = schemas.getJSONObject(i)
            arr.put(JSONObject().put("type", "function").put("function",
                JSONObject().put("name", t.getString("name")).put("description", t.getString("description")).put("parameters", t.getJSONObject("inputSchema"))))
        }
        for (e in ext) arr.put(JSONObject().put("type", "function").put("function",
            JSONObject().put("name", e.exposed).put("description", e.description.take(400)).put("parameters", McpHub.cleanSchema(e.schema))))
        return arr
    }

    private fun openAi(cfg: AiConfig, user: String, runner: ToolRunner): AiOutcome {
        val base = cfg.endpoint.trim().trimEnd('/')
        val headers = if (cfg.apiKey.isNotBlank()) mapOf("Authorization" to "Bearer ${cfg.apiKey.trim()}") else emptyMap()
        val messages = JSONArray()
            .put(JSONObject().put("role", "system").put("content", SYSTEM))
            .put(JSONObject().put("role", "user").put("content", user))
        for (turn in 0 until 6) {
            val body = JSONObject().put("model", cfg.model.trim()).put("messages", messages).put("tools", openAiTools(runner.external))
            val (code, text) = postRetry("$base/chat/completions", headers, body)
            if (code !in 200..299) return AiOutcome(false, errorText(code, text), runner.clip)
            val msg = try { JSONObject(text).getJSONArray("choices").getJSONObject(0).getJSONObject("message") }
            catch (_: Exception) { return AiOutcome(false, "The model returned no message.", runner.clip) }
            messages.put(msg)
            val calls = msg.optJSONArray("tool_calls")
            if (calls == null || calls.length() == 0) {
                val c = runner.clip
                val reply = msg.optString("content", "").trim()
                return if (c != null) AiOutcome(true, reply, c) else AiOutcome(false, reply.ifEmpty { "The model did not generate any MIDI." }, null)
            }
            for (i in 0 until calls.length()) {
                val call = calls.getJSONObject(i)
                val fn = call.getJSONObject("function")
                val args = try { JSONObject(fn.optString("arguments", "{}")) } catch (_: Exception) { JSONObject() }
                val result = runner.exec(fn.getString("name"), args)
                messages.put(JSONObject().put("role", "tool").put("tool_call_id", call.getString("id")).put("content", result.toString()))
            }
        }
        return AiOutcome(runner.clip != null, if (runner.clip != null) "Done." else "The model kept calling tools without producing MIDI.", runner.clip)
    }
}
