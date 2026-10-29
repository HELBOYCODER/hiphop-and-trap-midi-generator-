package com.hhmg.studio

import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

/**
 * Text prompt (+ what the app heard) -> MIDI through Gemini function calling.
 * The model never writes notes: it calls the engine's tools (style_guide, analyze_current_clip,
 * find_references, generate_midi ...), so every note stays in the scale.
 */
class GeminiClient {
    class Outcome(val ok: Boolean, val message: String, val clip: JSONObject?)

    private val system =
        "You are the composition guide inside a voice-to-MIDI app for trap, hip-hop and R&B producers. " +
        "You cannot write notes yourself: you MUST use the tools; the engine writes the notes and keeps every note in the scale. " +
        "The user hums or sings a melody; the app transcribes it and shows its analysis in the context. " +
        "Workflow: (1) call analyze_current_clip to recognise the style of what the user made (key, mode, BPM, chord degrees, swing, R&B/Trap/Hip-Hop scores). " +
        "(2) call style_guide with the user's words (any language) for matching recipes, and find_references for real loops with a similar key/tempo/style. " +
        "(3) call generate_midi once: keep the user's key and tempo unless they ask otherwise, take settings from the analysis or recipe, " +
        "and combine references with reference_progression / reference_rhythm. Explicit user wishes always win. " +
        "(4) Reply in the user's language in 3-6 short lines: the style you recognised, what you made (key, scale, BPM, chords), why, and 1-2 tips. " +
        "Never pass output_path. Only use ids returned by find_references."

    private fun post(apiKey: String, model: String, body: JSONObject): Pair<Int, String> {
        val conn = URL("https://generativelanguage.googleapis.com/v1beta/models/${model.trim()}:generateContent").openConnection() as HttpURLConnection
        try {
            conn.requestMethod = "POST"
            conn.connectTimeout = 30000; conn.readTimeout = 60000
            conn.doOutput = true
            conn.setRequestProperty("Content-Type", "application/json")
            conn.setRequestProperty("x-goog-api-key", apiKey.trim())
            conn.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }
            val code = conn.responseCode
            val stream = if (code in 200..299) conn.inputStream else conn.errorStream
            return code to (stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() } ?: "")
        } finally { conn.disconnect() }
    }

    private fun tools(): JSONArray {
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
        return JSONArray().put(JSONObject().put("functionDeclarations", decls))
    }

    fun run(apiKey: String, model: String, prompt: String, context: String, currentClip: () -> JSONObject?): Outcome {
        if (apiKey.isBlank()) return Outcome(false, "Enter your Gemini API key first (aistudio.google.com/apikey).", null)
        if (prompt.isBlank()) return Outcome(false, "Type what you want first.", null)
        var clip: JSONObject? = null
        val contents = JSONArray().put(JSONObject().put("role", "user").put("parts",
            JSONArray().put(JSONObject().put("text", "$prompt\n\nWhat the app heard / current settings: $context"))))
        for (turn in 0 until 6) {
            val body = JSONObject()
                .put("systemInstruction", JSONObject().put("parts", JSONArray().put(JSONObject().put("text", system))))
                .put("contents", contents)
                .put("tools", tools())
            val (code, text) = try { post(apiKey, model, body) } catch (e: Exception) { return Outcome(false, "No connection to the AI service (${e.message}).", clip) }
            if (code !in 200..299) {
                val detail = try { JSONObject(text).getJSONObject("error").optString("message") } catch (_: Exception) { "" }
                return Outcome(false, "AI service error (HTTP $code)" + if (detail.isNotEmpty()) ": $detail" else "", clip)
            }
            val content = try { JSONObject(text).getJSONArray("candidates").getJSONObject(0).getJSONObject("content") }
            catch (_: Exception) { return Outcome(false, "The model returned no answer.", clip) }
            val parts = content.optJSONArray("parts") ?: JSONArray()
            val responses = JSONArray()
            val reply = StringBuilder()
            for (i in 0 until parts.length()) {
                val part = parts.getJSONObject(i)
                if (part.has("text")) reply.append(part.getString("text"))
                val fc = part.optJSONObject("functionCall") ?: continue
                val name = fc.getString("name")
                val args = fc.optJSONObject("args") ?: JSONObject()
                val result: JSONObject = when (name) {
                    "generate_midi" -> {
                        val r = JSONObject(Native.generateTool(args.toString()))
                        r.optJSONObject("clip")?.let { clip = it }
                        r.remove("clip"); r
                    }
                    "analyze_current_clip" -> {
                        val c = currentClip()
                        if (c == null) JSONObject().put("ok", false).put("error", "no clip yet - ask the user to record first")
                        else JSONObject(Native.analyzeClip(c.toString()))
                    }
                    else -> JSONObject(Native.callTool(name, args.toString()))
                }
                responses.put(JSONObject().put("functionResponse", JSONObject().put("name", name).put("response", result)))
            }
            contents.put(content)   // echoed verbatim (keeps thought signatures)
            if (responses.length() == 0) {
                return if (clip != null) Outcome(true, reply.toString().trim(), clip)
                else Outcome(false, reply.toString().trim().ifEmpty { "The model did not generate any MIDI." }, null)
            }
            contents.put(JSONObject().put("role", "user").put("parts", responses))
        }
        return Outcome(clip != null, if (clip != null) "Done." else "The model kept calling tools without producing MIDI.", clip)
    }
}
