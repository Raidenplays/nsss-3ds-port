#pragma once
// Named Binary Tag reader/writer, compatible with b1.2's NBTBase family
// (NBTTagByte ... NBTTagCompound). Big-endian, Java modified-UTF-8 strings
// (kept as raw bytes, so any name round-trips unchanged).
//
// Compounds keep their entries in file order, and tags this port doesn't
// understand yet are kept as-is, so saving a world writes them back untouched.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nbt {

enum Type : uint8_t {
    End = 0, Byte = 1, Short = 2, Int = 3, Long = 4, Float = 5, Double = 6,
    ByteArray = 7, String = 8, List = 9, Compound = 10,
};

struct Tag;
using TagPtr = std::unique_ptr<Tag>;

struct Tag {
    Type type = End;
    int64_t i = 0;               // Byte/Short/Int/Long
    double d = 0;                // Float/Double
    std::vector<uint8_t> bytes;  // ByteArray
    std::string str;             // String
    Type listType = Byte;        // List element type (b1.2 writes Byte for empty lists)
    std::vector<TagPtr> items;   // List elements
    std::vector<std::pair<std::string, TagPtr>> entries; // Compound

    explicit Tag(Type t = End) : type(t) {}

    // ---- compound helpers (mirror NBTTagCompound's getters: missing -> 0/empty) ----
    Tag* get(const char* name) const;
    bool has(const char* name) const { return get(name) != nullptr; }
    int8_t getByte(const char* n) const { const Tag* t = get(n); return t ? int8_t(t->i) : 0; }
    int16_t getShort(const char* n) const { const Tag* t = get(n); return t ? int16_t(t->i) : 0; }
    int32_t getInt(const char* n) const { const Tag* t = get(n); return t ? int32_t(t->i) : 0; }
    int64_t getLong(const char* n) const { const Tag* t = get(n); return t ? t->i : 0; }
    float getFloat(const char* n) const { const Tag* t = get(n); return t ? float(t->d) : 0.0f; }
    double getDouble(const char* n) const { const Tag* t = get(n); return t ? t->d : 0.0; }
    bool getBool(const char* n) const { return getByte(n) != 0; }
    std::string getString(const char* n) const { const Tag* t = get(n); return t ? t->str : std::string(); }
    const std::vector<uint8_t>* getByteArray(const char* n) const {
        const Tag* t = get(n);
        return (t && t->type == ByteArray) ? &t->bytes : nullptr;
    }
    Tag* getCompound(const char* n) const { Tag* t = get(n); return (t && t->type == Compound) ? t : nullptr; }
    Tag* getList(const char* n) const { Tag* t = get(n); return (t && t->type == List) ? t : nullptr; }

    // Setters replace an existing entry in place (keeping its position) or append.
    Tag& set(const char* name, TagPtr value);
    void setByte(const char* n, int8_t v);
    void setShort(const char* n, int16_t v);
    void setInt(const char* n, int32_t v);
    void setLong(const char* n, int64_t v);
    void setFloat(const char* n, float v);
    void setDouble(const char* n, double v);
    void setBool(const char* n, bool v) { setByte(n, v ? 1 : 0); }
    void setString(const char* n, const std::string& v);
    void setByteArray(const char* n, const uint8_t* data, size_t len);
    Tag& setCompound(const char* n); // returns the (new or existing) compound
};

TagPtr makeCompound();
TagPtr makeList(Type elementType);
TagPtr makeDouble(double v);
TagPtr makeFloat(float v);
TagPtr clone(const Tag& t); // deep copy

// Raw (uncompressed) NBT, like CompressedStreamTools.func_1141_a / func_1139_a.
// The root must be a Compound. Returns null on malformed data.
TagPtr readRoot(const uint8_t* data, size_t len, std::string* rootName = nullptr);
void writeRoot(const Tag& root, std::vector<uint8_t>& out, const std::string& rootName = "");

// Compression helpers (zlib). inflateAny auto-detects gzip or zlib headers.
bool inflateAny(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);
bool deflateZlib(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);
bool deflateGzip(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);

// Gzipped NBT files (level.dat, legacy c.X.Z.dat), like func_1138_a / writeGzipped...
TagPtr readGzipFile(const std::string& path);
bool writeGzipFile(const std::string& path, const Tag& root);

} // namespace nbt
