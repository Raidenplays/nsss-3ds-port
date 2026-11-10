#include "RegionFile.h"
#include "Nbt.h"
#include <ctime>
#include <sys/stat.h>

namespace {
constexpr int SECTOR = 4096;

int32_t readInt(FILE* f) {
    uint8_t b[4] = {0, 0, 0, 0};
    if (fread(b, 1, 4, f) != 4) return 0;
    return int32_t(uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3]);
}
void writeInt(FILE* f, int32_t v) {
    uint8_t b[4] = {uint8_t(uint32_t(v) >> 24), uint8_t(uint32_t(v) >> 16), uint8_t(uint32_t(v) >> 8), uint8_t(v)};
    fwrite(b, 1, 4, f);
}
bool fileExists(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }
} // namespace

RegionFile::RegionFile(const std::string& p) : path(p) {
    file = fopen(p.c_str(), "r+b");
    if (!file) file = fopen(p.c_str(), "w+b"); // RandomAccessFile "rw" creates it
    if (!file) return;

    if (fileLength() < SECTOR) {
        fseek(file, 0, SEEK_SET);
        for (int i = 0; i < 2048; i++) writeInt(file, 0);
    }
    long len = fileLength();
    if (len & (SECTOR - 1)) { // pad to a whole sector, as Java does
        fseek(file, 0, SEEK_END);
        for (long i = 0; i < (len & (SECTOR - 1)); i++) fputc(0, file);
    }
    int sectors = int(fileLength() / SECTOR);
    sectorFree.assign(size_t(sectors), true);
    sectorFree[0] = false;
    sectorFree[1] = false;

    fseek(file, 0, SEEK_SET);
    for (int i = 0; i < 1024; i++) {
        int32_t o = readInt(file);
        offsets[i] = o;
        if (o != 0 && (o >> 8) + (o & 255) <= sectors)
            for (int s = 0; s < (o & 255); s++) sectorFree[size_t((o >> 8) + s)] = false;
    }
    for (int i = 0; i < 1024; i++) timestamps[i] = readInt(file);
}

RegionFile::~RegionFile() {
    if (file) fclose(file);
}

long RegionFile::fileLength() {
    fseek(file, 0, SEEK_END);
    return ftell(file);
}

bool RegionFile::readChunk(int x, int z, std::vector<uint8_t>& out) {
    if (!file || outOfBounds(x, z)) return false;
    int32_t o = offsets[x + z * 32];
    if (o == 0) return false;
    int sector = o >> 8, count = o & 255;
    if (sector + count > int(sectorFree.size())) return false;
    fseek(file, long(sector) * SECTOR, SEEK_SET);
    int32_t length = readInt(file);
    if (length > SECTOR * count || length <= 0) return false;
    int version = fgetc(file);
    if (version != 1 && version != 2) return false;
    std::vector<uint8_t> comp(size_t(length - 1));
    if (fread(comp.data(), 1, comp.size(), file) != comp.size()) return false;
    return nbt::inflateAny(comp.data(), comp.size(), out);
}

bool RegionFile::writeChunk(int x, int z, const std::vector<uint8_t>& nbtData) {
    if (!file || outOfBounds(x, z)) return false;
    std::vector<uint8_t> data;
    if (!nbt::deflateZlib(nbtData.data(), nbtData.size(), data)) return false;
    const int len = int(data.size());

    int32_t o = offsets[x + z * 32];
    int sector = o >> 8, oldCount = o & 255;
    int needed = (len + 5) / SECTOR + 1;
    if (needed >= 256) return false;

    if (sector != 0 && oldCount == needed) {
        writeSectors(sector, data); // rewrite in place
    } else {
        for (int i = 0; i < oldCount; i++) sectorFree[size_t(sector + i)] = true;
        // First run of free sectors long enough (same scan as Java's indexOf + loop)
        int start = -1, run = 0;
        for (int i = 0; i < int(sectorFree.size()); i++) {
            if (start == -1 && !sectorFree[size_t(i)]) continue;
            if (run != 0) {
                if (sectorFree[size_t(i)]) run++; else run = 0;
            } else if (sectorFree[size_t(i)]) {
                start = i;
                run = 1;
            }
            if (run >= needed) break;
        }
        if (run >= needed) {
            setOffset(x, z, start << 8 | needed);
            for (int i = 0; i < needed; i++) sectorFree[size_t(start + i)] = false;
            writeSectors(start, data);
        } else { // grow the file
            fseek(file, 0, SEEK_END);
            int at = int(sectorFree.size());
            static const uint8_t empty[SECTOR] = {};
            for (int i = 0; i < needed; i++) {
                fwrite(empty, 1, SECTOR, file);
                sectorFree.push_back(false);
            }
            writeSectors(at, data);
            setOffset(x, z, at << 8 | needed);
        }
    }
    setTimestamp(x, z, int32_t(time(nullptr)));
    fflush(file);
    return true;
}

bool RegionFile::writeSectors(int sector, const std::vector<uint8_t>& data) {
    fseek(file, long(sector) * SECTOR, SEEK_SET);
    writeInt(file, int32_t(data.size() + 1));
    fputc(2, file); // zlib
    return fwrite(data.data(), 1, data.size(), file) == data.size();
}

void RegionFile::setOffset(int x, int z, int32_t v) {
    offsets[x + z * 32] = v;
    fseek(file, long(x + z * 32) * 4, SEEK_SET);
    writeInt(file, v);
}

void RegionFile::setTimestamp(int x, int z, int32_t v) {
    timestamps[x + z * 32] = v;
    fseek(file, SECTOR + long(x + z * 32) * 4, SEEK_SET);
    writeInt(file, v);
}

// ------------------------------------------------------------------ cache

RegionFile* RegionCache::get(int chunkX, int chunkZ, bool create) {
    int rx = chunkX >> 5, rz = chunkZ >> 5;
    for (auto& e : open)
        if (e.rx == rx && e.rz == rz) return e.file;
    std::string regionDir = dir + "/region";
    std::string p = regionDir + "/r." + std::to_string(rx) + "." + std::to_string(rz) + ".mcr";
    if (!create && !fileExists(p)) return nullptr;
    mkdir(dir.c_str(), 0777);
    mkdir(regionDir.c_str(), 0777);
    RegionFile* f = new RegionFile(p);
    if (!f->isOpen()) { delete f; return nullptr; }
    if (open.size() >= 8) { delete open.front().file; open.erase(open.begin()); } // tiny LRU
    open.push_back({rx, rz, f});
    return f;
}

void RegionCache::closeAll() {
    for (auto& e : open) delete e.file;
    open.clear();
}
