// Minimal dependency-free JSON value, parser and serializer.
#pragma once
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace hhmg {

class Json {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool v) : type_(Bool), b_(v) {}
    Json(int v) : type_(Number), n_(v) {}
    Json(double v) : type_(Number), n_(v) {}
    Json(const char* v) : type_(String), s_(v) {}
    Json(std::string v) : type_(String), s_(std::move(v)) {}

    static Json array() { Json j; j.type_ = Array; return j; }
    static Json object() { Json j; j.type_ = Object; return j; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool isNumber() const { return type_ == Number; }
    bool isString() const { return type_ == String; }
    bool isArray() const { return type_ == Array; }
    bool isObject() const { return type_ == Object; }

    double num(double def = 0) const { return type_ == Number ? n_ : (type_ == Bool ? (b_ ? 1 : 0) : def); }
    int integer(int def = 0) const { return type_ == Number ? (int) (n_ < 0 ? n_ - 0.5 : n_ + 0.5) : def; }
    bool boolean(bool def = false) const { return type_ == Bool ? b_ : (type_ == Number ? n_ != 0 : def); }
    const std::string& str() const { return s_; }
    std::string str(const std::string& def) const { return type_ == String ? s_ : def; }

    size_t size() const { return type_ == Array ? a_.size() : keys_.size(); }
    const Json& at(size_t i) const { return a_.at(i); }
    const std::vector<Json>& items() const { return a_; }
    void push(Json v) { a_.push_back(std::move(v)); }

    bool has(const std::string& k) const { return find(k) >= 0; }
    const Json& operator[](const std::string& k) const {
        static const Json nul;
        int i = find(k);
        return i >= 0 ? v_[(size_t) i] : nul;
    }
    Json& set(const std::string& k, Json v) {
        int i = find(k);
        if (i >= 0) { v_[(size_t) i] = std::move(v); return v_[(size_t) i]; }
        keys_.push_back(k);
        v_.push_back(std::move(v));
        return v_.back();
    }
    const std::vector<std::string>& keys() const { return keys_; }
    const Json& valueAt(size_t i) const { return v_.at(i); }

    std::string dump(bool pretty = false) const {
        std::string out;
        write(out, pretty, 0);
        return out;
    }

    static Json parse(const std::string& text) {
        size_t p = 0;
        Json j = parseValue(text, p);
        skip(text, p);
        if (p != text.size()) throw std::runtime_error("JSON: trailing characters");
        return j;
    }

private:
    Type type_ = Null;
    bool b_ = false;
    double n_ = 0;
    std::string s_;
    std::vector<Json> a_;
    std::vector<std::string> keys_;
    std::vector<Json> v_;

    int find(const std::string& k) const {
        for (size_t i = 0; i < keys_.size(); ++i) if (keys_[i] == k) return (int) i;
        return -1;
    }

    static void skip(const std::string& t, size_t& p) {
        while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\r' || t[p] == '\t')) ++p;
    }

    static void escape(std::string& out, const std::string& s) {
        out += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                    else out += (char) c;
            }
        }
        out += '"';
    }

    void write(std::string& out, bool pretty, int depth) const {
        auto nl = [&](int d) { if (pretty) { out += '\n'; out.append((size_t) d * 2, ' '); } };
        switch (type_) {
            case Null: out += "null"; break;
            case Bool: out += b_ ? "true" : "false"; break;
            case Number: {
                char b[40];
                if (n_ == (long long) n_) std::snprintf(b, sizeof b, "%lld", (long long) n_);
                else std::snprintf(b, sizeof b, "%.6g", n_);
                out += b;
                break;
            }
            case String: escape(out, s_); break;
            case Array:
                out += '[';
                for (size_t i = 0; i < a_.size(); ++i) {
                    if (i) out += ',';
                    nl(depth + 1);
                    a_[i].write(out, pretty, depth + 1);
                }
                if (!a_.empty()) nl(depth);
                out += ']';
                break;
            case Object:
                out += '{';
                for (size_t i = 0; i < keys_.size(); ++i) {
                    if (i) out += ',';
                    nl(depth + 1);
                    escape(out, keys_[i]);
                    out += pretty ? ": " : ":";
                    v_[i].write(out, pretty, depth + 1);
                }
                if (!keys_.empty()) nl(depth);
                out += '}';
                break;
        }
    }

    static void appendUtf8(std::string& s, unsigned cp) {
        if (cp < 0x80) s += (char) cp;
        else if (cp < 0x800) { s += (char) (0xC0 | (cp >> 6)); s += (char) (0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { s += (char) (0xE0 | (cp >> 12)); s += (char) (0x80 | ((cp >> 6) & 0x3F)); s += (char) (0x80 | (cp & 0x3F)); }
        else { s += (char) (0xF0 | (cp >> 18)); s += (char) (0x80 | ((cp >> 12) & 0x3F)); s += (char) (0x80 | ((cp >> 6) & 0x3F)); s += (char) (0x80 | (cp & 0x3F)); }
    }

    static std::string parseString(const std::string& t, size_t& p) {
        std::string out;
        ++p; // opening quote
        while (p < t.size() && t[p] != '"') {
            char c = t[p++];
            if (c != '\\') { out += c; continue; }
            if (p >= t.size()) break;
            char e = t[p++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (p + 4 > t.size()) throw std::runtime_error("JSON: bad \\u escape");
                    unsigned cp = (unsigned) std::strtoul(t.substr(p, 4).c_str(), nullptr, 16);
                    p += 4;
                    if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= t.size() && t[p] == '\\' && t[p + 1] == 'u') {
                        unsigned lo = (unsigned) std::strtoul(t.substr(p + 2, 4).c_str(), nullptr, 16);
                        p += 6;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: out += e;
            }
        }
        if (p >= t.size()) throw std::runtime_error("JSON: unterminated string");
        ++p;
        return out;
    }

    static Json parseValue(const std::string& t, size_t& p) {
        skip(t, p);
        if (p >= t.size()) throw std::runtime_error("JSON: unexpected end");
        char c = t[p];
        if (c == '{') {
            Json o = object();
            ++p; skip(t, p);
            if (p < t.size() && t[p] == '}') { ++p; return o; }
            for (;;) {
                skip(t, p);
                if (p >= t.size() || t[p] != '"') throw std::runtime_error("JSON: expected key");
                std::string k = parseString(t, p);
                skip(t, p);
                if (p >= t.size() || t[p] != ':') throw std::runtime_error("JSON: expected ':'");
                ++p;
                o.set(k, parseValue(t, p));
                skip(t, p);
                if (p < t.size() && t[p] == ',') { ++p; continue; }
                if (p < t.size() && t[p] == '}') { ++p; return o; }
                throw std::runtime_error("JSON: expected ',' or '}'");
            }
        }
        if (c == '[') {
            Json a = array();
            ++p; skip(t, p);
            if (p < t.size() && t[p] == ']') { ++p; return a; }
            for (;;) {
                a.push(parseValue(t, p));
                skip(t, p);
                if (p < t.size() && t[p] == ',') { ++p; continue; }
                if (p < t.size() && t[p] == ']') { ++p; return a; }
                throw std::runtime_error("JSON: expected ',' or ']'");
            }
        }
        if (c == '"') return Json(parseString(t, p));
        if (t.compare(p, 4, "true") == 0) { p += 4; return Json(true); }
        if (t.compare(p, 5, "false") == 0) { p += 5; return Json(false); }
        if (t.compare(p, 4, "null") == 0) { p += 4; return Json(); }
        char* end = nullptr;
        double d = std::strtod(t.c_str() + p, &end);
        if (end == t.c_str() + p) throw std::runtime_error("JSON: unexpected character");
        p = (size_t) (end - t.c_str());
        return Json(d);
    }
};

} // namespace hhmg
