package com.hhmg.studio

/** Thin JNI wrapper around the shared C++ engine (same code as the VST3 plugin). */
object Native {
    init { System.loadLibrary("hhmg_android") }

    external fun loadPatterns(json: String): Boolean
    external fun libraryInfo(): String
    external fun indexMidi(name: String, pack: String, bytes: ByteArray): String
    external fun userLibraryJson(): String
    external fun loadUserLibrary(json: String): Boolean
    external fun similarToClip(clipJson: String, optionsJson: String): String
    external fun transcribe(samples: FloatArray, sampleRate: Int, bpm: Double): String
    external fun process(mode: String, notesJson: String, bpm: Double, bpmKnown: Boolean, optionsJson: String): String
    external fun callTool(name: String, argsJson: String): String
    external fun generateTool(argsJson: String): String
    external fun toolSchemas(): String
    external fun analyzeClip(clipJson: String): String
    external fun clipToMidi(clipJson: String): ByteArray
}
