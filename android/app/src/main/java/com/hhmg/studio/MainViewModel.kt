package com.hhmg.studio

import android.app.Application
import android.content.Context
import android.media.AudioTrack
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject

enum class Mode(val key: String, val label: String) {
    MELODY("melody", "ملودی خودم"),
    PLUS("melody_plus", "ملودی من + آکورد و ۸۰۸"),
    SIMILAR("similar", "ایده‌ی مشابه (جدید)"),
}

class MainViewModel(app: Application) : AndroidViewModel(app) {
    private val prefs = app.getSharedPreferences("hhmg", Context.MODE_PRIVATE)
    private val recorder = Recorder { level = it }
    private var track: AudioTrack? = null
    private var lastSamples: FloatArray? = null
    private var lastRate = 16000

    var recording by mutableStateOf(false); private set
    var recordSeconds by mutableStateOf(0.0); private set
    var level by mutableStateOf(0f); private set
    var busy by mutableStateOf(false); private set
    var status by mutableStateOf("برای شروع، ضبط را بزنید و ملودی را زمزمه یا بخوانید."); private set

    var transcription by mutableStateOf<Transcription?>(null); private set
    var clip by mutableStateOf<Clip?>(null); private set
    var analysisText by mutableStateOf(""); private set
    private var analysis: JSONObject? = null

    // ---- generator (same controls as the VST3: key/scale, style mix, complexity, humanize, swing, parts, seed, presets) ----
    var tab by mutableStateOf(0)                       // 0 voice, 1 generate, 2 lyrics
    var gRoot by mutableStateOf(9); var gScale by mutableStateOf("minor"); var gBars by mutableStateOf(4); var gBpm by mutableStateOf(140)
    var gRnb by mutableStateOf(0.15f); var gTrap by mutableStateOf(1f); var gHip by mutableStateOf(0.3f)
    var gComplexity by mutableStateOf(0.5f); var gHumanize by mutableStateOf(0.3f); var gSwing by mutableStateOf(0f); var gVelVar by mutableStateOf(0.3f)
    var gChords by mutableStateOf(true); var gMelody by mutableStateOf(true); var gBass by mutableStateOf(true); var gOctave by mutableStateOf(5)
    var gSeed by mutableStateOf(1)
    var scales by mutableStateOf<List<Pair<String, String>>>(emptyList())
    var lyricsText by mutableStateOf("")
    var clipFromVoice by mutableStateOf(false); private set

    var mode by mutableStateOf(Mode.MELODY)
    var style by mutableStateOf("")          // "" = detected automatically
    var bass by mutableStateOf(true)
    var bpmText by mutableStateOf("")        // empty = estimated from the singing
    var seed by mutableStateOf(1)

    var playing by mutableStateOf(false); private set
    var playTicks by mutableStateOf(0.0); private set

    var provider by mutableStateOf(prefs.getString("provider", "gemini") ?: "gemini")      // "gemini" | "openai"
    var apiKey by mutableStateOf(prefs.getString("apiKey", "") ?: "")
    var model by mutableStateOf(prefs.getString("model", "gemini-2.5-flash") ?: "gemini-2.5-flash")
    var endpoint by mutableStateOf(prefs.getString("endpoint", "https://api.openai.com/v1") ?: "https://api.openai.com/v1")
    var aiBusy by mutableStateOf(false); private set
    var aiMessage by mutableStateOf(""); private set
    var libraryInfo by mutableStateOf(""); private set
    var mcpServers by mutableStateOf(McpHub.parse(prefs.getString("mcp", null)))
    var mcpStatus by mutableStateOf("")

    init {
        viewModelScope.launch(Dispatchers.IO) {
            val am = getApplication<Application>().assets
            for (f in listOf("patterns.pop909.json", "patterns.cymatics.json", "typebeat_styles.json")) {
                try { Native.loadPatterns(am.open(f).bufferedReader().use { it.readText() }) } catch (_: Exception) {}
            }
            try { java.io.File(getApplication<Application>().filesDir, "user_library.json").takeIf { it.exists() }?.readText()?.let { Native.loadUserLibrary(it) } } catch (_: Exception) {}
            libraryInfo = Native.libraryInfo()
            try {
                val arr = JSONObject(Native.callTool("list_scales", "{}")).getJSONArray("scales")
                scales = (0 until arr.length()).map { arr.getJSONObject(it).let { o -> o.getString("id") to o.getString("label") } }
            } catch (_: Exception) {}
        }
    }

