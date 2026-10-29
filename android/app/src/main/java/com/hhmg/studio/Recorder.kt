package com.hhmg.studio

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import kotlin.math.abs
import kotlin.math.max

/** Records mono 16 kHz audio (up to 90 s) and reports a live input level. */
class Recorder(private val onLevel: (Float) -> Unit) {
    val sampleRate = 16000
    private var record: AudioRecord? = null
    private var thread: Thread? = null
    @Volatile private var running = false
    private val chunks = ArrayList<FloatArray>()
    private var total = 0

    @SuppressLint("MissingPermission")
    fun start(): Boolean {
        val min = AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        if (min <= 0) return false
        val rec = AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT, max(min, sampleRate / 2) * 2)
        if (rec.state != AudioRecord.STATE_INITIALIZED) { rec.release(); return false }
        chunks.clear(); total = 0
        record = rec; running = true
        rec.startRecording()
        thread = Thread {
            val buf = ShortArray(1024)
            while (running) {
                val n = rec.read(buf, 0, buf.size)
                if (n <= 0) continue
                val f = FloatArray(n)
                var peak = 0f
                for (i in 0 until n) { f[i] = buf[i] / 32768f; peak = max(peak, abs(f[i])) }
                synchronized(chunks) { chunks.add(f); total += n }
                onLevel(peak)
                if (total > sampleRate * 90) running = false
            }
        }.also { it.start() }
        return true
    }

    val seconds: Double get() = total.toDouble() / sampleRate

    fun stop(): FloatArray {
        running = false
        thread?.join(500)
        record?.let { try { it.stop() } catch (_: Exception) {}; it.release() }
        record = null
        val out = FloatArray(total)
        var p = 0
        synchronized(chunks) { for (c in chunks) { c.copyInto(out, p); p += c.size } }
        return out
    }
}
