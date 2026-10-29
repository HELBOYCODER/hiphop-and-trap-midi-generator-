package com.hhmg.studio

import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.os.Build
import android.provider.MediaStore
import androidx.core.content.FileProvider
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object MidiExport {
    private fun fileName(clip: Clip): String =
        "HipHopStudio_${clip.bpm}bpm_" + SimpleDateFormat("HHmmss", Locale.US).format(Date()) + ".mid"

    /** Private copy in the app's folder (used for sharing). */
    fun writeLocal(context: Context, clip: Clip): File {
        val dir = context.getExternalFilesDir(null) ?: context.cacheDir
        val f = File(dir, fileName(clip))
        f.writeBytes(Native.clipToMidi(clip.json.toString()))
        return f
    }

    /** Saves into Downloads (Android 10+); older versions keep it in the app folder. Returns a description of where it went. */
    fun saveToDownloads(context: Context, clip: Clip): String {
        val bytes = Native.clipToMidi(clip.json.toString())
        if (bytes.isEmpty()) return "Export failed"
        val name = fileName(clip)
        return if (Build.VERSION.SDK_INT >= 29) {
            val values = ContentValues().apply {
                put(MediaStore.Downloads.DISPLAY_NAME, name)
                put(MediaStore.Downloads.MIME_TYPE, "audio/midi")
                put(MediaStore.Downloads.RELATIVE_PATH, "Download/HipHopMidiStudio")
            }
            val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
            if (uri == null) "Could not create the file" else {
                context.contentResolver.openOutputStream(uri)?.use { it.write(bytes) }
                "Saved to Downloads/HipHopMidiStudio/$name"
            }
        } else {
            val f = File(context.getExternalFilesDir(null) ?: context.cacheDir, name)
            f.writeBytes(bytes)
            "Saved to ${f.absolutePath}"
        }
    }

    fun share(context: Context, clip: Clip) {
        val f = writeLocal(context, clip)
        val uri = FileProvider.getUriForFile(context, context.packageName + ".fileprovider", f)
        val send = Intent(Intent.ACTION_SEND).apply {
            type = "audio/midi"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        context.startActivity(Intent.createChooser(send, "Share MIDI").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    }
}