    private val noteNames = arrayOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
    fun noteLabel(i: Int) = noteNames[((i % 12) + 12) % 12]

    private fun genArgs(): JSONObject = JSONObject().put("root", gRoot).put("scale", gScale).put("bpm", gBpm).put("bars", gBars)
        .put("style_mix", JSONObject().put("rnb", gRnb.toDouble()).put("trap", gTrap.toDouble()).put("hiphop", gHip.toDouble()))
        .put("complexity", gComplexity.toDouble()).put("humanize", gHumanize.toDouble()).put("swing", gSwing.toDouble())
        .put("velocity_variation", gVelVar.toDouble()).put("chords", gChords).put("melody", gMelody).put("bass", gBass)
        .put("melody_octave", gOctave).put("seed", gSeed)

    /** Generate without any recording (the engine of the VST3). */
    fun generate(newSeed: Boolean = true) {
        stopPlayback()
        if (newSeed) gSeed = (1..99999).random()
        busy = true; status = "در حال ساخت..."
        val args = genArgs()
        viewModelScope.launch(Dispatchers.Default) {
            val r = JSONObject(Native.generateTool(args.toString()))
            withContext(Dispatchers.Main) {
                busy = false
                if (!r.optBoolean("ok")) { status = r.optString("error", "خطا"); return@withContext }
                clip = Clip(r.getJSONObject("clip")); clipFromVoice = false
                val ch = r.optJSONArray("chords")
                analysisText = "کلید: ${r.optString("scale")}  |  BPM: ${clip!!.bpm}" + (ch?.let { "\nآکوردها: " + (0 until it.length()).joinToString(" | ") { i -> it.getString(i) } } ?: "")
                status = "آماده: ${clip!!.description}"
            }
        }
    }

    fun applyPreset(i: Int) {
        data class P(val scale: String, val bpm: Int, val rnb: Float, val trap: Float, val hip: Float, val cx: Float, val hum: Float, val swing: Float, val bass: Boolean, val bars: Int)
        val p = listOf(
            P("minor", 140, .1f, 1f, .2f, .35f, .15f, 0f, true, 4), P("dorian", 78, 1f, 0f, .1f, .85f, .5f, .25f, false, 8),
            P("pentatonic_minor", 90, .1f, 0f, 1f, .4f, .6f, .5f, true, 4), P("phrygian", 132, .6f, .7f, 0f, .6f, .35f, .1f, true, 8),
            P("major", 86, 1f, 0f, .3f, 1f, .7f, .35f, false, 4))[i]
        gScale = p.scale; gBpm = p.bpm; gRnb = p.rnb; gTrap = p.trap; gHip = p.hip; gComplexity = p.cx; gHumanize = p.hum; gSwing = p.swing; gBass = p.bass; gBars = p.bars
        generate(true)
    }

    /** Lyrics / poetry (any language) -> melody + chords. */
    fun makeFromLyrics() {
        if (lyricsText.isBlank()) { status = "اول شعر را بنویسید."; return }
        stopPlayback(); busy = true; status = "در حال ساخت ملودی از شعر..."
        val args = genArgs().put("lyrics", lyricsText).put("include_notes", true)
        args.put("style", if (gTrap >= gRnb && gTrap >= gHip) "trap" else if (gHip >= gRnb) "hiphop" else "rnb")
        viewModelScope.launch(Dispatchers.Default) {
            val r = JSONObject(Native.callTool("lyrics_to_midi", args.toString()))
            withContext(Dispatchers.Main) {
                busy = false
                if (!r.optBoolean("ok")) { status = r.optString("error", "خطا"); return@withContext }
                clip = Clip(r.getJSONObject("clip")); clipFromVoice = false
                val ch = r.optJSONArray("chords")
                analysisText = "خطوط: ${r.optJSONArray("lines")?.length() ?: 0}  |  BPM: ${clip!!.bpm}" + (ch?.let { "\nآکوردها: " + (0 until it.length()).joinToString(" | ") { i -> it.getString(i) } } ?: "")
                status = "آماده: ${clip!!.description}"
            }
        }
    }

