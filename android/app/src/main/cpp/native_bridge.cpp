// JNI bridge: Kotlin <-> hhmg core. All data crosses as JSON strings / float arrays.
#include <jni.h>
#include <memory>
#include <mutex>
#include <string>
#include "hhmg/Library.h"
#include "hhmg/Studio.h"
#include "hhmg/Tools.h"

using namespace hhmg;

namespace {
std::mutex gMutex;
std::shared_ptr<PatternLibrary> gLib = std::make_shared<PatternLibrary>(PatternLibrary::builtin());
PatternLibrary gUser;   // only what the user added (profiles of their own MIDI files); saved by the app

std::string str(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string r(c ? c : "");
    if (c) env->ReleaseStringUTFChars(s, c);
    return r;
}
jstring jstr(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }
std::shared_ptr<PatternLibrary> lib() { std::lock_guard<std::mutex> l(gMutex); return gLib; }

std::string errJson(const std::string& m) { Json e = Json::object(); e.set("ok", false); e.set("error", m); return e.dump(); }

std::vector<Note> notesFromJson(const Json& arr) {
    std::vector<Note> notes;
    for (const auto& x : arr.items()) {
        if (x.size() < 3) continue;
        Note n; n.pitch = x.at(0).integer(); n.start = x.at(1).num(); n.length = x.at(2).num();
        n.velocity = x.size() > 3 ? x.at(3).integer() : 90; n.channel = kMelody;
        notes.push_back(n);
    }
    return notes;
}
} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL Java_com_hhmg_studio_Native_loadPatterns(JNIEnv* env, jobject, jstring json) {
    try {
        auto next = std::make_shared<PatternLibrary>(*lib());
        next->mergeJson(Json::parse(str(env, json)));
        std::lock_guard<std::mutex> l(gMutex);
        gLib = next;
        return JNI_TRUE;
    } catch (...) { return JNI_FALSE; }
}

// profiles of the user's own MIDI files (no notes are kept)
JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_indexMidi(JNIEnv* env, jobject, jstring name, jstring pack, jbyteArray bytes) {
    try {
        jsize n = env->GetArrayLength(bytes);
        std::vector<uint8_t> b((size_t) n);
        env->GetByteArrayRegion(bytes, 0, n, reinterpret_cast<jbyte*>(b.data()));
        auto next = std::make_shared<PatternLibrary>(*lib());
        std::string err;
        std::lock_guard<std::mutex> l(gMutex);
        bool ok = indexMidiBytes(b, str(env, pack), str(env, name), *next, &gUser, err);
        if (ok) gLib = next;
        Json out = Json::object();
        out.set("ok", ok);
        if (!ok) out.set("error", err);
        out.set("total_references", (int) gLib->references.size());
        return jstr(env, out.dump());
    } catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_userLibraryJson(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> l(gMutex);
    return jstr(env, gUser.toJson().dump());
}

JNIEXPORT jboolean JNICALL Java_com_hhmg_studio_Native_loadUserLibrary(JNIEnv* env, jobject, jstring json) {
    try {
        Json j = Json::parse(str(env, json));
        auto next = std::make_shared<PatternLibrary>(*lib());
        next->mergeJson(j);
        std::lock_guard<std::mutex> l(gMutex);
        gUser.mergeJson(j);
        gLib = next;
        return JNI_TRUE;
    } catch (...) { return JNI_FALSE; }
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_similarToClip(JNIEnv* env, jobject, jstring clipJson, jstring optionsJson) {
    try { return jstr(env, similarToClip(Json::parse(str(env, clipJson)), Json::parse(str(env, optionsJson)), *lib()).dump()); }
    catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_libraryInfo(JNIEnv* env, jobject) {
    auto l = lib();
    return jstr(env, std::to_string(l->progressions.size()) + " progressions, " + std::to_string(l->rhythms.size()) + " rhythms, " +
                         std::to_string(l->motifs.size()) + " motifs, " + std::to_string(l->references.size()) + " references");
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_transcribe(JNIEnv* env, jobject, jfloatArray samples, jint sampleRate, jdouble bpm) {
    try {
        jsize n = env->GetArrayLength(samples);
        std::vector<float> x((size_t) n);
        env->GetFloatArrayRegion(samples, 0, n, x.data());
        TranscribeOptions o; o.bpm = bpm;
        return jstr(env, transcriptionToJson(transcribeAudio(x, sampleRate, o)).dump());
    } catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_process(JNIEnv* env, jobject, jstring mode, jstring notesJson, jdouble bpm, jboolean bpmKnown, jstring optionsJson) {
    try {
        auto notes = notesFromJson(Json::parse(str(env, notesJson)));
        Json opt = Json::parse(str(env, optionsJson));
        return jstr(env, studioProcess(str(env, mode), notes, bpm, bpmKnown == JNI_TRUE, opt, *lib()).dump());
    } catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_callTool(JNIEnv* env, jobject, jstring name, jstring argsJson) {
    try {
        Json args = Json::parse(str(env, argsJson));
        args.set("output_path", "");   // a model must never write files on the phone
        return jstr(env, callTool(str(env, name), args, *lib()).dump());
    } catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

// generate_midi for the AI loop: returns the usual summary plus the clip
JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_generateTool(JNIEnv* env, jobject, jstring argsJson) {
    try {
        Json args = Json::parse(str(env, argsJson));
        auto l = lib();
        Request rq = requestFromJson(args);
        std::string refErr, note = applyReferences(rq, args, *l, refErr);
        if (!refErr.empty()) return jstr(env, errJson(refErr));
        Result r = generate(rq, *l);
        Json out = resultToJson(r, false);
        out.set("ok", true);
        if (!note.empty()) out.set("used_references", note);
        out.set("clip", clipToJson(r.notes, r.request.bpm, r.request.bars, r.description));
        return jstr(env, out.dump());
    } catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_toolSchemas(JNIEnv* env, jobject) { return jstr(env, toolSchemas().dump()); }

JNIEXPORT jstring JNICALL Java_com_hhmg_studio_Native_analyzeClip(JNIEnv* env, jobject, jstring clipJson) {
    try { return jstr(env, analyzeClip(Json::parse(str(env, clipJson)), *lib()).dump()); }
    catch (const std::exception& e) { return jstr(env, errJson(e.what())); }
}

JNIEXPORT jbyteArray JNICALL Java_com_hhmg_studio_Native_clipToMidi(JNIEnv* env, jobject, jstring clipJson) {
    try {
        auto bytes = clipToMidi(Json::parse(str(env, clipJson)));
        jbyteArray out = env->NewByteArray((jsize) bytes.size());
        if (!bytes.empty()) env->SetByteArrayRegion(out, 0, (jsize) bytes.size(), reinterpret_cast<const jbyte*>(bytes.data()));
        return out;
    } catch (...) { return env->NewByteArray(0); }
}

} // extern "C"
