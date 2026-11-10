#pragma once
// Every block's render data, generated from the NSSS jar's Block classes by tools/nsss/DumpNsss.java
// + tools/gen_assets.py --tables (BlockTable.cpp). Texture indices are terrain.png tiles (0-255).
#include <cstdint>

enum BlockFlags : uint8_t {
    BF_EXISTS = 1, BF_OPAQUE = 2, BF_CUBE = 4, BF_LIQUID = 8, BF_CULL_SAME = 16, BF_LEAVES = 32, BF_SLAB = 64,
};

// Block.getRenderType() values (RenderBlocks.renderBlockByRenderType)
namespace RenderType { enum { Cube = 0, Cross = 1, Torch = 2, Fire = 3, Liquid = 4, Wire = 5, Crops = 6,
                              Door = 7, Ladder = 8, Rail = 9, Stairs = 10, Fence = 11, Lever = 12,
                              Cactus = 13, Gear = 14, Model = 255 /* -1: tile entity renderer */ }; }

struct BlockInfo {
    const char* name;
    uint8_t render;
    uint8_t flags;
    uint8_t pass;      // 0 opaque, 1 translucent (water, ice)
    uint8_t opacity;   // Block.lightOpacity
    uint8_t light;     // Block.lightValue
    uint8_t index;     // Block.blockIndexInTexture
    float bounds[6];   // minX minY minZ maxX maxY maxZ
    int16_t tex[16][6]; // getBlockTextureFromSideAndMetadata [metadata][side] (negative: mirrored, BlockDoor)
};

extern const BlockInfo BLOCKS[256];

inline bool blockOpaque(uint8_t id) { return BLOCKS[id].flags & BF_OPAQUE; }