    fun setMcp(list: List<McpServer>) { mcpServers = list; prefs.edit().putString("mcp", McpHub.serialize(list)).apply() }
    fun testMcp() {
        mcpStatus = "در حال اتصال..."
        viewModelScope.launch(Dispatchers.IO) {
            val errors = ArrayList<String>()
            val tools = McpHub.tools(mcpServers, errors)
            withContext(Dispatchers.Main) { mcpStatus = if (errors.isEmpty()) "متصل: ${tools.size} ابزار از ${mcpServers.count { it.enabled }} سرور" else errors.joinToString("\n") }
        }
    }

    fun saveAiSettings() { prefs.edit().putString("provider", provider).putString("apiKey", apiKey).putString("model", model).putString("endpoint", endpoint).apply() }

    // ---- recording -----------------------------------------------------------------------------
    fun startRecording(): Boolean {
        stopPlayback()
        if (!recorder.start()) { status = "میکروفون در دسترس نیست."; return false }
        recording = true; recordSeconds = 0.0; status = "در حال ضبط... زمزمه کنید یا بخوانید."
        viewModelScope.launch { while (recording) { recordSeconds = recorder.seconds; if (recordSeconds >= 90) { stopRecording(); break }; delay(100) } }
        return true
    }

    fun stopRecording() {
        if (!recording) return
        recording = false; level = 0f
        val samples = recorder.stop()
        lastSamples = samples; lastRate = recorder.sampleRate
        busy = true; status = "در حال تشخیص نت‌ها..."
        viewModelScope.launch(Dispatchers.Default) {
            val bpm = bpmText.toDoubleOrNull() ?: 0.0
            val (tr, err) = parseTranscription(Native.transcribe(samples, lastRate, bpm))
            withContext(Dispatchers.Main) {
                busy = false
                if (tr == null) { status = err ?: "خطا در تشخیص"; return@withContext }
                transcription = tr
                process()
            }
        }
    }

    // ---- import an audio file instead of recording -------------------------------------------------------------------
    fun importAudio(uri: android.net.Uri) {
        stopPlayback()
        busy = true; status = "در حال خواندن فایل صوتی..."
        viewModelScope.launch(Dispatchers.Default) {
            try {
                val d = AudioDecoder.decode(getApplication(), uri)
                lastSamples = d.samples; lastRate = d.sampleRate
                val bpm = bpmText.toDoubleOrNull() ?: 0.0
                val (tr, err) = parseTranscription(Native.transcribe(d.samples, d.sampleRate, bpm))
                withContext(Dispatchers.Main) {
                    busy = false
                    if (tr == null) { status = err ?: "خطا در تشخیص"; return@withContext }
                    transcription = tr
                    process()
                }
            } catch (e: Exception) {
                withContext(Dispatchers.Main) { busy = false; status = "این فایل خوانده نشد: ${e.message}" }
            }
        }
    }

    // ---- the user's own MIDI files become reference profiles (no notes are kept) --------------------------------------------
    fun addMidiFiles(uris: List<android.net.Uri>) {
        busy = true; status = "در حال ایندکس MIDIها..."
        viewModelScope.launch(Dispatchers.IO) {
            var added = 0
            val app = getApplication<Application>()
            for (u in uris) {
                try {
                    val name = u.lastPathSegment?.substringAfterLast('/') ?: "midi"
                    val bytes = app.contentResolver.openInputStream(u)?.use { it.readBytes() } ?: continue
                    if (JSONObject(Native.indexMidi(name, "my library", bytes)).optBoolean("ok")) added++
                } catch (_: Exception) {}
            }
            try { java.io.File(app.filesDir, "user_library.json").writeText(Native.userLibraryJson()) } catch (_: Exception) {}
            withContext(Dispatchers.Main) { busy = false; libraryInfo = Native.libraryInfo(); status = "$added فایل MIDI به کتابخانه اضافه شد. حالت «مشابه» و AI از آن‌ها استفاده می‌کنند." }
        }
    }

    // ---- analysis + generation ---------------------------------------------------------------------
    fun process() {
        val tr = transcription ?: return
        stopPlayback()
        busy = true; status = "در حال ساخت MIDI..."
        val opt = JSONObject().put("style", style).put("seed", seed).put("bass", bass).put("scale_lock", true)
            .put("keep_bpm", mode != Mode.SIMILAR || !tr.bpmEstimated)
        viewModelScope.launch(Dispatchers.Default) {
            val r = JSONObject(Native.process(mode.key, tr.notes.toString(), tr.bpm, !tr.bpmEstimated, opt.toString()))
            withContext(Dispatchers.Main) {
                busy = false
                if (!r.optBoolean("ok")) { status = r.optString("error", "خطا"); return@withContext }
                clip = Clip(r.getJSONObject("clip")); clipFromVoice = true
                analysis = r.optJSONObject("analysis")
                analysisText = describe(r)
                status = "آماده: ${clip!!.description}"
            }
        }
    }

