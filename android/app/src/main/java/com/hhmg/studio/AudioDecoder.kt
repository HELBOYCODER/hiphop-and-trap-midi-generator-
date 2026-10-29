package com.hhmg.studio

import android.content.Context
import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import android.net.Uri
import java.nio.ByteOrder

/** Decodes any audio file Android can play (wav, mp3, m4a/aac, ogg, flac ...) to mono float PCM. */
object AudioDecoder {
    class Decoded(val samples: FloatArray, val sampleRate: Int)

    fun decode(context: Context, uri: Uri, maxSeconds: Int = 90): Decoded {
        val extractor = MediaExtractor()
        extractor.setDataSource(context, uri, null)
        var track = -1
        for (i in 0 until extractor.trackCount) {
            if (extractor.getTrackFormat(i).getString(MediaFormat.KEY_MIME)?.startsWith("audio/") == true) { track = i; break }
        }
        require(track >= 0) { "This file has no audio track" }
        extractor.selectTrack(track)
        val format = extractor.getTrackFormat(track)
        var sampleRate = format.getInteger(MediaFormat.KEY_SAMPLE_RATE)
        var channels = format.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
        val codec = MediaCodec.createDecoderByType(format.getString(MediaFormat.KEY_MIME)!!)
        codec.configure(format, null, null, 0)
        codec.start()

        val info = MediaCodec.BufferInfo()
        var out = FloatArray(sampleRate * 10)
        var count = 0
        var inputDone = false
        var outputDone = false
        try {
            while (!outputDone && count < sampleRate * maxSeconds) {
                if (!inputDone) {
                    val inIdx = codec.dequeueInputBuffer(10_000)
                    if (inIdx >= 0) {
                        val buf = codec.getInputBuffer(inIdx)!!
                        val size = extractor.readSampleData(buf, 0)
                        if (size < 0) {
                            codec.queueInputBuffer(inIdx, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                            inputDone = true
                        } else {
                            codec.queueInputBuffer(inIdx, 0, size, extractor.sampleTime, 0)
                            extractor.advance()
                        }
                    }
                }
                val outIdx = codec.dequeueOutputBuffer(info, 10_000)
                when {
                    outIdx >= 0 -> {
                        val buf = codec.getOutputBuffer(outIdx)!!
                        buf.position(info.offset); buf.limit(info.offset + info.size)
                        val shorts = buf.order(ByteOrder.nativeOrder()).asShortBuffer()
                        val frames = shorts.remaining() / channels
                        if (count + frames > out.size) out = out.copyOf(maxOf(out.size * 2, count + frames))
                        for (f in 0 until frames) {
                            var acc = 0f
                            for (c in 0 until channels) acc += shorts.get() / 32768f
                            out[count++] = acc / channels
                        }
                        codec.releaseOutputBuffer(outIdx, false)
                        if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) outputDone = true
                    }
                    outIdx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                        val f = codec.outputFormat
                        sampleRate = f.getInteger(MediaFormat.KEY_SAMPLE_RATE)
                        channels = f.getInteger(MediaFormat.KEY_CHANNEL_COUNT)
                    }
                }
            }
        } finally {
            try { codec.stop() } catch (_: Exception) {}
            codec.release(); extractor.release()
        }
        return Decoded(out.copyOf(count), sampleRate)
    }
}
