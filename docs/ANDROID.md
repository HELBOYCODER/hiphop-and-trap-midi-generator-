# HipHop MIDI Studio (Android) / نسخه‌ی اندروید

Sing or hum -> the app finds the notes, key and style -> you get a MIDI file you can hear and see on a piano roll.
Uses the same C++ engine as the VST3 (`core/`, through the NDK), so output never leaves the detected scale.

خواندن یا زمزمه ← تشخیص نت، گام و سبک ← فایل MIDI با پخش و پیانو رول. موتور همان موتور پلاگین است (NDK) و نت خارج از گام نمی‌سازد.

## What it does
1. **Record** (mic, up to 90 s, 16 kHz mono).
2. **Voice -> notes** (`core/src/Transcribe.cpp`): YIN pitch tracking, note segmentation, tempo estimate, 16th-grid quantising.
   The sung **frequency curve** is drawn over the piano roll (teal line); during playback the current note and its Hz are shown.
3. **Key / style analysis** (`Analyze.cpp`): key & mode, BPM, chord degrees, swing, R&B / Trap / Hip-Hop scores with reasons.
4. **Three outputs**: *My melody* (snapped to the detected scale), *My melody + chords + 808*, *Similar idea* (new notes in the same key, tempo, feel and rhythm). "Another idea" changes the seed; style, 808 and BPM can be overridden.
5. **Listen** with the built-in synth, **save** to `Downloads/HipHopMidiStudio/` (Android 10+) or **share** the `.mid`.
6. **AI (Gemini, needs internet)**: say what you want ("like this but darker trap"); the model calls the same tools as the plugin (`analyze_current_clip`, `style_guide`, `find_references`, `generate_midi`) and explains its choices. Your API key is stored only on the phone.

## Build
- **GitHub**: `.github/workflows/android.yml` builds `HipHopMidiStudio-debug.apk` (artifact). Debug-signed, so it installs by sideloading (enable "install unknown apps").
- **Locally**: open `android/` in Android Studio (JDK 17, NDK + CMake 3.22.1 from the SDK Manager) and run, or `cd android && gradle :app:assembleDebug`.

## Honest limits
- Pitch tracking is tuned for a single voice, close to the mic, in a quiet room. Noise, whispering, very low/high pitches or fast runs reduce accuracy. Test on synthetic singing: 8/8 notes exact; real voices will be worse.
- It transcribes one melody line (monophonic), not chords or multiple voices.
- Tempo is estimated from the note onsets; type the BPM if it guesses wrong (half/double tempo is common).
- Not signed for the Play Store; `minSdk` 26 (Android 8+). Needs the Gemini key for AI mode only; recording, transcription, generation and playback are offline.
