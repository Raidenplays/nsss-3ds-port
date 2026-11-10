#pragma once
// Port of Axeolon's Chunk storage: 16 x 256 x 16 (Axeolon raised Beta's 128).
//   blocks[x << 12 | z << 8 | y]                  (Chunk.blocks)
//   data / skylight / blocklight: nibbles, same index, two per byte
//   heightMap[z << 4 | x]: byte, Math.min(height, 255) like Axeolon
// Saved fields this port doesn't use yet (Entities, TileEntities, anything
// Axeolon adds) stay in `level` and are written back untouched.
#include <cstdint>
#include <cstring>
#include "Nbt.h"

class World;

struct NibbleArray {
    uint8_t data[32768];
    inline int get(int x, int y, int z) const {
        int i = x << 12 | z << 8 | y;
        return (i & 1) ? data[i >> 1] >> 4 & 15 : data[i >> 1] & 15;
    }
    inline void set(int x, int y, int z, int v) {
        int i = x << 12 | z << 8 | y;
        uint8_t& b = data[i >> 1];
        b = (i & 1) ? uint8_t((b & 0x0F) | (v & 15) << 4) : uint8_t((b & 0xF0) | (v & 15));
    }
};

class Chunk {
public:
    static constexpr int WIDTH = 16;
    static constexpr int HEIGHT = 256;
    static constexpr int VOLUME = WIDTH * WIDTH * HEIGHT;

    int xPosition, zPosition;
    uint8_t blocks[VOLUME];
    bool regenSkylight = false; // loaded without sky light / height map (ChunkLoader regenerates them)
    NibbleArray data, skylight, blocklight;
    uint8_t heightMap[256];      // as saved (capped at 255)
    bool terrainPopulated = false;
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

    static inline int index(int x, int y, int z) { return x << 12 | z << 8 | y; }

    inline uint8_t getBlockID(int x, int y, int z) const { return blocks[index(x, y, z)]; }
    inline int getMetadata(int x, int y, int z) const { return data.get(x, y, z); }
    inline int getSkyLight(int x, int y, int z) const { return skylight.get(x, y, z); }
    inline int getBlockLight(int x, int y, int z) const { return blocklight.get(x, y, z); }

    // Top of the column: first air above the highest non-air block (our own,
    // uncapped, for the map; the saved heightMap is light-opacity based).
    int topY(int x, int z) const {
        const uint8_t* col = &blocks[x << 12 | z << 8];
        int y = HEIGHT - 1;
        while (y >= 0 && col[y] == 0) y--;
        return y + 1;
    }

    void setBlockID(int x, int y, int z, uint8_t id) { blocks[index(x, y, z)] = id; modified = true; }
    void setMetadata(int x, int y, int z, int m) { data.set(x, y, z, m); modified = true; }
};

namespace ChunkIO {
// ChunkLoader.loadChunkIntoWorldFromCompound: reads a "Level" compound.
// 128-tall chunks (Blocks.length == 32768) are stretched like Chunk.expandBlocks;
// skyFill is the sky light for the new top half (15 overworld, 0 in the Nether).
Chunk* fromLevel(nbt::TagPtr level, int skyFill);
// ChunkLoader.storeChunkInCompound: returns the full root {Level:{...}} to save.
nbt::TagPtr toRoot(Chunk& c, int64_t worldTime);
}
