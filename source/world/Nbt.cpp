#include "Nbt.h"
#include <cstdio>
#include <cstring>
#include <zlib.h>

namespace nbt {

// ---------------------------------------------------------------- helpers

Tag* Tag::get(const char* name) const {
    for (auto& e : entries)
        if (e.first == name) return e.second.get();
    return nullptr;
}

Tag& Tag::set(const char* name, TagPtr value) {
    for (auto& e : entries)
        if (e.first == name) { e.second = std::move(value); return *e.second; }
    entries.emplace_back(name, std::move(value));
    return *entries.back().second;
}

static TagPtr make(Type t) { return TagPtr(new Tag(t)); }
TagPtr makeCompound() { return make(Compound); }
TagPtr makeList(Type elementType) { TagPtr t = make(List); t->listType = elementType; return t; }
TagPtr makeDouble(double v) { TagPtr t = make(Double); t->d = v; return t; }
TagPtr makeFloat(float v) { TagPtr t = make(Float); t->d = v; return t; }

void Tag::setByte(const char* n, int8_t v) { TagPtr t = make(Byte); t->i = v; set(n, std::move(t)); }
void Tag::setShort(const char* n, int16_t v) { TagPtr t = make(Short); t->i = v; set(n, std::move(t)); }
void Tag::setInt(const char* n, int32_t v) { TagPtr t = make(Int); t->i = v; set(n, std::move(t)); }
void Tag::setLong(const char* n, int64_t v) { TagPtr t = make(Long); t->i = v; set(n, std::move(t)); }
void Tag::setFloat(const char* n, float v) { set(n, makeFloat(v)); }
void Tag::setDouble(const char* n, double v) { set(n, makeDouble(v)); }
void Tag::setString(const char* n, const std::string& v) { TagPtr t = make(String); t->str = v; set(n, std::move(t)); }
void Tag::setByteArray(const char* n, const uint8_t* data, size_t len) {
    TagPtr t = make(ByteArray);
    t->bytes.assign(data, data + len);
    set(n, std::move(t));
}
Tag& Tag::setCompound(const char* n) {
    if (Tag* t = getCompound(n)) return *t;
    return set(n, makeCompound());
}

// ---------------------------------------------------------------- reading

namespace {

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    bool ok = true;

    bool need(size_t n) { if (size_t(end - p) < n) ok = false; return ok; }
    uint8_t u8() { if (!need(1)) return 0; return *p++; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = uint16_t(p[0] << 8 | p[1]); p += 2; return v; }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        p += 4;
        return v;
    }
    uint64_t u64() { uint64_t hi = u32(); return hi << 32 | u32(); }
    std::string utf() {
        uint16_t len = u16();
        if (!need(len)) return std::string();
        std::string s(reinterpret_cast<const char*>(p), len);
        p += len;
        return s;
    }
};

bool readPayload(Reader& r, Tag& t, int depth);

TagPtr readTyped(Reader& r, Type type, int depth) {
    TagPtr t = make(type);
    if (!readPayload(r, *t, depth)) return nullptr;
    return t;
}

bool readPayload(Reader& r, Tag& t, int depth) {
    if (depth > 512) return false; // malicious nesting guard
    switch (t.type) {
    case Byte: t.i = int8_t(r.u8()); break;
    case Short: t.i = int16_t(r.u16()); break;
    case Int: t.i = int32_t(r.u32()); break;
    case Long: t.i = int64_t(r.u64()); break;
    case Float: { uint32_t b = r.u32(); float f; std::memcpy(&f, &b, 4); t.d = f; break; }
    case Double: { uint64_t b = r.u64(); double f; std::memcpy(&f, &b, 8); t.d = f; break; }
    case ByteArray: {
        int32_t len = int32_t(r.u32());
        if (len < 0 || !r.need(size_t(len))) return false;
        t.bytes.assign(r.p, r.p + len);
        r.p += len;
        break;
    }
    case String: t.str = r.utf(); break;
    case List: {
        t.listType = Type(r.u8());
        int32_t n = int32_t(r.u32());
        if (n < 0 || t.listType > Compound) return false;
        for (int32_t k = 0; k < n && r.ok; k++) {
            TagPtr e = readTyped(r, t.listType, depth + 1);
            if (!e) return false;
            t.items.push_back(std::move(e));
        }
        break;
    }
    case Compound:
        for (;;) {
            Type ct = Type(r.u8());
            if (!r.ok) return false;
            if (ct == End) break;
            if (ct > Compound) return false;
            std::string name = r.utf();
            TagPtr e = readTyped(r, ct, depth + 1);
            if (!e) return false;
            t.entries.emplace_back(std::move(name), std::move(e));
        }
        break;
    default: return false;
    }
    return r.ok;
}

// ---------------------------------------------------------------- writing

