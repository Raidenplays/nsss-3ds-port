// WorldRenderer.updateRenderer + RenderBlocks, ported from the NSSS jar.
//
// Every block goes through renderBlockByRenderType as in the jar, in world coordinates, its quads
// written through a Tessellator stand-in (Tess) into the section's mesh: positions in 1/64 blocks
// relative to the section, texture coordinates in quarter texels of the atlas, and per quad the face
// factor and the light of the cell the jar reads its brightness from (the shader turns that into
// World.getBrightness with skylightSubtracted). tools/host/blocksim.cpp ("render") compares the quads
// with the jar's own Tessellator output.
#include "ChunkMesh.h"
#include "world/World.h"
#include "world/Chunk.h"
#include "world/BlockTable.h"
#include "world/BlockProps.h"
#include "world/Blocks.h"
#include "world/MathHelper.h"
#include <algorithm>
#include <cmath>
#include <cstring>

const float ChunkMesher::LIGHT_BRIGHTNESS[16] = {
    0.05f, 0.06666666f, 0.085185185f, 0.10588235f, 0.12916666f, 0.15555558f, 0.18571426f, 0.22051284f,
    0.26111114f, 0.30909094f, 0.36666667f, 0.43703708f, 0.52500004f, 0.63809526f, 0.7888889f, 1.0f,
};

std::function<void(int, int, int, const float[4][5], float, int)> ChunkMesher::recordQuad;

