package com.hhmg.studio

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.min
import kotlin.math.sin

/** Small additive synth so a generated clip can be heard right away (no soundfont needed). */
object Synth {
    const val SAMPLE_RATE = 22050

    fun render(clip: Clip): ShortArray {
        val total = (clip.seconds * SAMPLE_RATE).toInt() + SAMPLE_RATE / 2   // tail for releases
        val mix = FloatArray(total)
        for (n in clip.notes) {
            val start = (clip.ticksToSeconds(n.start) * SAMPLE_RATE).toInt()
            val dur = (clip.ticksToSeconds(n.length) * SAMPLE_RATE).toInt().coerceAtLeast(SAMPLE_RATE / 20)
            val release = SAMPLE_RATE / 6
            val hz = midiToHz(n.pitch.toDouble())
            val amp = n.velocity / 127.0 * when (n.channel) { 0 -> 0.16; 2 -> 0.34; else -> 0.26 }
            val step = 2.0 * PI * hz / SAMPLE_RATE
            var phase = 0.0
            val len = min(dur + release, total - start)
            for (i in 0 until len) {
                val t = i.toDouble() / SAMPLE_RATE
                val attack = min(1.0, t / (if (n.channel == 0) 0.04 else 0.008))
                val decay = when (n.channel) { 1 -> exp(-t * 2.2); 2 -> exp(-t * 1.1); else -> 0.75 + 0.25 * exp(-t * 3.0) }
                val rel = if (i > dur) 1.0 - (i - dur).toDouble() / release else 1.0
                phase += step
                val s = when (n.channel) {
                    2 -> sin(phase) + 0.35 * sin(2 * phase)                                  // 808: sine + a little 2nd
                    0 -> sin(phase) + 0.45 * sin(2 * phase) + 0.2 * sin(3 * phase)           // soft pad
                    else -> sin(phase) + 0.55 * sin(2 * phase) + 0.3 * sin(3 * phase) + 0.15 * sin(4 * phase) // pluck lead
                }
                mix[start + i] += (amp * attack * decay * rel * s).toFloat()
            }
        }
        val out = ShortArray(total)
        for (i in 0 until total) out[i] = (mix[i].coerceIn(-1f, 1f) * 30000f).toInt().toShort()
        return out
    }

    fun track(pcm: ShortArray): AudioTrack {
        val t = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_MEDIA).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            .setAudioFormat(AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(SAMPLE_RATE).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build())
            .setBufferSizeInBytes(pcm.size * 2)
            .setTransferMode(AudioTrack.MODE_STATIC)
            .build()
        t.write(pcm, 0, pcm.size)
        return t
    }
}