struct Writer {
    std::vector<uint8_t>& o;
    void u8(uint8_t v) { o.push_back(v); }
    void u16(uint16_t v) { o.push_back(uint8_t(v >> 8)); o.push_back(uint8_t(v)); }
    void u32(uint32_t v) { for (int s = 24; s >= 0; s -= 8) o.push_back(uint8_t(v >> s)); }
    void u64(uint64_t v) { u32(uint32_t(v >> 32)); u32(uint32_t(v)); }
    void utf(const std::string& s) { u16(uint16_t(s.size())); o.insert(o.end(), s.begin(), s.end()); }

    void payload(const Tag& t) {
        switch (t.type) {
        case Byte: u8(uint8_t(t.i)); break;
        case Short: u16(uint16_t(t.i)); break;
        case Int: u32(uint32_t(t.i)); break;
        case Long: u64(uint64_t(t.i)); break;
        case Float: { float f = float(t.d); uint32_t b; std::memcpy(&b, &f, 4); u32(b); break; }
        case Double: { uint64_t b; std::memcpy(&b, &t.d, 8); u64(b); break; }
        case ByteArray: u32(uint32_t(t.bytes.size())); o.insert(o.end(), t.bytes.begin(), t.bytes.end()); break;
        case String: utf(t.str); break;
        case List:
            // NBTTagList.writeTagContents: element type of the first item, Byte if empty
            u8(t.items.empty() ? uint8_t(Byte) : uint8_t(t.items[0]->type));
            u32(uint32_t(t.items.size()));
            for (auto& e : t.items) payload(*e);
            break;
        case Compound:
            for (auto& e : t.entries) {
                u8(e.second->type);
                utf(e.first);
                payload(*e.second);
            }
            u8(End);
            break;
        default: break;
        }
    }
};

} // namespace

TagPtr readRoot(const uint8_t* data, size_t len, std::string* rootName) {
    Reader r{data, data + len};
    if (Type(r.u8()) != Compound) return nullptr;
    std::string name = r.utf();
    if (rootName) *rootName = name;
    return readTyped(r, Compound, 0);
}

void writeRoot(const Tag& root, std::vector<uint8_t>& out, const std::string& rootName) {
    Writer w{out};
    w.u8(Compound);
    w.utf(rootName);
    w.payload(root);
}

// ---------------------------------------------------------------- compression

bool inflateAny(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out) {
    z_stream s{};
    if (inflateInit2(&s, 32 + MAX_WBITS) != Z_OK) return false; // 32 = auto gzip/zlib
    s.next_in = const_cast<Bytef*>(in);
    s.avail_in = uInt(inLen);
    out.clear();
    uint8_t buf[16384];
    int rc;
    do {
        s.next_out = buf;
        s.avail_out = sizeof buf;
        rc = inflate(&s, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) { inflateEnd(&s); return false; }
        out.insert(out.end(), buf, buf + (sizeof buf - s.avail_out));
    } while (rc != Z_STREAM_END && (s.avail_in > 0 || s.avail_out == 0));
    inflateEnd(&s);
    return rc == Z_STREAM_END;
}

static bool deflateWith(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out, int windowBits) {
    z_stream s{};
    if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
    s.next_in = const_cast<Bytef*>(in);
    s.avail_in = uInt(inLen);
    out.resize(deflateBound(&s, uLong(inLen)) + 32);
    s.next_out = out.data();
    s.avail_out = uInt(out.size());
    int rc = deflate(&s, Z_FINISH);
    out.resize(out.size() - s.avail_out);
    deflateEnd(&s);
    return rc == Z_STREAM_END;
}

bool deflateZlib(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out) { return deflateWith(in, inLen, out, MAX_WBITS); }
bool deflateGzip(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out) { return deflateWith(in, inLen, out, 16 + MAX_WBITS); }

// ---------------------------------------------------------------- files

static bool readWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize(size_t(n));
    bool ok = n == 0 || fread(out.data(), 1, size_t(n), f) == size_t(n);
    fclose(f);
    return ok;
}

TagPtr readGzipFile(const std::string& path) {
    std::vector<uint8_t> raw, data;
    if (!readWholeFile(path, raw) || !inflateAny(raw.data(), raw.size(), data)) return nullptr;
    return readRoot(data.data(), data.size());
}

bool writeGzipFile(const std::string& path, const Tag& root) {
    std::vector<uint8_t> data, gz;
    writeRoot(root, data);
    if (!deflateGzip(data.data(), data.size(), gz)) return false;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(gz.data(), 1, gz.size(), f) == gz.size();
    return fclose(f) == 0 && ok;
}

} // namespace nbt

namespace nbt {
TagPtr clone(const Tag& t) {
    TagPtr c(new Tag(t.type));
    c->i = t.i;
    c->d = t.d;
    c->bytes = t.bytes;
    c->str = t.str;
    c->listType = t.listType;
    for (auto& it : t.items) c->items.push_back(it ? clone(*it) : nullptr);
    for (auto& e : t.entries) c->entries.emplace_back(e.first, e.second ? clone(*e.second) : nullptr);
    return c;
}
} // namespace nbt