namespace {

// ---------------------------------------------------------------- the ChunkCache
// The section plus a 2-block border (BlockChest looks two blocks along), read once from the World so
// the port itself only does array reads. Accessors take world coordinates.
constexpr int B = 2, P = 16 + 2 * B;
constexpr int WORLD_H = 128; // ChunkCache: no blocks below 0 or from 128 up
struct Padded {
    uint8_t id[P * P * P];
    uint8_t meta[P * P * P];
    uint8_t light[P * P * P]; // sky << 4 | block, with ChunkCache's slab/farmland rule applied
};
Padded pad;
uint8_t rawLight[P * P * P];
int ox, oy, oz;    // world position of pad cell 0
int secX, secY, secZ;
inline int pidx(int x, int y, int z) { return ((y - oy) * P + (z - oz)) * P + (x - ox); }
inline int bid(int x, int y, int z) { return pad.id[pidx(x, y, z)]; }
inline int bmeta(int x, int y, int z) { return pad.meta[pidx(x, y, z)]; }
inline Mat bmat(int x, int y, int z) { return BP[bid(x, y, z)].mat; }       // getMaterialXYZ
inline bool opq(int id) { return BP[id].opaqueLookup; }                      // Block.opaqueCubeLookup
inline bool normalCube(int x, int y, int z) { return BP[bid(x, y, z)].opaqueCube; } // isBlockNormalCube

// The brightness the jar reads (getBrightness of a cell): a light byte, sky << 4 | block. The shader
// computes max(sky - skylightSubtracted, block) and looks it up; FULL is brightness 1.0 whatever the sky.
using Light = int;
constexpr Light FULL = 0x0F;
inline Light lightAt(int x, int y, int z) { return pad.light[pidx(x, y, z)]; }
// max(getBrightness(a), getBrightness(b)) as one byte: max commutes with the shader's formula
inline Light lightMax(Light a, Light b) { return std::max(a & 0xF0, b & 0xF0) | std::max(a & 15, b & 15); }

// ---------------------------------------------------------------- output
std::vector<uint16_t> groups[7];
MeshData* out;
bool full;

struct V { int16_t x, y, z, u, v; };

void emitQuad(const V q[4], int shade, int group) {
    if (out->vertices.size() + 4 > 65535) { full = true; return; }
    uint16_t base = uint16_t(out->vertices.size());
    for (int i = 0; i < 4; i++) out->vertices.push_back({q[i].x, q[i].y, q[i].z, int16_t(shade), q[i].u, q[i].v});
    std::vector<uint16_t>& g = groups[group];
    const uint16_t idx[6] = {base, uint16_t(base + 1), uint16_t(base + 2), base, uint16_t(base + 2), uint16_t(base + 3)};
    g.insert(g.end(), idx, idx + 6);
}

// The vertex's w: face factor in twentieths, sky and block light (see vshader.v.pica)
inline int shadeFor(float factor, int light) {
    int q = int(factor * 20.0f + 0.5f);
    if (q > 20) q = 20;
    if (q < 0) q = 0;
    return q * 256 + (light & 255);
}

int curX, curY, curZ; // the block being rendered (for recordQuad)

// Tessellator: setColorOpaque_F (a face factor times the brightness of a cell), setTranslationF and
// addVertexWithUV, four vertices to a quad
struct Tess {
    float factor = 1.0f;
    Light light = 0;
    double tx = 0, ty = 0, tz = 0;
    float q[4][5];
    int n = 0;
    void color(float f, Light l) { factor = f; light = l; }
    void v(double x, double y, double z, double u, double vv) {
        float* p = q[n];
        p[0] = float(x + tx); p[1] = float(y + ty); p[2] = float(z + tz); p[3] = float(u); p[4] = float(vv);
        if (++n == 4) { n = 0; flush(); }
    }
    void flush();
};
Tess T;

void Tess::flush() {
    if (ChunkMesher::recordQuad) { ChunkMesher::recordQuad(curX, curY, curZ, q, factor, light); return; }
    V vq[4];
    for (int k = 0; k < 4; ++k) {
        vq[k].x = int16_t(std::lround((double(q[k][0]) - secX) * 64.0));
        vq[k].y = int16_t(std::lround((double(q[k][1]) - secY) * 64.0));
        vq[k].z = int16_t(std::lround((double(q[k][2]) - secZ) * 64.0));
        vq[k].u = int16_t(std::lround(q[k][3] * 256.0f * 4.0f));
        vq[k].v = int16_t(std::lround((ChunkMesher::ATLAS_H - q[k][4] * 256.0f) * 4.0f));
    }
    // which way it faces, for the renderer's per-direction culling
    float ax = q[1][0] - q[0][0], ay = q[1][1] - q[0][1], az = q[1][2] - q[0][2];
    float bx = q[2][0] - q[0][0], by = q[2][1] - q[0][1], bz = q[2][2] - q[0][2];
    float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
    const float e = 1e-6f;
    int group = 6;
    if (std::fabs(nx) < e && std::fabs(nz) < e && std::fabs(ny) > e) group = ny < 0 ? 0 : 1;
    else if (std::fabs(nx) < e && std::fabs(ny) < e && std::fabs(nz) > e) group = nz < 0 ? 2 : 3;
    else if (std::fabs(ny) < e && std::fabs(nz) < e && std::fabs(nx) > e) group = nx < 0 ? 4 : 5;
    emitQuad(vq, shadeFor(factor, light), group);
}

// ---------------------------------------------------------------- the block being drawn
// Block bounds (minX..maxZ) are state on the jar's Block objects: setBlockBounds changes them for the
// rest of the call. Kept here for the current render call, starting from the block's own.
struct Bounds { double minX, minY, minZ, maxX, maxY, maxZ; };
Bounds bb;
int overrideTex = -1; // overrideBlockTexture
bool flipTex = false; // flipTexture
inline void setBounds(float a, float b, float c, float d, float e, float f) {
    bb = {double(a), double(b), double(c), double(d), double(e), double(f)};
}
inline bool lit(int id) { return BLOCKS[id].light > 0; } // Block.lightValue > 0

int texSM(int id, int side, int meta) { return BLOCKS[id].tex[meta & 15][side]; } // getBlockTextureFromSideAndMetadata
int texIndex(int id, int side) { return texSM(id, side, 0); }                       // getTextureIndex(side)

constexpr int CHEST = 54;
#define INDEX (BLOCKS[CHEST].index)

int chestTexture(int x, int y, int z, int meta) { // BlockChest.getTextureIndex(IBlockAccess, ...)
    int leftHalf = 57;
    int rightHalf = 58;
    int middle = 30;
    if (meta == 1) {
        return INDEX - 1;
    } else if (meta == 0) {
        return INDEX - 1;
    } else {
        int blockEast = bid(x, y, z - 1);
        int blockWest = bid(x, y, z + 1);
        int blockNorth = bid(x - 1, y, z);
        int blockSouth = bid(x + 1, y, z);
        int blockEaster = bid(x, y, z - 2);
        int blockWester = bid(x, y, z + 2);
        int blockNorther = bid(x - 2, y, z);
        int blockSouther = bid(x + 2, y, z);
        if (blockWest == CHEST && blockWester == CHEST) {
            if (meta == 2) {
                return INDEX;
            }

            if (meta == 3) {
                return INDEX;
            }

            if (meta == 4) {
                return leftHalf;
            }

            if (meta == 5) {
                return rightHalf;
            }
        }

        if (blockNorth == CHEST && blockNorther == CHEST) {
            if (meta == 2) {
                return leftHalf;
            }

            if (meta == 3) {
                return rightHalf;
            }

            if (meta == 4) {
                return INDEX;
            }

            if (meta == 5) {
                return INDEX;
            }
        }

        if (blockEast == CHEST && blockEaster == CHEST) {
            if (meta == 2) {
                return INDEX;
            }

            if (meta == 3) {
                return INDEX;
            }

            if (meta == 4) {
                return rightHalf;
            }

            if (meta == 5) {
                return leftHalf;
            }
        }

        if (blockSouth == CHEST && blockSouther == CHEST) {
            if (meta == 2) {
                return rightHalf;
            }

            if (meta == 3) {
                return leftHalf;
            }

            if (meta == 4) {
                return INDEX;
            }

            if (meta == 5) {
                return INDEX;
            }
        }

        if (blockEast == CHEST && blockWest == CHEST) {
            int byte0 = 3;
            if (opq(blockEast) && !opq(blockWest)) {
                byte0 = 3;
            }

            if (opq(blockWest) && !opq(blockEast)) {
                byte0 = 2;
            }

            if (opq(blockNorth) && !opq(blockSouth)) {
                byte0 = 5;
            }

            if (opq(blockSouth) && !opq(blockNorth)) {
                byte0 = 4;
            }

            return meta != byte0 ? INDEX : middle;
        } else if (blockNorth == CHEST && blockSouth == CHEST) {
            int byte0x = 3;
            if (opq(blockEast) && !opq(blockWest)) {
                byte0x = 3;
            }

            if (opq(blockWest) && !opq(blockEast)) {
                byte0x = 2;
            }

            if (opq(blockNorth) && !opq(blockSouth)) {
                byte0x = 5;
            }

            if (opq(blockSouth) && !opq(blockNorth)) {
                byte0x = 4;
            }

            return meta != byte0x ? INDEX : middle;
        } else if (blockEast != CHEST && blockWest != CHEST) {
            if (blockNorth != CHEST && blockSouth != CHEST) {
                int byte0xx = 3;
                if (opq(blockEast) && !opq(blockWest)) {
                    byte0xx = 3;
                }

                if (opq(blockWest) && !opq(blockEast)) {
                    byte0xx = 2;
                }

                if (opq(blockNorth) && !opq(blockSouth)) {
                    byte0xx = 5;
                }

                if (opq(blockSouth) && !opq(blockNorth)) {
                    byte0xx = 4;
                }

                return meta != byte0xx ? INDEX : INDEX + 1;
            } else if (meta != 4 && meta != 5) {
                int j2 = 0;
                if (blockNorth == CHEST) {
                    j2 = -1;
                }

                int l2 = bid(blockNorth != CHEST ? x + 1 : x - 1, y, z - 1);
                int j3 = bid(blockNorth != CHEST ? x + 1 : x - 1, y, z + 1);
                if (meta == 3) {
                    j2 = -1 - j2;
                }

                int byte2 = 3;
                if ((opq(blockEast) || opq(l2)) && !opq(blockWest) && !opq(j3)) {
                    byte2 = 3;
                }

                if ((opq(blockWest) || opq(j3)) && !opq(blockEast) && !opq(l2)) {
                    byte2 = 2;
                }

                return (meta != byte2 ? INDEX + 32 : INDEX + 16) + j2;
            } else {
                return INDEX;
            }
        } else if (meta != 2 && meta != 3) {
            int i2 = 0;
            if (blockEast == CHEST) {
                i2 = -1;
            }

            int k2 = bid(x - 1, y, blockEast != CHEST ? z + 1 : z - 1);
            int i3 = bid(x + 1, y, blockEast != CHEST ? z + 1 : z - 1);
            if (meta == 4) {
                i2 = -1 - i2;
            }

            int byte1 = 5;
            if ((opq(blockNorth) || opq(k2)) && !opq(blockSouth) && !opq(i3)) {
                byte1 = 5;
            }

            if ((opq(blockSouth) || opq(i3)) && !opq(blockNorth) && !opq(k2)) {
                byte1 = 4;
            }

            return (meta != byte1 ? INDEX + 32 : INDEX + 16) + i2;
        } else {
            return INDEX;
        }
    }
}

#undef INDEX

// ---------------------------------------------------------------- BlockFluids
bool shouldSide(int id, int x, int y, int z, int face);

// getIfSameMaterialAndHeight
int fluidDepth(Mat m, int x, int y, int z) {
    if (bmat(x, y, z) != m) return -1;
    int l = bmeta(x, y, z);
    return l >= 8 ? 0 : l;
}
// BlockFluids.shouldSideBeRendered with the fluid's own (full) bounds
bool fluidSide(Mat m, int x, int y, int z, int face) {
    Mat n = bmat(x, y, z);
    if (n == m) return false;
    if (n == Mat::ice) return false;
    if (face == 1) return true;
    return !normalCube(x, y, z);
}
struct Vec { double x, y, z; };
Vec normalize(Vec v) { // Vec3D.normalize (the length through MathHelper.sqrt_double, a float)
    double d = double(MathHelper::sqrt_double(v.x * v.x + v.y * v.y + v.z * v.z));
    return d < 1.0E-4 ? Vec{0, 0, 0} : Vec{v.x / d, v.y / d, v.z / d};
}
// getFlowDirectionVector
Vec flowVector(Mat m, int x, int y, int z) {
    Vec v{0, 0, 0};
    int c = fluidDepth(m, x, y, z);
    for (int s = 0; s < 4; ++s) {
        int sx = x, sz = z;
        if (s == 0) sx = x - 1;
        if (s == 1) sz = z - 1;
        if (s == 2) ++sx;
        if (s == 3) ++sz;
        int d = fluidDepth(m, sx, y, sz);
        if (d < 0) {
            if (!matSolid(bmat(sx, y, sz))) { // blocksMovement
                d = fluidDepth(m, sx, y - 1, sz);
                if (d >= 0) {
                    int h = d - (c - 8);
                    v.x += double((sx - x) * h);
                    v.z += double((sz - z) * h);
                }
            }
        } else {
            int h = d - c;
            v.x += double((sx - x) * h);
            v.z += double((sz - z) * h);
        }
    }
    if (bmeta(x, y, z) >= 8) {
        bool flag = fluidSide(m, x, y, z - 1, 2) || fluidSide(m, x, y, z + 1, 3) || fluidSide(m, x - 1, y, z, 4) ||
                    fluidSide(m, x + 1, y, z, 5) || fluidSide(m, x, y + 1, z - 1, 2) || fluidSide(m, x, y + 1, z + 1, 3) ||
                    fluidSide(m, x - 1, y + 1, z, 4) || fluidSide(m, x + 1, y + 1, z, 5);
        if (flag) {
            v = normalize(v);
            v.y += -6.0;
        }
    }
    return normalize(v);
}
// isFlowing: through waterStill's flow vector, or lavaStill's for lava
bool fluidFlowing(int x, int y, int z) {
    int id = bid(x, y, z);
    Vec v = flowVector(id == 10 || id == 11 ? Mat::lava : Mat::water, x, y, z);
    return v.x != 0.0 || v.y != 0.0 || v.z != 0.0;
}
// getFlowDirectionRadians
double flowRadians(Mat m, int x, int y, int z) {
    Vec v = flowVector(m, x, y, z);
    return v.x == 0.0 && v.z == 0.0 ? -1000.0 : std::atan2(v.z, v.x) - 1.5707963267948966;
}
float fluidLevel(int i) { // getFluidLevel
    if (i >= 8) i = 0;
    return float(i + 1) / 9.0f;
}

// ---------------------------------------------------------------- per-block hooks

// Block.getTextureIndex(IBlockAccess, x, y, z, side) and its overrides
int texAt(int id, int x, int y, int z, int side) {
    switch (id) {
    case 2: { // BlockGrass
        if (side == 1) return 0;
        if (side == 0) return 2;
        Mat m = bmat(x, y + 1, z);
        return m != Mat::snow && m != Mat::builtSnow ? 3 : 68;
    }
    case 54: return chestTexture(x, y, z, side);
    case 61: case 62: { // BlockFurnace
        if (side == 1 || side == 0) return BLOCKS[1].index;
        if (side != bmeta(x, y, z)) return BLOCKS[id].index;
        return id == 62 ? BLOCKS[id].index + 16 : BLOCKS[id].index - 1;
    }
    case 18: return BLOCKS[18].index; // BlockLeaves: 105 (setFastOrFancy), the tile TextureLeavesFX paints
    case 53: return texAt(5, x, y, z, side);          // BlockStairs: its modelBlock's
    case 67: return texAt(4, x, y, z, side);
    case 8: case 9: case 10: case 11: return fluidFlowing(x, y, z) ? BLOCKS[id].index + 1 : BLOCKS[id].index;
    default: return texSM(id, side, bmeta(x, y, z));
    }
}

// Block.setBlockBoundsBasedOnState and its overrides
void boundsFor(int id, int x, int y, int z) {
    const float* b0 = BLOCKS[id].bounds;
    setBounds(b0[0], b0[1], b0[2], b0[3], b0[4], b0[5]);
    switch (id) {
    case 77: { // BlockButton
        int l = bmeta(x, y, z), i1 = l & 7;
        bool flag = (l & 8) > 0;
        float f = 0.375f, f1 = 0.625f, f2 = 0.1875f, f3 = flag ? 0.0625f : 0.125f;
        if (i1 == 1) setBounds(0.0f, f, 0.5f - f2, f3, f1, 0.5f + f2);
        else if (i1 == 2) setBounds(1.0f - f3, f, 0.5f - f2, 1.0f, f1, 0.5f + f2);
        else if (i1 == 3) setBounds(0.5f - f2, f, 0.0f, 0.5f + f2, f1, f3);
        else if (i1 == 4) setBounds(0.5f - f2, f, 1.0f - f3, 0.5f + f2, f1, 1.0f);
        break;
    }
    case 64: case 71: { // BlockDoor: func_313_b(func_312_c(meta))
        int m = bmeta(x, y, z);
        int i = (m & 4) == 0 ? ((m - 1) & 3) : (m & 3);
        const float f = 0.1875f;
        setBounds(0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f);
        if (i == 0) setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, f);
        if (i == 1) setBounds(1.0f - f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        if (i == 2) setBounds(0.0f, 0.0f, 1.0f - f, 1.0f, 1.0f, 1.0f);
        if (i == 3) setBounds(0.0f, 0.0f, 0.0f, f, 1.0f, 1.0f);
        break;
    }
    case 66: { // BlockMinecartTrack
        int l = bmeta(x, y, z);
        if (l >= 2 && l <= 5) setBounds(0.0f, 0.0f, 0.0f, 1.0f, 0.625f, 1.0f);
        else setBounds(0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);
        break;
    }
    case 69: { // BlockLever
        int l = bmeta(x, y, z) & 7;
        const float f = 0.1875f;
        if (l == 1) setBounds(0.0f, 0.2f, 0.5f - f, f * 2.0f, 0.8f, 0.5f + f);
        else if (l == 2) setBounds(1.0f - f * 2.0f, 0.2f, 0.5f - f, 1.0f, 0.8f, 0.5f + f);
        else if (l == 3) setBounds(0.5f - f, 0.2f, 0.0f, 0.5f + f, 0.8f, f * 2.0f);
        else if (l == 4) setBounds(0.5f - f, 0.2f, 1.0f - f * 2.0f, 0.5f + f, 0.8f, 1.0f);
        else {
            const float g = 0.25f;
            setBounds(0.5f - g, 0.0f, 0.5f - g, 0.5f + g, 0.6f, 0.5f + g);
        }
        break;
    }
    case 70: case 72: case 96: { // BlockPressurePlate
        const float f = 0.0625f;
        if (bmeta(x, y, z) == 1) setBounds(f, 0.0f, f, 1.0f - f, 0.03125f, 1.0f - f);
        else setBounds(f, 0.0f, f, 1.0f - f, 0.0625f, 1.0f - f);
        break;
    }
    case 53: case 67: setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f); break; // BlockStairs
    case 44: // BlockStep (not the double slab)
        switch (bmeta(x, y, z)) {
        case 0: setBounds(0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f); break;
        case 1: setBounds(0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f); break;
        case 2: setBounds(0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f); break;
        case 3: setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f); break;
        case 4: setBounds(0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f); break;
        case 5: setBounds(0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f); break;
        default: setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f); break;
        }
        break;
    default: break;
    }
}

