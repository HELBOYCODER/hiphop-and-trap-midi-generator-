package com.hhmg.studio

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlin.math.max
import kotlin.math.min

private val BLACK_KEYS = setOf(1, 3, 6, 8, 10)
private val COL_CHORD = Color(0xFF5B8DEF)
private val COL_MELODY = Color(0xFFFFB020)
private val COL_BASS = Color(0xFFE0526B)
private val COL_VOICE = Color(0xFF39C5BB)

/**
 * Piano roll of the clip with the sung pitch curve (frequency) drawn over it, bar/beat grid and a playhead.
 * contour: (time sec, fractional MIDI or -1); offsetSec aligns the recording's first note with tick 0.
 */
@Composable
fun PianoRoll(clip: Clip, contour: List<Pair<Double, Double>>, offsetSec: Double, playTicks: Double, playing: Boolean, modifier: Modifier = Modifier) {
    BoxWithConstraints(modifier.fillMaxWidth().height(260.dp).background(Color(0xFF1A1C21))) {
        val width: Dp = max(maxWidth.value, clip.bars * 150f).dp
        val scroll = rememberScrollState()
        Box(Modifier.fillMaxWidth().height(260.dp).horizontalScroll(scroll)) {
        Canvas(Modifier.width(width).height(260.dp)) {
            val w = size.width; val h = size.height
            var lo = 127; var hi = 0
            for (n in clip.notes) { lo = min(lo, n.pitch); hi = max(hi, n.pitch) }
            if (lo > hi) { lo = 48; hi = 72 }
            lo -= 2; hi += 2
            if (hi - lo < 14) { val c = (hi + lo) / 2; lo = c - 7; hi = c + 7 }
            val rows = hi - lo + 1
            val rowH = h / rows
            val total = clip.totalTicks.toFloat()
            fun x(ticks: Double) = (ticks / total * w).toFloat()
            fun y(midi: Double) = (h - (midi - lo + 0.5) * rowH).toFloat()

            for (p in lo..hi) {
                val top = h - (p - lo + 1) * rowH
                if (p % 12 in BLACK_KEYS) drawRect(Color(0x14FFFFFF), Offset(0f, top), Size(w, rowH))
                if (p % 12 == 0) drawLine(Color(0x33FFFFFF), Offset(0f, top + rowH), Offset(w, top + rowH), 1f)
            }
            for (bar in 0..clip.bars) {
                val bx = x((bar * TICKS_PER_BAR).toDouble())
                drawLine(Color(if (bar % 4 == 0) 0x55FFFFFF else 0x22FFFFFF), Offset(bx, 0f), Offset(bx, h), if (bar % 4 == 0) 2f else 1f)
                if (bar < clip.bars) for (b in 1..3) {
                    val tx = x((bar * TICKS_PER_BAR + b * PPQ).toDouble())
                    drawLine(Color(0x12FFFFFF), Offset(tx, 0f), Offset(tx, h), 1f)
                }
            }
            for (n in clip.notes) {
                val col = when (n.channel) { 0 -> COL_CHORD; 2 -> COL_BASS; else -> COL_MELODY }
                val a = 0.45f + 0.55f * (n.velocity / 127f)
                drawRoundRect(col.copy(alpha = a), Offset(x(n.start), h - (n.pitch - lo + 1) * rowH + 0.5f),
                    Size(max(3f, x(n.length) - 1.5f), max(3f, rowH - 1f)), CornerRadius(3f, 3f))
            }
            // the singer's frequency curve
            if (contour.isNotEmpty()) {
                val ticksPerSec = clip.bpm / 60.0 * PPQ
                val path = Path()
                var open = false
                for ((t, m) in contour) {
                    if (m < 0) { open = false; continue }
                    val px = x((t - offsetSec) * ticksPerSec)
                    if (px < 0f || px > w) { open = false; continue }
                    val py = y(m).coerceIn(0f, h)
                    if (!open) { path.moveTo(px, py); open = true } else path.lineTo(px, py)
                }
                drawPath(path, COL_VOICE.copy(alpha = 0.9f), style = Stroke(width = 3f))
            }
            if (playing) drawLine(Color.White, Offset(x(playTicks), 0f), Offset(x(playTicks), h), 3f)
        }
        }
    }
}