    private fun describe(r: JSONObject): String {
        val a = r.optJSONObject("analysis") ?: return ""
        val sb = StringBuilder()
        sb.append("کلید: ").append(a.optString("key")).append("  |  BPM: ").append(a.opt("bpm")).append('\n')
        sb.append("سبک تشخیص‌داده‌شده: ").append(a.optString("detected_style"))
        a.optJSONObject("style_scores")?.let { sb.append("  (R&B ${pct(it, "rnb")}، Trap ${pct(it, "trap")}، Hip-Hop ${pct(it, "hiphop")})") }
        a.optJSONArray("reasons")?.let { for (i in 0 until minOf(3, it.length())) sb.append("\n• ").append(it.getString(i)) }
        r.optJSONObject("inspired_by")?.let { sb.append("\nالهام‌گرفته از مرجع: ").append(it.optString("name")).append(" (").append(it.optString("pack")).append(")") }
        r.optJSONArray("chords")?.let { sb.append("\nآکوردها: ").append((0 until it.length()).joinToString(" | ") { i -> it.getString(i) }) }
        return sb.toString()
    }
    private fun pct(o: JSONObject, k: String) = "${Math.round(o.optDouble(k) * 100)}٪"

    /** Re-reads the same recording with the typed BPM (or estimates it again when the field is empty). */
    fun applyBpmAndReanalyze() {
        val samples = lastSamples ?: return
        busy = true; status = "در حال تحلیل مجدد..."
        viewModelScope.launch(Dispatchers.Default) {
            val bpm = bpmText.toDoubleOrNull() ?: 0.0
            val (tr, err) = parseTranscription(Native.transcribe(samples, lastRate, bpm))
            withContext(Dispatchers.Main) {
                busy = false
                if (tr == null) { status = err ?: "خطا در تشخیص"; return@withContext }
                transcription = tr
                process()
            }
        }
    }

    fun variation() { seed += 1; process() }

    // ---- playback -----------------------------------------------------------------------------------------
    fun togglePlay() { if (playing) stopPlayback() else play() }

    private fun play() {
        val c = clip ?: return
        playing = true; playTicks = 0.0
        viewModelScope.launch {
            val pcm = withContext(Dispatchers.Default) { Synth.render(c) }
            if (!playing) return@launch
            val t = Synth.track(pcm)
            track = t
            t.play()
            while (playing) {
                val frames = t.playbackHeadPosition
                playTicks = frames.toDouble() / Synth.SAMPLE_RATE * c.bpm / 60.0 * PPQ
                if (frames >= pcm.size - Synth.SAMPLE_RATE / 4) break
                delay(25)
            }
            stopPlayback()
        }
    }

    fun stopPlayback() {
        playing = false
        track?.let { try { it.stop() } catch (_: Exception) {}; it.release() }
        track = null
    }

    // ---- export -----------------------------------------------------------------------------------------------
    fun save(): String = clip?.let { MidiExport.saveToDownloads(getApplication(), it) } ?: "هنوز کلیپی ساخته نشده"
    fun share() { clip?.let { MidiExport.share(getApplication(), it) } }

    // ---- AI ------------------------------------------------------------------------------------------------------
    fun askAi(prompt: String) {
        saveAiSettings()
        aiBusy = true; aiMessage = "در حال گفتگو با مدل..."
        val ctx = JSONObject().put("analysis", analysis ?: JSONObject()).put("mode", mode.key).put("style", style).toString()
        val current = clip?.json
        viewModelScope.launch(Dispatchers.IO) {
            val out = Ai.run(AiConfig(provider, apiKey, model, endpoint), prompt, ctx, mcpServers) { current }
            withContext(Dispatchers.Main) {
                aiBusy = false
                aiMessage = out.message
                out.clip?.let { stopPlayback(); clip = Clip(it); clipFromVoice = false; status = "ساخته‌شده با AI: ${clip!!.description}" }
            }
        }
    }

    override fun onCleared() { stopPlayback(); super.onCleared() }
}