// Block.shouldSideBeRendered, with the current block's bounds
bool shouldSideBase(int x, int y, int z, int face) {
    if (face == 1 && bid(x, y, z) == 78) return false;
    if (face == 0 && bb.minY > 0.0) return true;
    if (face == 1 && bb.maxY < 1.0) return true;
    if (face == 2 && bb.minZ > 0.0) return true;
    if (face == 3 && bb.maxZ < 1.0) return true;
    if (face == 4 && bb.minX > 0.0) return true;
    if (face == 5 && bb.maxX < 1.0) return true;
    return !normalCube(x, y, z);
}
// ... and the overrides (x, y, z is the neighbour)
bool shouldSide(int id, int x, int y, int z, int face) {
    switch (id) {
    case 20: return bid(x, y, z) == id ? false : shouldSideBase(x, y, z, face);     // BlockBreakable (glass)
    case 79: return bid(x, y, z) == id ? false : shouldSideBase(x, y, z, 1 - face); // BlockIce: Breakable, 1 - face
    case 78: // BlockSnow
        if (face == 1) return true;
        return bmat(x, y, z) == BP[id].mat ? false : shouldSideBase(x, y, z, face);
    case 8: case 9: case 10: case 11: { // BlockFluids
        Mat m = bmat(x, y, z);
        if (m == BP[id].mat) return false;
        if (m == Mat::ice) return false;
        return face == 1 ? true : shouldSideBase(x, y, z, face);
    }
    case 44: { // BlockStep (single): a slab beside a slab compares orientations, as the jar reads them
        int orientation = bmeta(x, y, z), from = 0;
        if (bid(x, y, z) == 44) {
            switch (face) {
            case 0: from = bmeta(x, y + 1, z); if (from != 0) return orientation != from; break;
            case 1: from = bmeta(x, y - 1, z); if (from != 0) return orientation != from; break;
            case 2: from = bmeta(x, y, z - 1); if (from == 4 || from == 5) return orientation != from; break;
            case 3: from = bmeta(x, y, z + 1); if (from == 4 || from == 5) return orientation != from; break;
            case 4: from = bmeta(x - 1, y, z); if (from == 2 || from == 3) return orientation != from; break;
            case 5: from = bmeta(x + 1, y, z); if (from == 2 || from == 3) return orientation != from; break;
            }
        }
        return shouldSideBase(x, y, z, face);
    }
    default: return shouldSideBase(x, y, z, face); // (BlockLeavesBase at leafLevel 3 too)
    }
}

// ---------------------------------------------------------------- textureFace*
inline void tileUV(int i, int& j, int& k) {
    if (overrideTex >= 0) i = overrideTex;
    j = (i & 15) << 4;
    k = i & 240;
}

void faceBelow(double d, double d1, double d2, int i) {
    int j, k;
    tileUV(i, j, k);
    double d3 = (j + bb.minX * 16.0) / 256.0, d4 = (j + bb.maxX * 16.0 - 0.01) / 256.0;
    double d5 = (k + bb.minZ * 16.0) / 256.0, d6 = (k + bb.maxZ * 16.0 - 0.01) / 256.0;
    if (bb.minX < 0.0 || bb.maxX > 1.0) { d3 = double((float(j) + 0.0f) / 256.0f); d4 = double((float(j) + 15.99f) / 256.0f); }
    if (bb.minZ < 0.0 || bb.maxZ > 1.0) { d5 = double((float(k) + 0.0f) / 256.0f); d6 = double((float(k) + 15.99f) / 256.0f); }
    double d7 = d + bb.minX, d8 = d + bb.maxX, d9 = d1 + bb.minY, d10 = d2 + bb.minZ, d11 = d2 + bb.maxZ;
    T.v(d7, d9, d11, d3, d6);
    T.v(d7, d9, d10, d3, d5);
    T.v(d8, d9, d10, d4, d5);
    T.v(d8, d9, d11, d4, d6);
}

void faceAbove(double d, double d1, double d2, int i) {
    int j, k;
    tileUV(i, j, k);
    double d3 = (j + bb.minX * 16.0) / 256.0, d4 = (j + bb.maxX * 16.0 - 0.01) / 256.0;
    double d5 = (k + bb.minZ * 16.0) / 256.0, d6 = (k + bb.maxZ * 16.0 - 0.01) / 256.0;
    if (bb.minX < 0.0 || bb.maxX > 1.0) { d3 = double((float(j) + 0.0f) / 256.0f); d4 = double((float(j) + 15.99f) / 256.0f); }
    if (bb.minZ < 0.0 || bb.maxZ > 1.0) { d5 = double((float(k) + 0.0f) / 256.0f); d6 = double((float(k) + 15.99f) / 256.0f); }
    double d7 = d + bb.minX, d8 = d + bb.maxX, d9 = d1 + bb.maxY, d10 = d2 + bb.minZ, d11 = d2 + bb.maxZ;
    T.v(d8, d9, d11, d4, d6);
    T.v(d8, d9, d10, d4, d5);
    T.v(d7, d9, d10, d3, d5);
    T.v(d7, d9, d11, d3, d6);
}

// The four sides: the u range runs along x (South, North) or z (West, East), the v range from minY at
// the top vertex (the jar's quirk: a half-height side shows the top half of the tile)
void sideUV(int i, double lo, double hi, double& d3, double& d4, double& d5, double& d6) {
    int j, k;
    tileUV(i, j, k);
    d3 = (j + lo * 16.0) / 256.0;
    d4 = (j + hi * 16.0 - 0.01) / 256.0;
    d5 = (k + bb.minY * 16.0) / 256.0;
    d6 = (k + bb.maxY * 16.0 - 0.01) / 256.0;
    if (flipTex) std::swap(d3, d4);
    if (lo < 0.0 || hi > 1.0) { d3 = double((float(j) + 0.0f) / 256.0f); d4 = double((float(j) + 15.99f) / 256.0f); }
    if (bb.minY < 0.0 || bb.maxY > 1.0) { d5 = double((float(k) + 0.0f) / 256.0f); d6 = double((float(k) + 15.99f) / 256.0f); }
}

void faceSouth(double d, double d1, double d2, int i) { // -Z
    double d3, d4, d5, d6;
    sideUV(i, bb.minX, bb.maxX, d3, d4, d5, d6);
    double d8 = d + bb.minX, d9 = d + bb.maxX, d10 = d1 + bb.minY, d11 = d1 + bb.maxY, d12 = d2 + bb.minZ;
    T.v(d8, d11, d12, d4, d5);
    T.v(d9, d11, d12, d3, d5);
    T.v(d9, d10, d12, d3, d6);
    T.v(d8, d10, d12, d4, d6);
}

void faceNorth(double d, double d1, double d2, int i) { // +Z
    double d3, d4, d5, d6;
    sideUV(i, bb.minX, bb.maxX, d3, d4, d5, d6);
    double d8 = d + bb.minX, d9 = d + bb.maxX, d10 = d1 + bb.minY, d11 = d1 + bb.maxY, d12 = d2 + bb.maxZ;
    T.v(d8, d11, d12, d3, d5);
    T.v(d8, d10, d12, d3, d6);
    T.v(d9, d10, d12, d4, d6);
    T.v(d9, d11, d12, d4, d5);
}

void faceWest(double d, double d1, double d2, int i) { // -X
    double d3, d4, d5, d6;
    sideUV(i, bb.minZ, bb.maxZ, d3, d4, d5, d6);
    double d8 = d + bb.minX, d9 = d1 + bb.minY, d10 = d1 + bb.maxY, d11 = d2 + bb.minZ, d12 = d2 + bb.maxZ;
    T.v(d8, d10, d12, d4, d5);
    T.v(d8, d10, d11, d3, d5);
    T.v(d8, d9, d11, d3, d6);
    T.v(d8, d9, d12, d4, d6);
}

void faceEast(double d, double d1, double d2, int i) { // +X
    double d3, d4, d5, d6;
    sideUV(i, bb.minZ, bb.maxZ, d3, d4, d5, d6);
    double d8 = d + bb.maxX, d9 = d1 + bb.minY, d10 = d1 + bb.maxY, d11 = d2 + bb.minZ, d12 = d2 + bb.maxZ;
    T.v(d8, d9, d12, d3, d6);
    T.v(d8, d9, d11, d4, d6);
    T.v(d8, d10, d11, d4, d5);
    T.v(d8, d10, d12, d3, d5);
}

void faceAt(int side, double d, double d1, double d2, int tex) {
    switch (side) {
    case 0: faceBelow(d, d1, d2, tex); break;
    case 1: faceAbove(d, d1, d2, tex); break;
    case 2: faceSouth(d, d1, d2, tex); break;
    case 3: faceNorth(d, d1, d2, tex); break;
    case 4: faceWest(d, d1, d2, tex); break;
    default: faceEast(d, d1, d2, tex); break;
    }
}

// ---------------------------------------------------------------- RenderBlocks

