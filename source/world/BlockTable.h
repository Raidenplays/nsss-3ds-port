#pragma once
// Every block's render data, generated from Axeolon's own Block classes by
// tools/java/DumpBlocks.java + tools/gen_assets.py (BlockTable.cpp).
// Texture indices: 0-255 = terrain.png, 256-511 = Axeolon's terrain2.png.
#include <cstdint>

enum BlockFlags : uint8_t {
    BF_EXISTS = 1, BF_OPAQUE = 2, BF_CUBE = 4, BF_LIQUID = 8, BF_CULL_SAME = 16, BF_LEAVES = 32, BF_SLAB = 64,
};

// Block.getRenderType() values (Beta RenderBlocks)
namespace RenderType { enum { Cube = 0, Cross = 1, Torch = 2, Fire = 3, Liquid = 4, Wire = 5, Crops = 6,
                              Door = 7, Ladder = 8, Rail = 9, Stairs = 10, Fence = 11, Lever = 12,
                              Cactus = 13, Model = 255 /* -1: tile entity renderer */ }; }

struct BlockInfo {
    const char* name;
    uint8_t render;
    uint8_t flags;
    uint8_t pass;      // 0 opaque, 1 translucent (water, ice)
    uint8_t opacity;   // Block.lightOpacity
    uint8_t light;     // Block.lightValue
    float bounds[6];   // minX minY minZ maxX maxY maxZ
    uint16_t tex[16][6]; // [metadata][side] -> atlas index
};

extern const BlockInfo BLOCKS[256];

inline bool blockOpaque(uint8_t id) { return BLOCKS[id].flags & BF_OPAQUE; }
