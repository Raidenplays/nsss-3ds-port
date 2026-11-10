#pragma once
// The jar's Chunk storage: 16 x 128 x 16.
//   blocks[x << 11 | z << 7 | y]                  (Chunk.blocks)
//   data / skylight / blocklight: nibbles, same index, two per byte
//   heightMap[z << 4 | x]: byte
// Saved fields this port keeps elsewhere or doesn't use (Entities, TileEntities, Frozen, Snowy)
// stay in `level` and are written back untouched.
#include <cstdint>
#include <cstring>
#include "Nbt.h"

class World;

struct NibbleArray {
    uint8_t data[16384];
    inline int get(int x, int y, int z) const {
        int i = x << 11 | z << 7 | y;
        return (i & 1) ? data[i >> 1] >> 4 & 15 : data[i >> 1] & 15;
    }
    inline void set(int x, int y, int z, int v) {
        int i = x << 11 | z << 7 | y;
        uint8_t& b = data[i >> 1];
        b = (i & 1) ? uint8_t((b & 0x0F) | (v & 15) << 4) : uint8_t((b & 0xF0) | (v & 15));
    }
};

class Chunk {
public:
    static constexpr int WIDTH = 16;
    static constexpr int HEIGHT = 128;
    static constexpr int VOLUME = WIDTH * WIDTH * HEIGHT;

    int xPosition, zPosition;
    uint8_t blocks[VOLUME];
    bool regenSkylight = false; // loaded without sky light / height map (ChunkLoader regenerates them)
    NibbleArray data, skylight, blocklight;
    uint8_t heightMap[256];
    bool terrainPopulated = false;
    bool frozen = false, snowy = false; // the season's ice and snow are on it (Chunk.setFrozen / setSnowy)
    int lowestHeight = 0;        // field_1532_i
    bool modified = false;       // needs saving
    World* world = nullptr;
    nbt::TagPtr level;           // the "Level" compound this chunk came from (for round-tripping)

    Chunk(int cx, int cz) : xPosition(cx), zPosition(cz) {
        std::memset(blocks, 0, sizeof blocks);
        std::memset(&data, 0, sizeof data);
        std::memset(&skylight, 0, sizeof skylight);
        std::memset(&blocklight, 0, sizeof blocklight);
        std::memset(heightMap, 0, sizeof heightMap);
    }

    static inline int index(int x, int y, int z) { return x << 11 | z << 7 | y; }
    static inline int column(int x, int z) { return x << 11 | z << 7; }

    inline uint8_t getBlockID(int x, int y, int z) const { return blocks[index(x, y, z)]; }
    inline int getMetadata(int x, int y, int z) const { return data.get(x, y, z); }
    inline int getSkyLight(int x, int y, int z) const { return skylight.get(x, y, z); }
    inline int getBlockLight(int x, int y, int z) const { return blocklight.get(x, y, z); }

    // Top of the column: first air above the highest non-air block (our own,
    // uncapped, for the map; the saved heightMap is light-opacity based).
    int topY(int x, int z) const {
        const uint8_t* col = &blocks[column(x, z)];
        int y = HEIGHT - 1;
        while (y >= 0 && col[y] == 0) y--;
        return y + 1;
    }

    void setBlockID(int x, int y, int z, uint8_t id) { blocks[index(x, y, z)] = id; modified = true; }
    void setMetadata(int x, int y, int z, int m) { data.set(x, y, z, m); modified = true; }
};

namespace ChunkIO {
// ChunkLoader.loadChunkIntoWorldFromCompound: reads a "Level" compound.
Chunk* fromLevel(nbt::TagPtr level);
// ChunkLoader.storeChunkInCompound: returns the full root {Level:{...}} to save.
nbt::TagPtr toRoot(Chunk& c, int64_t worldTime);
}
