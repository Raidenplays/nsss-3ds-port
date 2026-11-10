#pragma once
// Platform-independent mesher — the 3DS stand-in for Beta's RenderBlocks +
// Tessellator. One mesh per 16x16x16 section (Beta's WorldRenderer).
//
// Old 3DS budget: 12-byte vertices. Positions are 1/64-block units relative
// to the section (0..1024); the shader adds the section origin.
#include <cstdint>
#include <vector>

class World;

struct BlockVertex {
    int16_t x, y, z;  // 1/64 block, section-relative
    int16_t shade;    // 0..255: Beta face factor x light brightness
    int16_t u, v;     // quarter-texel coords in the 256x1024 atlas, v counted from the bottom
};
static_assert(sizeof(BlockVertex) == 12, "BlockVertex must stay 12 bytes");

struct MeshData {
    std::vector<BlockVertex> vertices;
    std::vector<uint16_t> indices;
    // Indices grouped by facing (Beta order: -Y +Y -Z +Z -X +X), plus group 6
    // for faces that can face anywhere (plants, rotated quads). The renderer
    // skips groups that can't face the camera.
    uint32_t faceStart[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

namespace ChunkMesher {
// Beta/Axeolon light brightness table (WorldProvider.lightBrightnessTable at the
// default Brightness setting), exported from the Java game.
extern const float LIGHT_BRIGHTNESS[16];
constexpr int ATLAS_W = 256, ATLAS_H = 1024; // terrain.png, terrain2.png, then the block model textures

// Builds geometry for section (cx, sy, cz): blocks cx*16.., sy*16.., cz*16..
void buildSection(const World& world, int cx, int sy, int cz, MeshData& out);
}