const float FACTOR[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
const int DIR[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

// renderBlock / renderBlockWithColoring. TODO(seasons): colorMultiplier - BlockLeaves tints its leaves
// in the fall by the foliage level; the mesh has no vertex colour yet, so they draw untinted.
// `inset` is func_1230_b's 1/16 translation of the cactus sides.
bool renderStandard(int id, int i, int j, int k, float inset = 0.0f) {
    bool any = false;
    Light own = lightAt(i, j, k);
    for (int side = 0; side < 6; ++side) {
        int nx = i + DIR[side][0], ny = j + DIR[side][1], nz = k + DIR[side][2];
        if (!shouldSide(id, nx, ny, nz, side)) continue;
        Light l = lightAt(nx, ny, nz);
        if (side == 1 && bb.maxY != 1.0) l = own; // (!getIsGroundCover: no liquid comes through here)
        if (side == 2 && bb.minZ > 0.0) l = own;
        if (side == 3 && bb.maxZ < 1.0) l = own;
        if (side == 4 && bb.minX > 0.0) l = own;
        if (side == 5 && bb.maxX < 1.0) l = own;
        T.color(FACTOR[side], l);
        double sx = 0, sz = 0; // setTranslationF before the face, the opposite after
        if (inset != 0.0f) {
            if (side == 2) sz = inset;
            if (side == 3) sz = -inset;
            if (side == 4) sx = inset;
            if (side == 5) sx = -inset;
        }
        T.tx += sx; T.tz += sz;
        faceAt(side, i, j, k, texAt(id, i, j, k, side));
        T.tx -= sx; T.tz -= sz;
        any = true;
    }
    return any;
}

// renderTorchFace
void torchFace(int id, double x, double y, double z, double leanX, double leanZ) {
    int t = overrideTex >= 0 ? overrideTex : texIndex(id, 0);
    int topA = (t & 15) << 4, topB = t & 240;
    float top1 = float(topA) / 256.0f, top2 = (float(topA) + 15.99f) / 256.0f;
    float top3 = float(topB) / 256.0f, top4 = (float(topB) + 15.99f) / 256.0f;
    double o1 = double(top1) + 0.02734375, o2 = double(top3) + 0.0234375, o3 = double(top1) + 0.03515625, o4 = double(top3) + 0.03125;
    x += 0.5;
    z += 0.5;
    double x0 = x - 0.5, x1 = x + 0.5, z0 = z - 0.5, z1 = z + 0.5, r = 0.0625, h = 0.625;
    T.v(x + leanX * (1.0 - h) - r, y + h, z + leanZ * (1.0 - h) - r, o1, o2);
    T.v(x + leanX * (1.0 - h) - r, y + h, z + leanZ * (1.0 - h) + r, o1, o4);
    T.v(x + leanX * (1.0 - h) + r, y + h, z + leanZ * (1.0 - h) + r, o3, o4);
    T.v(x + leanX * (1.0 - h) + r, y + h, z + leanZ * (1.0 - h) - r, o3, o2);
    T.v(x - r, y + 1.0, z0, top1, top3);
    T.v(x - r + leanX, y + 0.0, z0 + leanZ, top1, top4);
    T.v(x - r + leanX, y + 0.0, z1 + leanZ, top2, top4);
    T.v(x - r, y + 1.0, z1, top2, top3);
    T.v(x + r, y + 1.0, z1, top1, top3);
    T.v(x + leanX + r, y + 0.0, z1 + leanZ, top1, top4);
    T.v(x + leanX + r, y + 0.0, z0 + leanZ, top2, top4);
    T.v(x + r, y + 1.0, z0, top2, top3);
    T.v(x0, y + 1.0, z + r, top1, top3);
    T.v(x0 + leanX, y + 0.0, z + r + leanZ, top1, top4);
    T.v(x1 + leanX, y + 0.0, z + r + leanZ, top2, top4);
    T.v(x1, y + 1.0, z + r, top2, top3);
    T.v(x1, y + 1.0, z - r, top1, top3);
    T.v(x1 + leanX, y + 0.0, z - r + leanZ, top1, top4);
    T.v(x0 + leanX, y + 0.0, z - r + leanZ, top2, top4);
    T.v(x0, y + 1.0, z - r, top2, top3);
}

bool renderTorch(int id, int i, int j, int k) {
    int l = bmeta(i, j, k);
    T.color(1.0f, lit(id) ? FULL : lightAt(i, j, k));
    const double tx = double(0.4f), hy = 0.5 - tx, tz = double(0.2f);
    if (l == 1) torchFace(id, i - hy, j + tz, k, -tx, 0.0);
    else if (l == 2) torchFace(id, i + hy, j + tz, k, tx, 0.0);
    else if (l == 3) torchFace(id, i, j + tz, k - hy, 0.0, -tx);
    else if (l == 4) torchFace(id, i, j + tz, k + hy, 0.0, tx);
    else torchFace(id, i, j, k, 0.0, 0.0);
    return true;
}

// Vec3D.rotateAroundX / Y
struct V3 {
    double x, y, z;
    void rotX(float a) {
        float c = MathHelper::cos(a), s = MathHelper::sin(a);
        double ny = y * c + z * s, nz = z * c - y * s;
        y = ny; z = nz;
    }
    void rotY(float a) {
        float c = MathHelper::cos(a), s = MathHelper::sin(a);
        double nx = x * c + z * s, nz = z * c - x * s;
        x = nx; z = nz;
    }
};

bool renderLever(int id, int i, int j, int k) {
    int l = bmeta(i, j, k), i1 = l & 7;
    bool on = (l & 8) > 0;
    bool had = overrideTex >= 0;
    if (!had) overrideTex = BLOCKS[4].index; // the cobblestone base
    const float f = 0.25f, f1 = 0.1875f, f2 = 0.1875f;
    if (i1 == 5) setBounds(0.5f - f1, 0.0f, 0.5f - f, 0.5f + f1, f2, 0.5f + f);
    else if (i1 == 6) setBounds(0.5f - f, 0.0f, 0.5f - f1, 0.5f + f, f2, 0.5f + f1);
    else if (i1 == 4) setBounds(0.5f - f1, 0.5f - f, 1.0f - f2, 0.5f + f1, 0.5f + f, 1.0f);
    else if (i1 == 3) setBounds(0.5f - f1, 0.5f - f, 0.0f, 0.5f + f1, 0.5f + f, f2);
    else if (i1 == 2) setBounds(1.0f - f2, 0.5f - f, 0.5f - f1, 1.0f, 0.5f + f, 0.5f + f1);
    else if (i1 == 1) setBounds(0.0f, 0.5f - f, 0.5f - f1, f2, 0.5f + f, 0.5f + f1);
    renderStandard(id, i, j, k);
    if (!had) overrideTex = -1;
    T.color(1.0f, lit(id) ? FULL : lightAt(i, j, k));
    int j1 = overrideTex >= 0 ? overrideTex : texIndex(id, 0);
    int k1 = (j1 & 15) << 4, l1 = j1 & 240;
    float f4 = float(k1) / 256.0f, f5 = (float(k1) + 15.99f) / 256.0f, f6 = float(l1) / 256.0f, f7 = (float(l1) + 15.99f) / 256.0f;
    const double f8 = 0.0625f, f9 = 0.0625f, f10 = 0.625f;
    V3 v[8] = {{-f8, 0.0, -f9}, {f8, 0.0, -f9}, {f8, 0.0, f9}, {-f8, 0.0, f9},
               {-f8, f10, -f9}, {f8, f10, -f9}, {f8, f10, f9}, {-f8, f10, f9}};
    for (V3& p : v) {
        if (on) { p.z -= 0.0625; p.rotX(float(3.141592653589793 * 2.0 / 9.0)); }
        else { p.z += 0.0625; p.rotX(float(-3.141592653589793 * 2.0 / 9.0)); }
        if (i1 == 6) p.rotY(1.570796f);
        if (i1 < 5) {
            p.y -= 0.375;
            p.rotX(1.570796f);
            if (i1 == 4) p.rotY(0.0f);
            if (i1 == 3) p.rotY(3.141593f);
            if (i1 == 2) p.rotY(1.570796f);
            if (i1 == 1) p.rotY(-1.570796f);
            p.x += i + 0.5;
            p.y += double(float(j) + 0.5f);
            p.z += k + 0.5;
        } else {
            p.x += i + 0.5;
            p.y += double(float(j) + 0.125f);
            p.z += k + 0.5;
        }
    }
    static const int Q[6][4] = {{0, 1, 2, 3}, {7, 6, 5, 4}, {1, 0, 4, 5}, {2, 1, 5, 6}, {3, 2, 6, 7}, {0, 3, 7, 4}};
    for (int j2 = 0; j2 < 6; ++j2) {
        if (j2 == 0) {
            f4 = float(k1 + 7) / 256.0f; f5 = (float(k1 + 9) - 0.01f) / 256.0f;
            f6 = float(l1 + 6) / 256.0f; f7 = (float(l1 + 8) - 0.01f) / 256.0f;
        } else if (j2 == 2) {
            f4 = float(k1 + 7) / 256.0f; f5 = (float(k1 + 9) - 0.01f) / 256.0f;
            f6 = float(l1 + 6) / 256.0f; f7 = (float(l1 + 16) - 0.01f) / 256.0f;
        }
        const V3 &a = v[Q[j2][0]], &b = v[Q[j2][1]], &c = v[Q[j2][2]], &d = v[Q[j2][3]];
        T.v(a.x, a.y, a.z, f4, f7);
        T.v(b.x, b.y, b.z, f5, f7);
        T.v(c.x, c.y, c.z, f5, f6);
        T.v(d.x, d.y, d.z, f4, f6);
    }
    return true;
}

bool catchesFire(int x, int y, int z) { return Blocks::fireEncourage(bid(x, y, z)) > 0; } // BlockFire.canBlockCatchFire

bool renderFire(int id, int i, int j, int k) {
    int l = overrideTex >= 0 ? overrideTex : texIndex(id, 0);
    T.color(1.0f, lightAt(i, j, k));
    int i1 = (l & 15) << 4, j1 = l & 240;
    double d = float(i1) / 256.0f, d2 = (float(i1) + 15.99f) / 256.0f;
    double d4 = float(j1) / 256.0f, d6 = (float(j1) + 15.99f) / 256.0f;
    const float f1 = 1.4f;
    if (!normalCube(i, j - 1, k) && !catchesFire(i, j - 1, k)) {
        const float f3 = 0.2f, f4 = 0.0625f;
        if (((i + j + k) & 1) == 1) {
            d = float(i1) / 256.0f;
            d2 = (float(i1) + 15.99f) / 256.0f;
            d4 = float(j1 + 16) / 256.0f;
            d6 = (float(j1) + 15.99f + 16.0f) / 256.0f;
        }
        if (((i / 2 + j / 2 + k / 2) & 1) == 1) std::swap(d, d2);
        if (catchesFire(i - 1, j, k)) {
            T.v(i + f3, j + f1 + f4, k + 1, d2, d4);
            T.v(i + 0, j + 0 + f4, k + 1, d2, d6);
            T.v(i + 0, j + 0 + f4, k + 0, d, d6);
            T.v(i + f3, j + f1 + f4, k + 0, d, d4);
            T.v(i + f3, j + f1 + f4, k + 0, d, d4);
            T.v(i + 0, j + 0 + f4, k + 0, d, d6);
            T.v(i + 0, j + 0 + f4, k + 1, d2, d6);
            T.v(i + f3, j + f1 + f4, k + 1, d2, d4);
        }
        if (catchesFire(i + 1, j, k)) {
            T.v(i + 1 - f3, j + f1 + f4, k + 0, d, d4);
            T.v(i + 1 - 0, j + 0 + f4, k + 0, d, d6);
            T.v(i + 1 - 0, j + 0 + f4, k + 1, d2, d6);
            T.v(i + 1 - f3, j + f1 + f4, k + 1, d2, d4);
            T.v(i + 1 - f3, j + f1 + f4, k + 1, d2, d4);
            T.v(i + 1 - 0, j + 0 + f4, k + 1, d2, d6);
            T.v(i + 1 - 0, j + 0 + f4, k + 0, d, d6);
            T.v(i + 1 - f3, j + f1 + f4, k + 0, d, d4);
        }
        if (catchesFire(i, j, k - 1)) {
            T.v(i + 0, j + f1 + f4, k + f3, d2, d4);
            T.v(i + 0, j + 0 + f4, k + 0, d2, d6);
            T.v(i + 1, j + 0 + f4, k + 0, d, d6);
            T.v(i + 1, j + f1 + f4, k + f3, d, d4);
            T.v(i + 1, j + f1 + f4, k + f3, d, d4);
            T.v(i + 1, j + 0 + f4, k + 0, d, d6);
            T.v(i + 0, j + 0 + f4, k + 0, d2, d6);
            T.v(i + 0, j + f1 + f4, k + f3, d2, d4);
        }
        if (catchesFire(i, j, k + 1)) {
            T.v(i + 1, j + f1 + f4, k + 1 - f3, d, d4);
            T.v(i + 1, j + 0 + f4, k + 1 - 0, d, d6);
            T.v(i + 0, j + 0 + f4, k + 1 - 0, d2, d6);
            T.v(i + 0, j + f1 + f4, k + 1 - f3, d2, d4);
            T.v(i + 0, j + f1 + f4, k + 1 - f3, d2, d4);
            T.v(i + 0, j + 0 + f4, k + 1 - 0, d2, d6);
            T.v(i + 1, j + 0 + f4, k + 1 - 0, d, d6);
            T.v(i + 1, j + f1 + f4, k + 1 - f3, d, d4);
        }
        if (catchesFire(i, j + 1, k)) {
            double d11 = i + 0.5 + 0.5, d13 = i + 0.5 - 0.5, d15 = k + 0.5 + 0.5, d17 = k + 0.5 - 0.5;
            double d19 = i + 0.5 - 0.5, d21 = i + 0.5 + 0.5, d23 = k + 0.5 - 0.5, d24 = k + 0.5 + 0.5;
            double d1 = float(i1) / 256.0f, d3 = (float(i1) + 15.99f) / 256.0f;
            double d5 = float(j1) / 256.0f, d7 = (float(j1) + 15.99f) / 256.0f;
            ++j;
            const float f2 = -0.2f;
            if (((i + j + k) & 1) == 0) {
                T.v(d19, j + f2, k + 0, d3, d5);
                T.v(d11, j + 0, k + 0, d3, d7);
                T.v(d11, j + 0, k + 1, d1, d7);
                T.v(d19, j + f2, k + 1, d1, d5);
                d1 = float(i1) / 256.0f; d3 = (float(i1) + 15.99f) / 256.0f;
                d5 = float(j1 + 16) / 256.0f; d7 = (float(j1) + 15.99f + 16.0f) / 256.0f;
                T.v(d21, j + f2, k + 1, d3, d5);
                T.v(d13, j + 0, k + 1, d3, d7);
                T.v(d13, j + 0, k + 0, d1, d7);
                T.v(d21, j + f2, k + 0, d1, d5);
            } else {
                T.v(i + 0, j + f2, d24, d3, d5);
                T.v(i + 0, j + 0, d17, d3, d7);
                T.v(i + 1, j + 0, d17, d1, d7);
                T.v(i + 1, j + f2, d24, d1, d5);
                d1 = float(i1) / 256.0f; d3 = (float(i1) + 15.99f) / 256.0f;
                d5 = float(j1 + 16) / 256.0f; d7 = (float(j1) + 15.99f + 16.0f) / 256.0f;
                T.v(i + 1, j + f2, d23, d3, d5);
                T.v(i + 1, j + 0, d15, d3, d7);
                T.v(i + 0, j + 0, d15, d1, d7);
                T.v(i + 0, j + f2, d23, d1, d5);
            }
        }
    } else {
        double d8 = i + 0.5 + 0.2, d9 = i + 0.5 - 0.2, d12 = k + 0.5 + 0.2, d14 = k + 0.5 - 0.2;
        double d16 = i + 0.5 - 0.3, d18 = i + 0.5 + 0.3, d20 = k + 0.5 - 0.3, d22 = k + 0.5 + 0.3;
        T.v(d16, j + f1, k + 1, d2, d4);
        T.v(d8, j + 0, k + 1, d2, d6);
        T.v(d8, j + 0, k + 0, d, d6);
        T.v(d16, j + f1, k + 0, d, d4);
        T.v(d18, j + f1, k + 0, d2, d4);
        T.v(d9, j + 0, k + 0, d2, d6);
        T.v(d9, j + 0, k + 1, d, d6);
        T.v(d18, j + f1, k + 1, d, d4);
        d = float(i1) / 256.0f; d2 = (float(i1) + 15.99f) / 256.0f;
        d4 = float(j1 + 16) / 256.0f; d6 = (float(j1) + 15.99f + 16.0f) / 256.0f;
        T.v(i + 1, j + f1, d22, d2, d4);
        T.v(i + 1, j + 0, d14, d2, d6);
        T.v(i + 0, j + 0, d14, d, d6);
        T.v(i + 0, j + f1, d22, d, d4);
        T.v(i + 0, j + f1, d20, d2, d4);
        T.v(i + 0, j + 0, d12, d2, d6);
        T.v(i + 1, j + 0, d12, d, d6);
        T.v(i + 1, j + f1, d20, d, d4);
        d8 = i + 0.5 - 0.5; d9 = i + 0.5 + 0.5; d12 = k + 0.5 - 0.5; d14 = k + 0.5 + 0.5;
        d16 = i + 0.5 - 0.4; d18 = i + 0.5 + 0.4; d20 = k + 0.5 - 0.4; d22 = k + 0.5 + 0.4;
        T.v(d16, j + f1, k + 0, d, d4);
        T.v(d8, j + 0, k + 0, d, d6);
        T.v(d8, j + 0, k + 1, d2, d6);
        T.v(d16, j + f1, k + 1, d2, d4);
        T.v(d18, j + f1, k + 1, d, d4);
        T.v(d9, j + 0, k + 1, d, d6);
        T.v(d9, j + 0, k + 0, d2, d6);
        T.v(d18, j + f1, k + 0, d2, d4);
        d = float(i1) / 256.0f; d2 = (float(i1) + 15.99f) / 256.0f;
        d4 = float(j1) / 256.0f; d6 = (float(j1) + 15.99f) / 256.0f;
        T.v(i + 0, j + f1, d22, d, d4);
        T.v(i + 0, j + 0, d14, d, d6);
        T.v(i + 1, j + 0, d14, d2, d6);
        T.v(i + 1, j + f1, d22, d2, d4);
        T.v(i + 1, j + f1, d20, d, d4);
        T.v(i + 1, j + 0, d12, d, d6);
        T.v(i + 0, j + 0, d12, d2, d6);
        T.v(i + 0, j + f1, d20, d2, d4);
    }
    return true;
}

// BlockRedDustWire.isPowerProviderOrWire: the wire, or a block whose canProvidePower is true
bool powerOrWire(int x, int y, int z) {
    switch (bid(x, y, z)) {
    case 55: case 69: case 70: case 72: case 96: case 75: case 76: case 77: return true;
    default: return false;
    }
}

bool renderRedWire(int id, int i, int j, int k) {
    int l = texSM(id, 1, bmeta(i, j, k));
    if (overrideTex >= 0) l = overrideTex;
    T.color(1.0f, lightAt(i, j, k));
    int i1 = (l & 15) << 4, j1 = l & 240;
    double d = float(i1) / 256.0f, d1 = (float(i1) + 15.99f) / 256.0f;
    double d2 = float(j1) / 256.0f, d3 = (float(j1) + 15.99f) / 256.0f;
    const float f1 = 0.0f, f2 = 0.03125f;
    bool flag = powerOrWire(i - 1, j, k) || (!normalCube(i - 1, j, k) && powerOrWire(i - 1, j - 1, k));
    bool flag1 = powerOrWire(i + 1, j, k) || (!normalCube(i + 1, j, k) && powerOrWire(i + 1, j - 1, k));
    bool flag2 = powerOrWire(i, j, k - 1) || (!normalCube(i, j, k - 1) && powerOrWire(i, j - 1, k - 1));
    bool flag3 = powerOrWire(i, j, k + 1) || (!normalCube(i, j, k + 1) && powerOrWire(i, j - 1, k + 1));
    if (!normalCube(i, j + 1, k)) {
        if (normalCube(i - 1, j, k) && powerOrWire(i - 1, j + 1, k)) flag = true;
        if (normalCube(i + 1, j, k) && powerOrWire(i + 1, j + 1, k)) flag1 = true;
        if (normalCube(i, j, k - 1) && powerOrWire(i, j + 1, k - 1)) flag2 = true;
        if (normalCube(i, j, k + 1) && powerOrWire(i, j + 1, k + 1)) flag3 = true;
    }
    const float f3 = 0.3125f;
    float f4 = float(i + 0), f5 = float(i + 1), f6 = float(k + 0), f7 = float(k + 1);
    int byte0 = 0;
    if ((flag || flag1) && !flag2 && !flag3) byte0 = 1;
    if ((flag2 || flag3) && !flag1 && !flag) byte0 = 2;
    if (byte0 != 0) {
        d = float(i1 + 16) / 256.0f;
        d1 = (float(i1 + 16) + 15.99f) / 256.0f;
        d2 = float(j1) / 256.0f;
        d3 = (float(j1) + 15.99f) / 256.0f;
    }
    if (byte0 == 0) {
        if (flag1 || flag2 || flag3 || flag) {
            if (!flag) { f4 += f3; d += f3 / 16.0f; }
            if (!flag1) { f5 -= f3; d1 -= f3 / 16.0f; }
            if (!flag2) { f6 += f3; d2 += f3 / 16.0f; }
            if (!flag3) { f7 -= f3; d3 -= f3 / 16.0f; }
        }
        T.v(f5 + f1, j + f2, f7 + f1, d1, d3);
        T.v(f5 + f1, j + f2, f6 - f1, d1, d2);
        T.v(f4 - f1, j + f2, f6 - f1, d, d2);
        T.v(f4 - f1, j + f2, f7 + f1, d, d3);
    }
    if (byte0 == 1) {
        T.v(f5 + f1, j + f2, f7 + f1, d1, d3);
        T.v(f5 + f1, j + f2, f6 - f1, d1, d2);
        T.v(f4 - f1, j + f2, f6 - f1, d, d2);
        T.v(f4 - f1, j + f2, f7 + f1, d, d3);
    }
    if (byte0 == 2) {
        T.v(f5 + f1, j + f2, f7 + f1, d1, d3);
        T.v(f5 + f1, j + f2, f6 - f1, d, d3);
        T.v(f4 - f1, j + f2, f6 - f1, d, d2);
        T.v(f4 - f1, j + f2, f7 + f1, d1, d2);
    }
    d = float(i1 + 16) / 256.0f;
    d1 = (float(i1 + 16) + 15.99f) / 256.0f;
    d2 = float(j1) / 256.0f;
    d3 = (float(j1) + 15.99f) / 256.0f;
    if (!normalCube(i, j + 1, k)) {
        if (normalCube(i - 1, j, k) && bid(i - 1, j + 1, k) == 55) {
            T.v(i + f2, j + 1 + f1, k + 1 + f1, d1, d2);
            T.v(i + f2, j + 0 - f1, k + 1 + f1, d, d2);
            T.v(i + f2, j + 0 - f1, k + 0 - f1, d, d3);
            T.v(i + f2, j + 1 + f1, k + 0 - f1, d1, d3);
        }
        if (normalCube(i + 1, j, k) && bid(i + 1, j + 1, k) == 55) {
            T.v(i + 1 - f2, j + 0 - f1, k + 1 + f1, d, d3);
            T.v(i + 1 - f2, j + 1 + f1, k + 1 + f1, d1, d3);
            T.v(i + 1 - f2, j + 1 + f1, k + 0 - f1, d1, d2);
            T.v(i + 1 - f2, j + 0 - f1, k + 0 - f1, d, d2);
        }
        if (normalCube(i, j, k - 1) && bid(i, j + 1, k - 1) == 55) {
            T.v(i + 1 + f1, j + 0 - f1, k + f2, d, d3);
            T.v(i + 1 + f1, j + 1 + f1, k + f2, d1, d3);
            T.v(i + 0 - f1, j + 1 + f1, k + f2, d1, d2);
            T.v(i + 0 - f1, j + 0 - f1, k + f2, d, d2);
        }
        if (normalCube(i, j, k + 1) && bid(i, j + 1, k + 1) == 55) {
            T.v(i + 1 + f1, j + 1 + f1, k + 1 - f2, d1, d2);
            T.v(i + 1 + f1, j + 0 - f1, k + 1 - f2, d, d2);
            T.v(i + 0 - f1, j + 0 - f1, k + 1 - f2, d, d3);
            T.v(i + 0 - f1, j + 1 + f1, k + 1 - f2, d1, d3);
        }
    }
    return true;
}

bool renderMinecartTrack(int id, int i, int j, int k) {
    int l = bmeta(i, j, k);
    int i1 = texSM(id, 0, l);
    if (overrideTex >= 0) i1 = overrideTex;
    T.color(1.0f, lightAt(i, j, k));
    int j1 = (i1 & 15) << 4, k1 = i1 & 240;
    double d = float(j1) / 256.0f, d1 = (float(j1) + 15.99f) / 256.0f;
    double d2 = float(k1) / 256.0f, d3 = (float(k1) + 15.99f) / 256.0f;
    const float f1 = 0.0625f;
    float f2 = float(i + 1), f3 = float(i + 1), f4 = float(i + 0), f5 = float(i + 0);
    float f6 = float(k + 0), f7 = float(k + 1), f8 = float(k + 1), f9 = float(k + 0);
    float f10 = float(j) + f1, f11 = float(j) + f1, f12 = float(j) + f1, f13 = float(j) + f1;
    if (l == 1 || l == 2 || l == 3 || l == 7) {
        f2 = f5 = float(i + 1);
        f3 = f4 = float(i + 0);
        f6 = f7 = float(k + 1);
        f8 = f9 = float(k + 0);
    } else if (l == 8) {
        f2 = f3 = float(i + 0);
        f4 = f5 = float(i + 1);
        f6 = f9 = float(k + 1);
        f7 = f8 = float(k + 0);
    } else if (l == 9) {
        f2 = f5 = float(i + 0);
        f3 = f4 = float(i + 1);
        f6 = f7 = float(k + 0);
        f8 = f9 = float(k + 1);
    }
    if (l == 2 || l == 4) { f10++; f13++; }
    else if (l == 3 || l == 5) { f11++; f12++; }
    T.v(f2, f10, f6, d1, d2);
    T.v(f3, f11, f7, d1, d3);
    T.v(f4, f12, f8, d, d3);
    T.v(f5, f13, f9, d, d2);
    T.v(f5, f13, f9, d, d2);
    T.v(f4, f12, f8, d, d3);
    T.v(f3, f11, f7, d1, d3);
    T.v(f2, f10, f6, d1, d2);
    return true;
}

bool renderLadder(int id, int i, int j, int k) {
    int l = texIndex(id, 0);
    if (overrideTex >= 0) l = overrideTex;
    T.color(1.0f, lightAt(i, j, k));
    int i1 = (l & 15) << 4, j1 = l & 240;
    double d = float(i1) / 256.0f, d1 = (float(i1) + 15.99f) / 256.0f;
    double d2 = float(j1) / 256.0f, d3 = (float(j1) + 15.99f) / 256.0f;
    int k1 = bmeta(i, j, k);
    const float f1 = 0.0f, f2 = 0.05f;
    if (k1 == 5) {
        T.v(i + f2, j + 1 + f1, k + 1 + f1, d, d2);
        T.v(i + f2, j + 0 - f1, k + 1 + f1, d, d3);
        T.v(i + f2, j + 0 - f1, k + 0 - f1, d1, d3);
        T.v(i + f2, j + 1 + f1, k + 0 - f1, d1, d2);
    }
    if (k1 == 4) {
        T.v(i + 1 - f2, j + 0 - f1, k + 1 + f1, d1, d3);
        T.v(i + 1 - f2, j + 1 + f1, k + 1 + f1, d1, d2);
        T.v(i + 1 - f2, j + 1 + f1, k + 0 - f1, d, d2);
        T.v(i + 1 - f2, j + 0 - f1, k + 0 - f1, d, d3);
    }
    if (k1 == 3) {
        T.v(i + 1 + f1, j + 0 - f1, k + f2, d1, d3);
        T.v(i + 1 + f1, j + 1 + f1, k + f2, d1, d2);
        T.v(i + 0 - f1, j + 1 + f1, k + f2, d, d2);
        T.v(i + 0 - f1, j + 0 - f1, k + f2, d, d3);
    }
    if (k1 == 2) {
        T.v(i + 1 + f1, j + 1 + f1, k + 1 - f2, d, d2);
        T.v(i + 1 + f1, j + 0 - f1, k + 1 - f2, d, d3);
        T.v(i + 0 - f1, j + 0 - f1, k + 1 - f2, d1, d3);
        T.v(i + 0 - f1, j + 1 + f1, k + 1 - f2, d1, d2);
    }
    return true;
}

// renderCrossedSquares
void crossedSquares(int id, int meta, double d, double d1, double d2) {
    int j = texSM(id, 0, meta);
    if (overrideTex >= 0) j = overrideTex;
    int k = (j & 15) << 4, l = j & 240;
    double d3 = float(k) / 256.0f, d4 = (float(k) + 15.99f) / 256.0f;
    double d5 = float(l) / 256.0f, d6 = (float(l) + 15.99f) / 256.0f;
    double d7 = d + 0.5 - double(0.45f), d8 = d + 0.5 + double(0.45f);
    double d9 = d2 + 0.5 - double(0.45f), d10 = d2 + 0.5 + double(0.45f);
    T.v(d7, d1 + 1.0, d9, d3, d5);
    T.v(d7, d1 + 0.0, d9, d3, d6);
    T.v(d8, d1 + 0.0, d10, d4, d6);
    T.v(d8, d1 + 1.0, d10, d4, d5);
    T.v(d8, d1 + 1.0, d10, d3, d5);
    T.v(d8, d1 + 0.0, d10, d3, d6);
    T.v(d7, d1 + 0.0, d9, d4, d6);
    T.v(d7, d1 + 1.0, d9, d4, d5);
    T.v(d7, d1 + 1.0, d10, d3, d5);
    T.v(d7, d1 + 0.0, d10, d3, d6);
    T.v(d8, d1 + 0.0, d9, d4, d6);
    T.v(d8, d1 + 1.0, d9, d4, d5);
    T.v(d8, d1 + 1.0, d9, d3, d5);
    T.v(d8, d1 + 0.0, d9, d3, d6);
    T.v(d7, d1 + 0.0, d10, d4, d6);
    T.v(d7, d1 + 1.0, d10, d4, d5);
}

// func_1245_b: the crops' four planes, a quarter in from each side
void cropPlanes(int id, int meta, double d, double d1, double d2) {
    int j = texSM(id, 0, meta);
    if (overrideTex >= 0) j = overrideTex;
    int k = (j & 15) << 4, l = j & 240;
    double d3 = float(k) / 256.0f, d4 = (float(k) + 15.99f) / 256.0f;
    double d5 = float(l) / 256.0f, d6 = (float(l) + 15.99f) / 256.0f;
    double d7 = d + 0.5 - 0.25, d8 = d + 0.5 + 0.25, d9 = d2 + 0.5 - 0.5, d10 = d2 + 0.5 + 0.5;
    T.v(d7, d1 + 1.0, d9, d3, d5);
    T.v(d7, d1 + 0.0, d9, d3, d6);
    T.v(d7, d1 + 0.0, d10, d4, d6);
    T.v(d7, d1 + 1.0, d10, d4, d5);
    T.v(d7, d1 + 1.0, d10, d3, d5);
    T.v(d7, d1 + 0.0, d10, d3, d6);
    T.v(d7, d1 + 0.0, d9, d4, d6);
    T.v(d7, d1 + 1.0, d9, d4, d5);
    T.v(d8, d1 + 1.0, d10, d3, d5);
    T.v(d8, d1 + 0.0, d10, d3, d6);
    T.v(d8, d1 + 0.0, d9, d4, d6);
    T.v(d8, d1 + 1.0, d9, d4, d5);
    T.v(d8, d1 + 1.0, d9, d3, d5);
    T.v(d8, d1 + 0.0, d9, d3, d6);
    T.v(d8, d1 + 0.0, d10, d4, d6);
    T.v(d8, d1 + 1.0, d10, d4, d5);
    d7 = d + 0.5 - 0.5; d8 = d + 0.5 + 0.5; d9 = d2 + 0.5 - 0.25; d10 = d2 + 0.5 + 0.25;
    T.v(d7, d1 + 1.0, d9, d3, d5);
    T.v(d7, d1 + 0.0, d9, d3, d6);
    T.v(d8, d1 + 0.0, d9, d4, d6);
    T.v(d8, d1 + 1.0, d9, d4, d5);
    T.v(d8, d1 + 1.0, d9, d3, d5);
    T.v(d8, d1 + 0.0, d9, d3, d6);
    T.v(d7, d1 + 0.0, d9, d4, d6);
    T.v(d7, d1 + 1.0, d9, d4, d5);
    T.v(d8, d1 + 1.0, d10, d3, d5);
    T.v(d8, d1 + 0.0, d10, d3, d6);
    T.v(d7, d1 + 0.0, d10, d4, d6);
    T.v(d7, d1 + 1.0, d10, d4, d5);
    T.v(d7, d1 + 1.0, d10, d3, d5);
    T.v(d7, d1 + 0.0, d10, d3, d6);
    T.v(d8, d1 + 0.0, d10, d4, d6);
    T.v(d8, d1 + 1.0, d10, d4, d5);
}

bool renderPlant(int id, int i, int j, int k) {
    T.color(1.0f, lightAt(i, j, k));
    crossedSquares(id, bmeta(i, j, k), i, j, k);
    return true;
}

bool renderCrops(int id, int i, int j, int k) {
    T.color(1.0f, lightAt(i, j, k));
    cropPlanes(id, bmeta(i, j, k), i, double(float(j) - 0.0625f), k);
    return true;
}

// getFluidSideHeight
float fluidCorner(int x, int y, int z, Mat m) {
    int l = 0;
    float f = 0.0f;
    for (int s = 0; s < 4; ++s) {
        int j1 = x - (s & 1), l1 = z - (s >> 1 & 1);
        if (bmat(j1, y + 1, l1) == m) return 1.0f;
        Mat m1 = bmat(j1, y, l1);
        if (m1 == m) {
            int i2 = bmeta(j1, y, l1);
            if (i2 >= 8 || i2 == 0) {
                f += fluidLevel(i2) * 10.0f;
                l += 10;
            }
            f += fluidLevel(i2);
            ++l;
        } else if (!matSolid(m1)) {
            ++f;
            ++l;
        }
    }
    return 1.0f - f / float(l);
}

// BlockFluids.getBlockBrightness: the brighter of the cell and the one above
Light fluidLight(int x, int y, int z) { return lightMax(lightAt(x, y, z), lightAt(x, y + 1, z)); }

bool renderFluid(int id, int x, int y, int z) {
    bool top = shouldSide(id, x, y + 1, z, 1), bottom = shouldSide(id, x, y - 1, z, 0);
    bool sides[4] = {shouldSide(id, x, y, z - 1, 2), shouldSide(id, x, y, z + 1, 3), shouldSide(id, x - 1, y, z, 4),
                     shouldSide(id, x + 1, y, z, 5)};
    if (!top && !bottom && !sides[0] && !sides[1] && !sides[2] && !sides[3]) return false;
    bool flag2 = false;
    const float f = 0.5f, f2 = 0.8f, f3 = 0.6f;
    Mat m = BP[id].mat;
    float f4 = fluidCorner(x, y, z, m), f5 = fluidCorner(x, y, z + 1, m);
    float f6 = fluidCorner(x + 1, y, z + 1, m), f7 = fluidCorner(x + 1, y, z, m);
    if (top) {
        flag2 = true;
        int i1 = texAt(id, x, y, z, 1);
        float f9 = float(flowRadians(m, x, y, z));
        if (f9 > -999.0f) i1 = texAt(id, x, y, z, 7);
        int l1 = (i1 & 15) << 4, j2 = i1 & 240;
        double d2 = (l1 + 8.0) / 256.0, d3 = (j2 + 8.0) / 256.0;
        if (f9 < -999.0f) {
            f9 = 0.0f;
        } else {
            d2 = float(l1 + 16) / 256.0f;
            d3 = float(j2 + 16) / 256.0f;
        }
        float f11 = MathHelper::sin(f9) * 8.0f / 256.0f, f13 = MathHelper::cos(f9) * 8.0f / 256.0f;
        T.color(1.0f, fluidLight(x, y, z));
        T.v(x + 0, y + f4, z + 0, d2 - f13 - f11, d3 - f13 + f11);
        T.v(x + 0, y + f5, z + 1, d2 - f13 + f11, d3 + f13 + f11);
        T.v(x + 1, y + f6, z + 1, d2 + f13 + f11, d3 + f13 - f11);
        T.v(x + 1, y + f7, z + 0, d2 + f13 - f11, d3 - f13 - f11);
    }
    if (bottom) {
        T.color(f, fluidLight(x, y - 1, z));
        faceBelow(x, y, z, texIndex(id, 0));
        flag2 = true;
    }
    for (int s = 0; s < 4; ++s) {
        int px = x, pz = z;
        if (s == 0) pz = z - 1;
        if (s == 1) ++pz;
        if (s == 2) px = x - 1;
        if (s == 3) ++px;
        int l2 = texAt(id, x, y, z, s + 2);
        int i3 = (l2 & 15) << 4, j3 = l2 & 240;
        if (!sides[s]) continue;
        float f10, f12, f14, f16, f17, f18;
        if (s == 0) { f10 = f4; f12 = f7; f14 = float(x); f17 = float(x + 1); f16 = float(z); f18 = float(z); }
        else if (s == 1) { f10 = f6; f12 = f5; f14 = float(x + 1); f17 = float(x); f16 = float(z + 1); f18 = float(z + 1); }
        else if (s == 2) { f10 = f5; f12 = f4; f14 = float(x); f17 = float(x); f16 = float(z + 1); f18 = float(z); }
        else { f10 = f7; f12 = f6; f14 = float(x + 1); f17 = float(x + 1); f16 = float(z); f18 = float(z + 1); }
        flag2 = true;
        double d4 = float(i3 + 0) / 256.0f, d5 = (i3 + 16 - 0.01) / 256.0;
        double d6 = (float(j3) + (1.0f - f10) * 16.0f) / 256.0f, d7 = (float(j3) + (1.0f - f12) * 16.0f) / 256.0f;
        double d8 = (j3 + 16 - 0.01) / 256.0;
        T.color(s < 2 ? f2 : f3, fluidLight(px, y, pz));
        T.v(f14, y + f10, f16, d4, d6);
        T.v(f17, y + f12, f18, d5, d7);
        T.v(f17, y + 0, f18, d5, d8);
        T.v(f14, y + 0, f16, d4, d8);
    }
    return flag2;
}

bool renderCactus(int id, int i, int j, int k) { return renderStandard(id, i, j, k, 0.0625f); }

// renderPressurePlate - render type 11, which NSSS gives the fence
bool renderFence(int id, int i, int j, int k) {
    float f = 0.375f, f1 = 0.625f;
    setBounds(f, 0.0f, f, f1, 1.0f, f1);
    renderStandard(id, i, j, k);
    auto joins = [&](int x, int z) { int n = bid(x, j, z); return n != 0 && (BP[n].opaqueCube || n == id); };
    bool blockSouth = joins(i - 1, k), blockNorth = joins(i + 1, k), blockEast = joins(i, k - 1), blockWest = joins(i, k + 1);
    bool flag1 = blockNorth || blockSouth, flag2 = blockEast || blockWest;
    if (!flag1 && !flag2) flag1 = true;
    f = 0.4375f; f1 = 0.5625f;
    float f2 = 0.75f, f3 = 0.9375f;
    float f4 = blockSouth ? 0.0f : f, f5 = blockNorth ? 1.0f : f1, f6 = blockEast ? 0.0f : f, f7 = blockWest ? 1.0f : f1;
    if (flag1) { setBounds(f4, f2, f, f5, f3, f1); renderStandard(id, i, j, k); }
    if (flag2) { setBounds(f, f2, f6, f1, f3, f7); renderStandard(id, i, j, k); }
    f2 = 0.375f; f3 = 0.5625f;
    if (flag1) { setBounds(f4, f2, f, f5, f3, f1); renderStandard(id, i, j, k); }
    if (flag2) { setBounds(f, f2, f6, f1, f3, f7); renderStandard(id, i, j, k); }
    setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    return false;
}

bool renderStairs(int id, int i, int j, int k) {
    int l = bmeta(i, j, k);
    if (l == 0) {
        setBounds(0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f); renderStandard(id, i, j, k);
        setBounds(0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f); renderStandard(id, i, j, k);
    } else if (l == 1) {
        setBounds(0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f); renderStandard(id, i, j, k);
        setBounds(0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f); renderStandard(id, i, j, k);
    } else if (l == 2) {
        setBounds(0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f); renderStandard(id, i, j, k);
        setBounds(0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f); renderStandard(id, i, j, k);
    } else if (l == 3) {
        setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f); renderStandard(id, i, j, k);
        setBounds(0.0f, 0.0f, 0.5f, 1.0f, 0.5f, 1.0f); renderStandard(id, i, j, k);
    }
    setBounds(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    return false;
}

// renderDoor: every face, whatever is beside it; a negative texture is drawn mirrored
bool renderDoor(int id, int i, int j, int k) {
    Light own = lightAt(i, j, k);
    for (int side = 0; side < 6; ++side) {
        Light l = lightAt(i + DIR[side][0], j + DIR[side][1], k + DIR[side][2]);
        if (side == 0 && bb.minY > 0.0) l = own;
        if (side == 1 && bb.maxY < 1.0) l = own;
        if (side == 2 && bb.minZ > 0.0) l = own;
        if (side == 3 && bb.maxZ < 1.0) l = own;
        if (side == 4 && bb.minX > 0.0) l = own;
        if (side == 5 && bb.maxX < 1.0) l = own;
        if (lit(id)) l = FULL;
        T.color(FACTOR[side], l);
        int t = texAt(id, i, j, k, side);
        if (side >= 2 && t < 0) {
            flipTex = true;
            t = -t;
        }
        faceAt(side, i, j, k, t);
        flipTex = false;
    }
    return true;
}

// renderGear: a tile on each solid block beside it, flipped on alternate cells (the decompiled source
// seems to reuse the x parameter for the texture column; the jar's quads are where the gear is)
bool renderGear(int id, int x, int y, int z) {
    int n18 = texSM(id, 1, bmeta(x, y, z));
    if (overrideTex >= 0) n18 = overrideTex;
    T.color(1.0f, lightAt(x, y, z));
    int n = ((n18 & 15) << 4) + 16, n19 = (n18 & 15) << 4, n31 = n18 & 240;
    if (((x + y + z) & 1) == 1) {
        n = (n18 & 15) << 4;
        n19 = ((n18 & 15) << 4) + 16;
    }
    float av = float(n) / 256.0f, ay = (float(n) + 15.99f) / 256.0f, n8 = float(n31) / 256.0f, n32 = (float(n31) + 15.99f) / 256.0f;
    float n21 = float(n19) / 256.0f, n22 = (float(n19) + 15.99f) / 256.0f, n23 = float(n31) / 256.0f, n24 = (float(n31) + 15.99f) / 256.0f;
    if (normalCube(x - 1, y, z)) {
        T.v(x + 0.05f, y + 1 + 0.125f, z + 1 + 0.125f, av, n8);
        T.v(x + 0.05f, y - 0.125f, z + 1 + 0.125f, av, n32);
        T.v(x + 0.05f, y - 0.125f, z - 0.125f, ay, n32);
        T.v(x + 0.05f, y + 1 + 0.125f, z - 0.125f, ay, n8);
    }
    if (normalCube(x + 1, y, z)) {
        T.v(x + 1 - 0.05f, y - 0.125f, z + 1 + 0.125f, ay, n32);
        T.v(x + 1 - 0.05f, y + 1 + 0.125f, z + 1 + 0.125f, ay, n8);
        T.v(x + 1 - 0.05f, y + 1 + 0.125f, z - 0.125f, av, n8);
        T.v(x + 1 - 0.05f, y - 0.125f, z - 0.125f, av, n32);
    }
    if (normalCube(x, y, z - 1)) {
        T.v(x + 1 + 0.125f, y - 0.125f, z + 0.05f, n22, n24);
        T.v(x + 1 + 0.125f, y + 1 + 0.125f, z + 0.05f, n22, n23);
        T.v(x - 0.125f, y + 1 + 0.125f, z + 0.05f, n21, n23);
        T.v(x - 0.125f, y - 0.125f, z + 0.05f, n21, n24);
    }
    if (normalCube(x, y, z + 1)) {
        T.v(x + 1 + 0.125f, y + 1 + 0.125f, z + 1 - 0.05f, n21, n23);
        T.v(x + 1 + 0.125f, y - 0.125f, z + 1 - 0.05f, n21, n24);
        T.v(x - 0.125f, y - 0.125f, z + 1 - 0.05f, n22, n24);
        T.v(x - 0.125f, y + 1 + 0.125f, z + 1 - 0.05f, n22, n23);
    }
    return true;
}

// renderBlockByRenderType
bool renderByType(int id, int x, int y, int z) {
    boundsFor(id, x, y, z);
    switch (BLOCKS[id].render) {
    case RenderType::Cube: return renderStandard(id, x, y, z);
    case RenderType::Liquid: return renderFluid(id, x, y, z);
    case RenderType::Cactus: return renderCactus(id, x, y, z);
    case RenderType::Cross: return renderPlant(id, x, y, z);
    case RenderType::Crops: return renderCrops(id, x, y, z);
    case RenderType::Torch: return renderTorch(id, x, y, z);
    case RenderType::Fire: return renderFire(id, x, y, z);
    case RenderType::Wire: return renderRedWire(id, x, y, z);
    case RenderType::Ladder: return renderLadder(id, x, y, z);
    case RenderType::Door: return renderDoor(id, x, y, z);
    case RenderType::Rail: return renderMinecartTrack(id, x, y, z);
    case RenderType::Stairs: return renderStairs(id, x, y, z);
    case RenderType::Fence: return renderFence(id, x, y, z);
    case RenderType::Lever: return renderLever(id, x, y, z);
    case RenderType::Gear: return renderGear(id, x, y, z);
    default: return false;
    }
}

void fillPad(const World& world) {
    for (int z = 0; z < P; z++) {
        int wz = oz + z;
        for (int x = 0; x < P; x++) {
            int wx = ox + x;
            const Chunk* c = world.getChunk(wx >> 4, wz >> 4);
            for (int y = 0; y < P; y++) {
                int wy = oy + y, i = (y * P + z) * P + x;
                if (wy < 0 || wy >= WORLD_H) {
                    pad.id[i] = 0;
                    pad.meta[i] = 0;
                    rawLight[i] = wy < 0 ? 0 : 0xF0;
                    continue;
                }
                if (!c) { // not loaded: a solid wall, so no faces are drawn into it until it is
                    pad.id[i] = 1;
                    pad.meta[i] = 0;
                    rawLight[i] = 0xF0;
                    continue;
                }
                int lx = wx & 15, lz = wz & 15;
                pad.id[i] = c->getBlockID(lx, wy, lz);
                pad.meta[i] = uint8_t(c->getMetadata(lx, wy, lz));
                rawLight[i] = uint8_t(c->getSkyLight(lx, wy, lz) << 4 | c->getBlockLight(lx, wy, lz));
            }
        }
    }
    // ChunkCache.getBrightness: a single slab or farmland takes the brightest of the cell above and the
    // four beside it (sky and block separately: max commutes with the shader's formula)
    std::memcpy(pad.light, rawLight, sizeof rawLight);
    for (int y = 1; y < P - 1; y++)
        for (int z = 1; z < P - 1; z++)
            for (int x = 1; x < P - 1; x++) {
                int i = (y * P + z) * P + x, id = pad.id[i];
                if (id != 44 && id != 60) continue;
                const int n[5] = {i + P * P, i + 1, i - 1, i + P, i - P};
                int l = 0;
                for (int k : n) l = lightMax(l, rawLight[k]);
                pad.light[i] = uint8_t(l);
            }
}

} // namespace

void ChunkMesher::buildSection(const World& world, int cx, int sy, int cz, MeshData& data) {
    data.vertices.clear();
    data.indices.clear();
    for (auto& f : data.faceStart) f = 0;
    for (auto& g : groups) g.clear();
    out = &data;
    full = false;

    secX = cx * 16; secY = sy * 16; secZ = cz * 16;
    ox = secX - B; oy = secY - B; oz = secZ - B;
    fillPad(world);

    // the jar's passes: opaque blocks, then the translucent ones (water, ice)
    for (int pass = 0; pass < 2 && !full; pass++)
        for (int y = secY; y < secY + 16 && !full; y++)
            for (int z = secZ; z < secZ + 16 && !full; z++)
                for (int x = secX; x < secX + 16 && !full; x++) {
                    int id = bid(x, y, z);
                    if (!id || BLOCKS[id].pass != pass) continue;
                    curX = x; curY = y; curZ = z;
                    overrideTex = -1;
                    flipTex = false;
                    T.tx = T.ty = T.tz = 0;
                    T.n = 0;
                    renderByType(id, x, y, z);
                }

    for (int f = 0; f < 7; f++) {
        data.faceStart[f] = uint32_t(data.indices.size());
        data.indices.insert(data.indices.end(), groups[f].begin(), groups[f].end());
    }
    data.faceStart[7] = uint32_t(data.indices.size());
}
