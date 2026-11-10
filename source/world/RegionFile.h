#pragma once
// Port of Axeolon's RegionFile (the Beta 1.3 McRegion format it adopted):
// region/r.X.Z.mcr holds 32x32 chunks. Header = 1024 offsets (sector << 8 |
// sector count) + 1024 timestamps; each chunk = int length, byte compression
// (1 gzip, 2 zlib), then the compressed NBT. Sectors are 4096 bytes.
// Sector allocation follows RegionFile.write() exactly, so files stay
// compatible with (and look the same as) ones the Java game writes.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class RegionFile {
public:
    explicit RegionFile(const std::string& path);
    ~RegionFile();

    bool isOpen() const { return file != nullptr; }
    bool isChunkSaved(int x, int z) const { return !outOfBounds(x, z) && offsets[x + z * 32] != 0; }

    // Decompressed chunk NBT for local chunk (x, z) in 0..31. False if absent/corrupt.
    bool readChunk(int x, int z, std::vector<uint8_t>& nbtOut);
    // Compresses with zlib (type 2) and stores, like getChunkDataOutputStream + write().
    bool writeChunk(int x, int z, const std::vector<uint8_t>& nbt);

private:
    FILE* file = nullptr;
    std::string path;
    int32_t offsets[1024] = {};
    int32_t timestamps[1024] = {};
    std::vector<bool> sectorFree;

    static bool outOfBounds(int x, int z) { return x < 0 || x >= 32 || z < 0 || z >= 32; }
    bool writeSectors(int sector, const std::vector<uint8_t>& data);
    void setOffset(int x, int z, int32_t v);
    void setTimestamp(int x, int z, int32_t v);
    long fileLength();
};

// Keeps region files open (RegionFileCache equivalent). Small: the 3DS only
// ever touches a handful of regions around the player.
class RegionCache {
public:
    explicit RegionCache(const std::string& dimensionDir) : dir(dimensionDir) {}
    ~RegionCache() { closeAll(); }
    RegionFile* get(int chunkX, int chunkZ, bool create);
    void closeAll();

private:
    struct Entry { int rx, rz; RegionFile* file; };
    std::string dir;
    std::vector<Entry> open;
};
