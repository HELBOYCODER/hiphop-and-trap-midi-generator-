package com.hhmg.studio

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.ln
import kotlin.math.pow
import kotlin.math.roundToInt

const val PPQ = 480
const val TICKS_PER_BAR = PPQ * 4

data class NoteEv(val pitch: Int, val start: Double, val length: Double, val velocity: Int, val channel: Int)

/** channel 0 = chords, 1 = melody, 2 = 808 bass */
class Clip(val json: JSONObject) {
    val bpm: Int = json.optInt("bpm", 120)
    val bars: Int = json.optInt("bars", 4).coerceIn(1, 64)
    val description: String = json.optString("description", "")
    val notes: List<NoteEv> = run {
        val arr = json.optJSONArray("notes") ?: JSONArray()
        (0 until arr.length()).mapNotNull { i ->
            val a = arr.optJSONArray(i) ?: return@mapNotNull null
            if (a.length() < 5) null else NoteEv(a.getInt(0), a.getDouble(1), a.getDouble(2), a.getInt(3), a.getInt(4))
        }
    }
    val totalTicks: Double get() = (bars * TICKS_PER_BAR).toDouble()
    val seconds: Double get() = totalTicks / PPQ * 60.0 / bpm
    fun ticksToSeconds(t: Double) = t / PPQ * 60.0 / bpm
}

class Transcription(
    val bpm: Double,
    val bpmEstimated: Boolean,
    val durationSec: Double,
    val startOffsetSec: Double,
    val contour: List<Pair<Double, Double>>,   // (time sec, fractional MIDI or -1 for silence)
    val notes: JSONArray,
)

fun parseTranscription(json: String): Pair<Transcription?, String?> {
    val o = JSONObject(json)
    if (!o.optBoolean("ok", false)) return null to o.optString("error", "Could not read the recording")
    val c = o.getJSONArray("contour")
    val contour = (0 until c.length()).map { val p = c.getJSONArray(it); p.getDouble(0) to p.getDouble(1) }
    return Transcription(o.getDouble("bpm"), o.optBoolean("bpm_estimated", true), o.optDouble("duration_sec", 0.0),
        o.optDouble("start_offset_sec", 0.0), contour, o.getJSONArray("notes")) to null
}

private val NAMES = arrayOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
fun noteName(midi: Int): String = NAMES[((midi % 12) + 12) % 12] + (midi / 12 - 1)
fun midiToHz(midi: Double): Double = 440.0 * 2.0.pow((midi - 69.0) / 12.0)
fun hzText(midi: Int): String = "${midiToHz(midi.toDouble()).roundToInt()} Hz"
@Suppress("unused") fun hzToMidi(hz: Double): Double = 69.0 + 12.0 * ln(hz / 440.0) / ln(2.0)
