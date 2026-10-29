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

    var recording by mutableStateOf(false); private set
    var recordSeconds by mutableStateOf(0.0); private set
    var level by mutableStateOf(0f); private set
    var busy by mutableStateOf(false); private set
    var status by mutableStateOf("برای شروع، ضبط را بزنید و ملودی را زمزمه یا بخوانید."); private set

    var transcription by mutableStateOf<Transcription?>(null); private set
    var clip by mutableStateOf<Clip?>(null); private set
    var analysisText by mutableStateOf(""); private set
    private var analysis: JSONObject? = null

    var mode by mutableStateOf(Mode.MELODY)
    var style by mutableStateOf("")          // "" = detected automatically
    var bass by mutableStateOf(true)
    var bpmText by mutableStateOf("")        // empty = estimated from the singing
    var seed by mutableStateOf(1)

    var playing by mutableStateOf(false); private set
    var playTicks by mutableStateOf(0.0); private set

    var apiKey by mutableStateOf(prefs.getString("apiKey", "") ?: "")
    var model by mutableStateOf(prefs.getString("model", "gemini-2.5-flash") ?: "gemini-2.5-flash")
    var aiBusy by mutableStateOf(false); private set
    var aiMessage by mutableStateOf(""); private set
    var libraryInfo by mutableStateOf(""); private set

    init {
        viewModelScope.launch(Dispatchers.IO) {
            val am = getApplication<Application>().assets
            for (f in listOf("patterns.pop909.json", "patterns.cymatics.json")) {
                try { Native.loadPatterns(am.open(f).bufferedReader().use { it.readText() }) } catch (_: Exception) {}
            }
            libraryInfo = Native.libraryInfo()
        }
    }

    fun saveAiSettings() { prefs.edit().putString("apiKey", apiKey).putString("model", model).apply() }

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
        lastSamples = samples
        busy = true; status = "در حال تشخیص نت‌ها..."
        viewModelScope.launch(Dispatchers.Default) {
            val bpm = bpmText.toDoubleOrNull() ?: 0.0
            val (tr, err) = parseTranscription(Native.transcribe(samples, recorder.sampleRate, bpm))
            withContext(Dispatchers.Main) {
                busy = false
                if (tr == null) { status = err ?: "خطا در تشخیص"; return@withContext }
                transcription = tr
                process()
            }
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
                clip = Clip(r.getJSONObject("clip"))
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
            val (tr, err) = parseTranscription(Native.transcribe(samples, recorder.sampleRate, bpm))
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
            val out = GeminiClient().run(apiKey, model, prompt, ctx) { current }
            withContext(Dispatchers.Main) {
                aiBusy = false
                aiMessage = out.message
                out.clip?.let { stopPlayback(); clip = Clip(it); status = "ساخته‌شده با AI: ${clip!!.description}" }
            }
        }
    }

    override fun onCleared() { stopPlayback(); super.onCleared() }
}
