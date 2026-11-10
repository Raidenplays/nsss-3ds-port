#pragma once
// Platform-independent mesher: the NSSS jar's RenderBlocks and Tessellator. One mesh per
// 16x16x16 section (the jar's WorldRenderer).
//
// Old 3DS budget: 12-byte vertices. Positions are 1/64-block units relative
// to the section (0..1024); the shader adds the section origin.
#include <cstdint>
#include <functional>
#include <vector>

class World;

struct BlockVertex {
    int16_t x, y, z;  // 1/64 block, section-relative
    int16_t shade;    // face factor in twentieths << 8 | the light byte (sky << 4 | block)
    int16_t u, v;     // quarter-texel coords in the 256x1024 atlas, v counted from the bottom
};
static_assert(sizeof(BlockVertex) == 12, "BlockVertex must stay 12 bytes");

struct MeshData {
    std::vector<BlockVertex> vertices;
    std::vector<uint16_t> indices;
    // Indices grouped by facing (-Y +Y -Z +Z -X +X), plus group 6
    // for faces that can face anywhere (plants, rotated quads). The renderer
    // skips groups that can't face the camera.
    uint32_t faceStart[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

namespace ChunkMesher {
// World.field_1042_i: the brightness of each light level
extern const float LIGHT_BRIGHTNESS[16];
constexpr int ATLAS_W = 256, ATLAS_H = 256; // terrain.png

// Builds geometry for section (cx, sy, cz): blocks cx*16.., sy*16.., cz*16..
void buildSection(const World& world, int cx, int sy, int cz, MeshData& out);

// For the render parity test: when set, quads go here instead of into the mesh - the block they came
// from, four vertices (world x y z, u v as the jar's Tessellator has them), the face factor and the
// light byte the brightness comes from (0x0F: full brightness).
extern std::function<void(int x, int y, int z, const float q[4][5], float factor, int light)> recordQuad;
}
