#include "ChunkMesh.h"
#include "world/World.h"
#include "world/BlockTable.h"
#include "world/BlockProps.h"
#include "model/BlockModels.h"
#include "DynamicLights.h"
#include <algorithm>
#include <cmath>
#include <cstring>

const float ChunkMesher::LIGHT_BRIGHTNESS[16] = {
    0.05f, 0.06666666f, 0.085185185f, 0.10588235f, 0.12916666f, 0.15555558f, 0.18571426f, 0.22051284f,
    0.26111114f, 0.30909094f, 0.36666667f, 0.43703708f, 0.52500004f, 0.63809526f, 0.7888889f, 1.0f,
};

namespace {

// ---------------------------------------------------------------- padded copy
// The section plus a 1-block border, read once from the World so the mesher
// itself only does array reads (big win on the Old 3DS).
constexpr int P = 18;
struct Padded {
    uint8_t id[P * P * P];
    uint8_t meta[P * P * P];
    uint8_t light[P * P * P];    // sky << 4 | block (the shader applies skylightSubtracted)
    uint8_t unloaded[P * P * P]; // neighbour chunk not loaded: never draw faces into it
};
Padded pad;
inline int pidx(int x, int y, int z) { return (y * P + z) * P + x; }

// ---------------------------------------------------------------- output
std::vector<uint16_t> groups[7];
MeshData* out;
bool full;

// Beta face factors: bottom 0.5, top 1.0, z sides 0.8, x sides 0.6
const float FACE_SHADE[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};

struct V { int16_t x, y, z, u, v; };

void emitQuad(const V q[4], int shade, int group, bool twoSided) {
    if (out->vertices.size() + (twoSided ? 8 : 4) > 65535) { full = true; return; }
    uint16_t base = uint16_t(out->vertices.size());
    for (int i = 0; i < 4; i++) out->vertices.push_back({q[i].x, q[i].y, q[i].z, int16_t(shade), q[i].u, q[i].v});
    std::vector<uint16_t>& g = groups[group];
    const uint16_t idx[6] = {base, uint16_t(base + 1), uint16_t(base + 2), base, uint16_t(base + 2), uint16_t(base + 3)};
    g.insert(g.end(), idx, idx + 6);
    if (twoSided) {
        const uint16_t rev[6] = {base, uint16_t(base + 2), uint16_t(base + 1), base, uint16_t(base + 3), uint16_t(base + 2)};
        g.insert(g.end(), rev, rev + 6);
    }
}

// The vertex's w: face factor in twentieths, sky and block light (see vshader.v.pica)
inline int shadeFor(float factor, int light) {
    int q = int(factor * 20.0f + 0.5f);
    if (q > 20) q = 20;
    if (q < 0) q = 0;
    return q * 256 + (light & 255);
}
int curSub = 0; // skylightSubtracted while meshing (only for model blocks' baked shades)
inline int combined(int light) {
    int s = (light >> 4) - curSub, b = light & 15;
    return s > b ? s : b;
}

// Atlas coordinates in quarter texels: tile column/row from the index, `col`/`row` inside the
// tile in Beta's image space (row 0 = top), v flipped to count from the bottom of the 256x1024 atlas.
inline int16_t texU(int tex, float col) { return int16_t(((tex & 15) * 16) * 4 + int(std::lround(col * 4.0f))); }
inline int16_t texV(int tex, float row) { return int16_t((ChunkMesher::ATLAS_H - (tex >> 4) * 16) * 4 - int(std::lround(row * 4.0f))); }

// ---------------------------------------------------------------- model blocks
// AxeolonBlockAnims.drawStill: chests, doors, furnaces... straight from their Blockbench models.
const World* curWorld;
int secX, secY, secZ;
std::vector<axm::ModelVertex> rigQuads;
std::vector<BlockModels::Batch> rigBatches;

void emitModelBlock(int px, int py, int pz) {
    rigQuads.clear();
    rigBatches.clear();
    int wx = secX + px - 1, wy = secY + py - 1, wz = secZ + pz - 1;
    if (!BlockModels::drawStill(*curWorld, wx, wy, wz, px - 1, py - 1, pz - 1, rigQuads, rigBatches)) return;
    for (const auto& b : rigBatches) {
        const RigTexture& t = *b.tex;
        for (size_t q = b.start; q + 4 <= b.start + b.count; q += 4) {
            V v[4];
            int shade = 0;
            for (int k = 0; k < 4; k++) {
                const axm::ModelVertex& m = rigQuads[q + k];
                v[k].x = int16_t(std::lround(m.x * 64.0f));
                v[k].y = int16_t(std::lround(m.y * 64.0f));
                v[k].z = int16_t(std::lround(m.z * 64.0f));
                v[k].u = int16_t(std::lround((t.x + m.u * t.w) * 4.0f));
                v[k].v = int16_t(std::lround((ChunkMesher::ATLAS_H - (t.y + m.v * t.h)) * 4.0f));
                shade = int(m.shade * 1000.0f + 0.5f);
            }
            // the model's shade already holds the light: take it back out, keep the factor. A solid
            // model block (a chest, a furnace) is dark inside, so its light is the brightest of the
            // air around it, like Beta's standard block lighting reads its neighbours
            int light = pad.light[pidx(px, py, pz)];
            {
                static const int N[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                int sky = light >> 4, blk = light & 15;
                for (const auto& d : N) {
                    int l = pad.light[pidx(px + d[0], py + d[1], pz + d[2])];
                    sky = std::max(sky, l >> 4);
                    blk = std::max(blk, l & 15);
                }
                light = sky << 4 | blk;
            }
            float b = ChunkMesher::LIGHT_BRIGHTNESS[combined(light)];
            emitQuad(v, shadeFor(float(shade) / 1000.0f / b, light), 6, false);
        }
    }
}

// ---------------------------------------------------------------- box faces
// Corner templates, counter-clockwise seen from outside; 0 = box min, 1 = box max.
const int CORNERS[6][4][3] = {
    {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}, // -Y
    {{0,1,0},{0,1,1},{1,1,1},{1,1,0}}, // +Y
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}, // -Z
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, // +Z
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, // -X
    {{1,0,1},{1,0,0},{1,1,0},{1,1,1}}, // +X
};
const int NORMAL[6][3] = {{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}};

struct Box { float b[6]; }; // minX minY minZ maxX maxY maxZ, block units

// RenderBlocks texture mapping, per face (verified against renderXxxFace):
//  top/bottom: col = x*16, row = z*16
//  sides: col runs along the face's "right" (mirrored on -Z and +X), and
//         row = (minY + maxY - y)*16 - Beta maps a box's top edge to row minY*16.
void faceUV(int side, const Box& bx, const float c[3], float& col, float& row) {
    const float* b = bx.b;
    switch (side) {
    case 0: case 1: col = c[0] * 16; row = c[2] * 16; return;
    case 2: col = (b[0] + b[3] - c[0]) * 16; break; // -Z: right is -X
    case 3: col = c[0] * 16; break;                 // +Z: right is +X
    case 4: col = c[2] * 16; break;                 // -X: right is +Z
    case 5: col = (b[2] + b[5] - c[2]) * 16; break; // +X: right is -Z
    }
    row = (b[1] + b[4] - c[1]) * 16;
}

bool sameLiquid(uint8_t a, uint8_t b) {
    auto grp = [](uint8_t id) { return (id == 8 || id == 9) ? 1 : (id == 10 || id == 11) ? 2 : 0; };
    return grp(a) != 0 && grp(a) == grp(b);
}

// Block.shouldSideBeRendered (+ BlockBreakable / BlockFluids rules)
bool sideVisible(uint8_t id, int side, const Box& bx, int nx, int ny, int nz) {
    int n = pidx(nx, ny, nz);
    if (pad.unloaded[n]) return false;
    uint8_t nid = pad.id[n];
    const BlockInfo& bi = BLOCKS[id];
    if ((bi.flags & BF_LIQUID) && sameLiquid(id, nid)) return false;
    const float* b = bx.b;
    bool inside = (side == 0 && b[1] > 0) || (side == 1 && b[4] < 1) || (side == 2 && b[2] > 0) ||
                  (side == 3 && b[5] < 1) || (side == 4 && b[0] > 0) || (side == 5 && b[3] < 1);
    if (inside) return true;
    if ((bi.flags & BF_LIQUID) && side == 1) return true;
    if ((bi.flags & BF_CULL_SAME) && nid == id) return false;
    return !blockOpaque(nid);
}

// One box with per-side textures. (px,py,pz) = padded coords of the block.
void emitBox(int px, int py, int pz, uint8_t id, const Box& bx, const uint16_t tex[6], bool cull = true, int skip = -1) {
    const int ownLight = pad.light[pidx(px, py, pz)];
    for (int side = 0; side < 6; side++) {
        if (side == skip) continue;
        const int* nrm = NORMAL[side];
        int nx = px + nrm[0], ny = py + nrm[1], nz = pz + nrm[2];
        if (cull && !sideVisible(id, side, bx, nx, ny, nz)) continue;
        const float* b = bx.b;
        bool onEdge = (side == 0 && b[1] <= 0) || (side == 1 && b[4] >= 1) || (side == 2 && b[2] <= 0) ||
                      (side == 3 && b[5] >= 1) || (side == 4 && b[0] <= 0) || (side == 5 && b[3] >= 1);
        int light = onEdge ? pad.light[pidx(nx, ny, nz)] : ownLight;
        int t = tex[side];
        V q[4];
        for (int k = 0; k < 4; k++) {
            float c[3];
            for (int a = 0; a < 3; a++) c[a] = CORNERS[side][k][a] ? b[a + 3] : b[a];
            float col, row;
            faceUV(side, bx, c, col, row);
            q[k].x = int16_t(((px - 1) * 64) + int(std::lround(c[0] * 64)));
            q[k].y = int16_t(((py - 1) * 64) + int(std::lround(c[1] * 64)));
            q[k].z = int16_t(((pz - 1) * 64) + int(std::lround(c[2] * 64)));
            q[k].u = texU(t, col);
            q[k].v = texV(t, row);
        }
        emitQuad(q, shadeFor(FACE_SHADE[side], light), side, false);
    }
}

void emitBoxSameTex(int px, int py, int pz, uint8_t id, const Box& bx, int texIndex, bool cull = true) {
    uint16_t t[6] = {uint16_t(texIndex), uint16_t(texIndex), uint16_t(texIndex),
                     uint16_t(texIndex), uint16_t(texIndex), uint16_t(texIndex)};
    emitBox(px, py, pz, id, bx, t, cull);
}

// A free quad in block units (plants, rails, wire...), full tile, own-cell brightness.
void emitFlat(int px, int py, int pz, const float p[4][3], int tex, float factor, int group, bool twoSided) {
    static const float UV[4][2] = {{0, 16}, {16, 16}, {16, 0}, {0, 0}}; // bottom-left, bottom-right, top-right, top-left
    V q[4];
    for (int k = 0; k < 4; k++) {
        q[k].x = int16_t((px - 1) * 64 + int(std::lround(p[k][0] * 64)));
        q[k].y = int16_t((py - 1) * 64 + int(std::lround(p[k][1] * 64)));
        q[k].z = int16_t((pz - 1) * 64 + int(std::lround(p[k][2] * 64)));
        q[k].u = texU(tex, UV[k][0]);
        q[k].v = texV(tex, UV[k][1]);
    }
    emitQuad(q, shadeFor(factor, pad.light[pidx(px, py, pz)]), group, twoSided);
}

// A free quad with its own texture coordinates (0..16 inside the tile)
void emitUV(int px, int py, int pz, const float p[4][3], const float uv[4][2], int tex, float factor, int group, bool twoSided) {
    V q[4];
    for (int k = 0; k < 4; k++) {
        q[k].x = int16_t((px - 1) * 64 + int(std::lround(p[k][0] * 64)));
        q[k].y = int16_t((py - 1) * 64 + int(std::lround(p[k][1] * 64)));
        q[k].z = int16_t((pz - 1) * 64 + int(std::lround(p[k][2] * 64)));
        q[k].u = texU(tex, uv[k][0]);
        q[k].v = texV(tex, uv[k][1]);
    }
    emitQuad(q, shadeFor(factor, pad.light[pidx(px, py, pz)]), group, twoSided);
}

// BlockRedstoneWire.isPowerProviderOrWire, from the padded copy
bool rsProvider(uint8_t id) {
    switch (id) {
    case 55: case 69: case 70: case 72: case 75: case 76: case 77: case 93: case 94: case 154: case 224: case 226:
        return true;
    default: return false;
    }
}
bool padOpaque(int px, int py, int pz) { return blockOpaque(pad.id[pidx(px, py, pz)]); }

// RenderBlocks.renderBlockRedstoneWire: a cross trimmed to its links, a line when it runs straight,
// and a strip up any wall it climbs
void emitRedstone(int px, int py, int pz, int tex) {
    auto at = [&](int dx, int dy, int dz) { return pad.id[pidx(px + dx, py + dy, pz + dz)]; };
    bool xn = rsProvider(at(-1, 0, 0)) || (!padOpaque(px - 1, py, pz) && rsProvider(at(-1, -1, 0)));
    bool xp = rsProvider(at(1, 0, 0)) || (!padOpaque(px + 1, py, pz) && rsProvider(at(1, -1, 0)));
    bool zn = rsProvider(at(0, 0, -1)) || (!padOpaque(px, py, pz - 1) && rsProvider(at(0, -1, -1)));
    bool zp = rsProvider(at(0, 0, 1)) || (!padOpaque(px, py, pz + 1) && rsProvider(at(0, -1, 1)));
    bool openAbove = !padOpaque(px, py + 1, pz);
    if (openAbove) {
        if (padOpaque(px - 1, py, pz) && rsProvider(at(-1, 1, 0))) xn = true;
        if (padOpaque(px + 1, py, pz) && rsProvider(at(1, 1, 0))) xp = true;
        if (padOpaque(px, py, pz - 1) && rsProvider(at(0, 1, -1))) zn = true;
        if (padOpaque(px, py, pz + 1) && rsProvider(at(0, 1, 1))) zp = true;
    }
    const float y = 1.0f / 32.0f, k = 5.0f / 16.0f;
    int kind = (xn || xp) && !zn && !zp ? 1 : (zn || zp) && !xn && !xp ? 2 : 0;
    if (kind == 0) {
        float x0 = 0, x1 = 1, z0 = 0, z1 = 1;
        if (xn || xp || zn || zp) {
            if (!xn) x0 += k;
            if (!xp) x1 -= k;
            if (!zn) z0 += k;
            if (!zp) z1 -= k;
        }
        const float q[4][3] = {{x0, y, z1}, {x1, y, z1}, {x1, y, z0}, {x0, y, z0}};
        const float uv[4][2] = {{x0 * 16, z1 * 16}, {x1 * 16, z1 * 16}, {x1 * 16, z0 * 16}, {x0 * 16, z0 * 16}};
        emitUV(px, py, pz, q, uv, tex, 1.0f, 1, true);
    } else {
        const float q[4][3] = {{0, y, 1}, {1, y, 1}, {1, y, 0}, {0, y, 0}};
        const float a[4][2] = {{0, 16}, {16, 16}, {16, 0}, {0, 0}};    // u along x
        const float b[4][2] = {{16, 0}, {16, 16}, {0, 16}, {0, 0}};    // u along z
        emitUV(px, py, pz, q, kind == 1 ? a : b, tex + 1, 1.0f, 1, true);
    }
    if (!openAbove) return;
    const float e = 1.0f / 32.0f;
    // up the walls: the line texture standing on end
    const float vu[4][2] = {{16, 0}, {16, 16}, {0, 16}, {0, 0}};
    if (padOpaque(px - 1, py, pz) && at(-1, 1, 0) == 55) {
        const float q[4][3] = {{e, 0, 0}, {e, 0, 1}, {e, 1, 1}, {e, 1, 0}};
        emitUV(px, py, pz, q, vu, tex + 1, 1.0f, 1, true);
    }
    if (padOpaque(px + 1, py, pz) && at(1, 1, 0) == 55) {
        const float q[4][3] = {{1 - e, 0, 1}, {1 - e, 0, 0}, {1 - e, 1, 0}, {1 - e, 1, 1}};
        emitUV(px, py, pz, q, vu, tex + 1, 1.0f, 1, true);
    }
    if (padOpaque(px, py, pz - 1) && at(0, 1, -1) == 55) {
        const float q[4][3] = {{1, 0, e}, {0, 0, e}, {0, 1, e}, {1, 1, e}};
        emitUV(px, py, pz, q, vu, tex + 1, 1.0f, 1, true);
    }
    if (padOpaque(px, py, pz + 1) && at(0, 1, 1) == 55) {
        const float q[4][3] = {{0, 0, 1 - e}, {1, 0, 1 - e}, {1, 1, 1 - e}, {0, 1, 1 - e}};
        emitUV(px, py, pz, q, vu, tex + 1, 1.0f, 1, true);
    }
}

// RenderBlocks.renderLapisWire: like redstone, but it also lies flat on walls
bool lapisDust(uint8_t id) { return id == 95 || (id >= 234 && id <= 237); }
bool lapisConnects(uint8_t id) {
    return lapisDust(id) || id == 96 || id == 97 || id == 98 || id == 99 || id == 93 || id == 94 || id == 225 || id == 22;
}
int lapisWall(uint8_t id) { return id >= 234 && id <= 237 ? id - 234 : -1; }
void emitLapis(int px, int py, int pz, uint8_t id, int power) {
    auto at = [&](int dx, int dy, int dz) { return pad.id[pidx(px + dx, py + dy, pz + dz)]; };
    int cross = 496 + (power > 0 ? 2 : 0), line = cross + 1;
    const float e = 1.0f / 64.0f;
    int myWall = lapisWall(id);
    auto floorLink = [&](int dx, int dz) {
        if (lapisConnects(at(dx, 0, dz))) return true;
        int wi = dx < 0 ? 0 : dx > 0 ? 1 : dz < 0 ? 2 : 3;
        if (myWall == wi || lapisWall(at(0, 1, 0)) == wi) return true;
        if (!padOpaque(px + dx, py, pz + dz) && lapisWall(at(dx, -1, dz)) == (wi ^ 1)) return true;
        return myWall == wi && !padOpaque(px, py + 1, pz) && lapisDust(at(dx, 1, dz));
    };
    bool onFloor = padOpaque(px, py - 1, pz);
    if (onFloor) {
        bool xn = floorLink(-1, 0), xp = floorLink(1, 0), zn = floorLink(0, -1), zp = floorLink(0, 1);
        if ((xn || xp) && !zn && !zp) {
            const float q[4][3] = {{0, e, 1}, {1, e, 1}, {1, e, 0}, {0, e, 0}};
            const float a[4][2] = {{0, 16}, {16, 16}, {16, 0}, {0, 0}};
            emitUV(px, py, pz, q, a, line, 1.0f, 1, true);
        } else if ((zn || zp) && !xn && !xp) {
            const float q[4][3] = {{0, e, 1}, {1, e, 1}, {1, e, 0}, {0, e, 0}};
            const float b[4][2] = {{16, 0}, {16, 16}, {0, 16}, {0, 0}};
            emitUV(px, py, pz, q, b, line, 1.0f, 1, true);
        } else {
            bool any = xn || xp || zn || zp;
            const float k = 5.0f / 16.0f;
            float a0 = any && !xn ? k : 0, a1 = any && !xp ? 1 - k : 1, b0 = any && !zn ? k : 0, b1 = any && !zp ? 1 - k : 1;
            const float q[4][3] = {{a0, e, b1}, {a1, e, b1}, {a1, e, b0}, {a0, e, b0}};
            const float uv[4][2] = {{a0 * 16, b1 * 16}, {a1 * 16, b1 * 16}, {a1 * 16, b0 * 16}, {a0 * 16, b0 * 16}};
            emitUV(px, py, pz, q, uv, cross, 1.0f, 1, true);
        }
    }
    static const int DIRS[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        int dx = DIRS[i][0], dz = DIRS[i][1];
        if (!padOpaque(px + dx, py, pz + dz)) continue;
        int tex;
        bool upright;
        if (onFloor || myWall >= 0) {
            if (i != myWall && lapisWall(at(0, 1, 0)) != i) continue;
            tex = line;
            upright = true;
        } else {
            bool vert = lapisDust(at(0, 1, 0)) || lapisDust(at(0, -1, 0)) || lapisDust(at(dx, 1, dz));
            int sx = dz != 0 ? 1 : 0, sz = dx != 0 ? 1 : 0;
            bool side = lapisConnects(at(-sx, 0, -sz)) || lapisConnects(at(sx, 0, sz));
            tex = (vert != side) ? line : cross;
            upright = vert && !side;
        }
        float w = dx < 0 ? e : dx > 0 ? 1 - e : 0, wz = dz < 0 ? e : dz > 0 ? 1 - e : 0;
        float q[4][3];
        if (dx != 0) {
            const float t[4][3] = {{w, 0, 0}, {w, 0, 1}, {w, 1, 1}, {w, 1, 0}};
            std::memcpy(q, t, sizeof q);
        } else {
            const float t[4][3] = {{0, 0, wz}, {1, 0, wz}, {1, 1, wz}, {0, 1, wz}};
            std::memcpy(q, t, sizeof q);
        }
        const float flat[4][2] = {{0, 16}, {16, 16}, {16, 0}, {0, 0}};
        const float up[4][2] = {{16, 0}, {16, 16}, {0, 16}, {0, 0}};
        emitUV(px, py, pz, q, upright ? up : flat, tex, 1.0f, 1, true);
    }
}

// RenderBlocks.renderPowerRepeater: a slab, its top turned to face the front, and two little torches
void emitRepeater(int px, int py, int pz, uint8_t id, int meta) {
    const bool lit = id == 94;
    const int SIDE = 443, TOP = lit ? 435 : 434, RS_TORCH = lit ? 444 : 445, LAPIS_TORCH = lit ? 436 : 437;
    Box slab{{0, 0, 0, 1, 0.125f, 1}};
    const uint16_t t[6] = {SIDE, SIDE, SIDE, SIDE, SIDE, SIDE};
    emitBox(px, py, pz, id, slab, t, true, 1); // the top is drawn turned, below
    int f = meta & 3;
    int turn = f == 2 ? 0 : f == 3 ? 1 : f == 0 ? 2 : 3;
    const float yy = 0.125f;
    const float c[4][3] = {{0, yy, 0}, {1, yy, 0}, {1, yy, 1}, {0, yy, 1}}; // NW NE SE SW
    const float uv[4][2] = {{0, 0}, {16, 0}, {16, 16}, {0, 16}};          // TL TR BR BL
    float q[4][3], u[4][2];
    static const int ORDER[4] = {3, 2, 1, 0}; // counter-clockwise seen from above
    for (int n = 0; n < 4; ++n) {
        int i = ORDER[n];
        std::memcpy(q[n], c[i], sizeof q[n]);
        std::memcpy(u[n], uv[(i - turn + 4) % 4], sizeof u[n]);
    }
    emitUV(px, py, pz, q, u, TOP, 1.0f, 1, false);
    int fx = f == 1 ? -1 : f == 3 ? 1 : 0, fz = f == 0 ? 1 : f == 2 ? -1 : 0;
    float fr = 0.25f - float(((meta >> 2) & 3)) * 0.125f, bk = -0.3125f;
    const float s = 1.0f / 16.0f;
    auto torch = [&](float ox, float oz, int tex) {
        Box tb{{0.5f - s + ox, -0.1875f, 0.5f - s + oz, 0.5f + s + ox, 10 * s - 0.1875f, 0.5f + s + oz}};
        emitBoxSameTex(px, py, pz, id, tb, tex, false);
    };
    torch(fx * bk, fz * bk, RS_TORCH);
    torch(fx * fr, fz * fr, LAPIS_TORCH);
}

// RenderBlocks.renderCrossedSquares: two diagonal, double-sided quads
void emitCross(int px, int py, int pz, int tex, float h = 1.0f) {
    const float a = 0.05f, b = 0.95f;
    const float q1[4][3] = {{a, 0, a}, {b, 0, b}, {b, h, b}, {a, h, a}};
    const float q2[4][3] = {{a, 0, b}, {b, 0, a}, {b, h, a}, {a, h, b}};
    emitFlat(px, py, pz, q1, tex, 1.0f, 6, true);
    emitFlat(px, py, pz, q2, tex, 1.0f, 6, true);
}

// RenderBlocks.renderBlockCropsImpl: '#' pattern
void emitCrops(int px, int py, int pz, int tex) {
    const float l = 0.25f, r = 0.75f, h = 1.0f - 1.0f / 16.0f;
    const float qs[4][4][3] = {
        {{l, 0, 0}, {l, 0, 1}, {l, h, 1}, {l, h, 0}}, {{r, 0, 0}, {r, 0, 1}, {r, h, 1}, {r, h, 0}},
        {{0, 0, l}, {1, 0, l}, {1, h, l}, {0, h, l}}, {{0, 0, r}, {1, 0, r}, {1, h, r}, {0, h, r}},
    };
    for (auto& q : qs) emitFlat(px, py, pz, q, tex, 1.0f, 6, true);
}

// ---------------------------------------------------------------- Axeolon stairs
// Port of BlockStairs.getStairBoxes (modern stair shapes with corners).
const int DIR_X[4] = {1, -1, 0, 0};
const int DIR_Z[4] = {0, 0, 1, -1};
constexpr int UPSIDE_DOWN = 4;

int stairMetaAt(int px, int py, int pz) {
    int i = pidx(px, py, pz);
    return BLOCKS[pad.id[i]].render == RenderType::Stairs ? pad.meta[i] : -1;
}
bool sameHalf(int a, int b) { return (a & UPSIDE_DOWN) == (b & UPSIDE_DOWN); }
bool isPerpendicular(int a, int b) { return (a >> 1) != (b >> 1); }
bool canTakeShape(int px, int py, int pz, int meta, int dir) {
    int other = stairMetaAt(px + DIR_X[dir], py, pz + DIR_Z[dir]);
    return other < 0 || (other & 3) != (meta & 3) || !sameHalf(meta, other);
}
struct Quarters { bool q[2][2]; };
Quarters halfOn(int facing) {
    Quarters r;
    for (int xi = 0; xi < 2; xi++)
        for (int zi = 0; zi < 2; zi++)
            r.q[xi][zi] = (DIR_X[facing] > 0 && xi == 1) || (DIR_X[facing] < 0 && xi == 0) ||
                          (DIR_Z[facing] > 0 && zi == 1) || (DIR_Z[facing] < 0 && zi == 0);
    return r;
}

int stairBoxes(int px, int py, int pz, Box* boxes) {
    int meta = pad.meta[pidx(px, py, pz)];
    // Stair blocks at the padded border can't see their own neighbours; that's fine (only shape, not culling)
    int facing = meta & 3;
    bool upsideDown = (meta & UPSIDE_DOWN) != 0;
    float baseMin = upsideDown ? 0.5f : 0.0f, stepMin = upsideDown ? 0.0f : 0.5f;
    int n = 0;
    boxes[n++] = {{0, baseMin, 0, 1, baseMin + 0.5f, 1}};
    Quarters step = halfOn(facing);
    bool shaped = false;
    bool inner = px > 0 && px < P - 1 && pz > 0 && pz < P - 1;
    if (inner) {
        int behind = stairMetaAt(px + DIR_X[facing], py, pz + DIR_Z[facing]);
        if (behind >= 0 && sameHalf(meta, behind)) {
            int bf = behind & 3;
            if (isPerpendicular(facing, bf) && canTakeShape(px, py, pz, meta, bf ^ 1)) {
                Quarters h = halfOn(bf);
                for (int a = 0; a < 2; a++) for (int c = 0; c < 2; c++) step.q[a][c] = step.q[a][c] && h.q[a][c];
                shaped = true;
            }
        }
        if (!shaped) {
            int front = stairMetaAt(px - DIR_X[facing], py, pz - DIR_Z[facing]);
            if (front >= 0 && sameHalf(meta, front)) {
                int ff = front & 3;
                if (isPerpendicular(facing, ff) && canTakeShape(px, py, pz, meta, ff)) {
                    Quarters h = halfOn(ff);
                    for (int a = 0; a < 2; a++) for (int c = 0; c < 2; c++) step.q[a][c] = step.q[a][c] || h.q[a][c];
                }
            }
        }
    }
    // addQuadrantBoxes
    bool left[2][2] = {{step.q[0][0], step.q[0][1]}, {step.q[1][0], step.q[1][1]}};
    float y0 = stepMin, y1 = stepMin + 0.5f;
    for (int xi = 0; xi < 2; xi++)
        if (left[xi][0] && left[xi][1]) {
            boxes[n++] = {{xi * 0.5f, y0, 0, xi * 0.5f + 0.5f, y1, 1}};
            left[xi][0] = left[xi][1] = false;
        }
    for (int zi = 0; zi < 2; zi++)
        if (left[0][zi] && left[1][zi]) {
            boxes[n++] = {{0, y0, zi * 0.5f, 1, y1, zi * 0.5f + 0.5f}};
            left[0][zi] = left[1][zi] = false;
        }
    for (int xi = 0; xi < 2; xi++)
        for (int zi = 0; zi < 2; zi++)
            if (left[xi][zi]) boxes[n++] = {{xi * 0.5f, y0, zi * 0.5f, xi * 0.5f + 0.5f, y1, zi * 0.5f + 0.5f}};
    return n;
}

// ---------------------------------------------------------------- RenderBlocks.renderBlockFluids
int liquidGroup(uint8_t id) { return (id == 8 || id == 9) ? 1 : (id == 10 || id == 11) ? 2 : 0; }

// BlockFluids.func_289_b: this liquid's level here (0 full .. 7), -1 if it's not this liquid
int fluidLevel(int grp, int x, int y, int z) {
    int i = pidx(x, y, z);
    if (liquidGroup(pad.id[i]) != grp) return -1;
    int m = pad.meta[i];
    return m >= 8 ? 0 : m;
}

float fluidHeightOf(int m) { // BlockFluids.setFluidHeight
    if (m >= 8) m = 0;
    return float(m + 1) / 9.0f;
}

// func_1224_a: a corner's height, from the four cells round it
float fluidCorner(int grp, int x, int y, int z) {
    int n = 0;
    float sum = 0.0f;
    for (int k = 0; k < 4; ++k) {
        int cx = x - (k & 1), cz = z - ((k >> 1) & 1);
        if (liquidGroup(pad.id[pidx(cx, y + 1, cz)]) == grp) return 1.0f;
        int i = pidx(cx, y, cz);
        uint8_t nid = pad.id[i];
        if (liquidGroup(nid) != grp) {
            if (!matSolid(BP[nid].mat)) {
                sum += 1.0f;
                ++n;
            }
        } else {
            int m = pad.meta[i];
            if (m >= 8 || m == 0) {
                sum += fluidHeightOf(m) * 10.0f;
                n += 10;
            }
            sum += fluidHeightOf(m);
            ++n;
        }
    }
    return 1.0f - sum / float(n);
}

// BlockFluids.shouldSideBeRendered
bool fluidFace(uint8_t id, int side, int nx, int ny, int nz) {
    int i = pidx(nx, ny, nz);
    if (pad.unloaded[i]) return false;
    uint8_t nid = pad.id[i];
    if (sameLiquid(id, nid) || nid == 79) return false;
    if (side == 1) return true;
    return !blockOpaque(nid);
}

// func_293_a: which way it flows (radians, for turning the texture), or false when it's still
bool fluidFlow(int grp, int x, int y, int z, uint8_t id, float& angle) {
    double vx = 0.0, vz = 0.0;
    int own = fluidLevel(grp, x, y, z);
    static const int D[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};
    for (auto& d : D) {
        int ax = x + d[0], az = z + d[1];
        int lv = fluidLevel(grp, ax, y, az);
        if (lv < 0) {
            if (!matSolid(BP[pad.id[pidx(ax, y, az)]].mat)) {
                lv = fluidLevel(grp, ax, y - 1, az);
                if (lv >= 0) {
                    int k = lv - (own - 8);
                    vx += d[0] * k;
                    vz += d[1] * k;
                }
            }
        } else {
            int k = lv - own;
            vx += d[0] * k;
            vz += d[1] * k;
        }
    }
    // (falling water's push down doesn't turn the texture, so it's left out)
    double l = std::sqrt(vx * vx + vz * vz);
    (void)id;
    if (l < 1.0e-4 || (vx == 0.0 && vz == 0.0)) return false;
    angle = float(std::atan2(vz, vx) - 3.141592653589793 * 0.5);
    return true;
}

void emitFluid(int px, int py, int pz, uint8_t id, int meta, const uint16_t tex[6]) {
    int grp = liquidGroup(id);
    bool top = fluidFace(id, 1, px, py + 1, pz), bottom = fluidFace(id, 0, px, py - 1, pz);
    bool sides[4] = {fluidFace(id, 2, px, py, pz - 1), fluidFace(id, 3, px, py, pz + 1), fluidFace(id, 4, px - 1, py, pz),
                     fluidFace(id, 5, px + 1, py, pz)};
    if (!top && !bottom && !sides[0] && !sides[1] && !sides[2] && !sides[3]) return;
    float h00 = fluidCorner(grp, px, py, pz), h01 = fluidCorner(grp, px, py, pz + 1);
    float h11 = fluidCorner(grp, px + 1, py, pz + 1), h10 = fluidCorner(grp, px + 1, py, pz);
    if (top) {
        // still: the top tile; flowing: the 2 x 2 flow texture, turned to run downstream
        float angle = 0.0f;
        bool flowing = fluidFlow(grp, px, py, pz, id, angle);
        int t = flowing ? tex[2] : tex[1];
        float c = flowing ? 16.0f : 8.0f;
        float s = std::sin(angle) * 8.0f, co = std::cos(angle) * 8.0f;
        const float p[4][3] = {{0, h00, 0}, {0, h01, 1}, {1, h11, 1}, {1, h10, 0}};
        const float uv[4][2] = {{c - co - s, c - co + s}, {c - co + s, c + co + s}, {c + co + s, c + co - s}, {c + co - s, c - co - s}};
        emitUV(px, py, pz, p, uv, t, 1.0f, 1, false);
    }
    if (bottom) {
        const float p[4][3] = {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}};
        const float uv[4][2] = {{0, 16}, {0, 0}, {16, 0}, {16, 16}};
        V q[4];
        for (int k = 0; k < 4; k++) {
            q[k].x = int16_t((px - 1) * 64 + int(std::lround(p[k][0] * 64)));
            q[k].y = int16_t((py - 1) * 64 + int(std::lround(p[k][1] * 64)));
            q[k].z = int16_t((pz - 1) * 64 + int(std::lround(p[k][2] * 64)));
            q[k].u = texU(tex[0], uv[k][0]);
            q[k].v = texV(tex[0], uv[k][1]);
        }
        emitQuad(q, shadeFor(0.5f, pad.light[pidx(px, py - 1, pz)]), 0, false);
    }
    for (int k = 0; k < 4; ++k) {
        if (!sides[k]) continue;
        float ha, hb, x0, x1, z0, z1;
        int nx = px, nz = pz;
        if (k == 0) { ha = h00; hb = h10; x0 = 0; x1 = 1; z0 = 0; z1 = 0; nz = pz - 1; }
        else if (k == 1) { ha = h11; hb = h01; x0 = 1; x1 = 0; z0 = 1; z1 = 1; nz = pz + 1; }
        else if (k == 2) { ha = h01; hb = h00; x0 = 0; x1 = 0; z0 = 1; z1 = 0; nx = px - 1; }
        else { ha = h10; hb = h11; x0 = 1; x1 = 1; z0 = 0; z1 = 1; nx = px + 1; }
        int t = tex[k + 2];
        const float p[4][3] = {{x0, ha, z0}, {x1, hb, z1}, {x1, 0, z1}, {x0, 0, z0}};
        const float uv[4][2] = {{0, (1.0f - ha) * 16.0f}, {16, (1.0f - hb) * 16.0f}, {16, 16}, {0, 16}};
        V q[4];
        for (int j = 0; j < 4; j++) {
            q[j].x = int16_t((px - 1) * 64 + int(std::lround(p[j][0] * 64)));
            q[j].y = int16_t((py - 1) * 64 + int(std::lround(p[j][1] * 64)));
            q[j].z = int16_t((pz - 1) * 64 + int(std::lround(p[j][2] * 64)));
            q[j].u = texU(t, uv[j][0]);
            q[j].v = texV(t, uv[j][1]);
        }
        emitQuad(q, shadeFor(k < 2 ? 0.8f : 0.6f, pad.light[pidx(nx, py, nz)]), k + 2, false);
    }
    (void)meta;
}

// ---------------------------------------------------------------- per block
void renderBlock(int px, int py, int pz) {
    int i = pidx(px, py, pz);
    uint8_t id = pad.id[i];
    const BlockInfo& bi = BLOCKS[id];
    if (!(bi.flags & BF_EXISTS)) return;
    if (BlockModels::isModelBlock(id)) { emitModelBlock(px, py, pz); return; }
    int meta = pad.meta[i];
    const uint16_t* tex = bi.tex[meta & 15];
    Box box{{bi.bounds[0], bi.bounds[1], bi.bounds[2], bi.bounds[3], bi.bounds[4], bi.bounds[5]}};

    switch (bi.render) {
    case RenderType::Cube: {
        uint16_t t[6] = {tex[0], tex[1], tex[2], tex[3], tex[4], tex[5]};
        if (id == 2) { // BlockGrass.getBlockTexture: top 0, bottom 2, sides 3 (68 under snow)
            uint8_t above = pad.id[pidx(px, py + 1, pz)];
            t[0] = 2; t[1] = 0;
            uint16_t side = (above == 78 || above == 80) ? 68 : 3;
            t[2] = t[3] = t[4] = t[5] = side;
        }
        if (bi.flags & BF_SLAB) { // BlockStep.setBlockBoundsBasedOnState: meta & TOP
            if (meta & 8) { box.b[1] = 0.5f; box.b[4] = 1.0f; } else { box.b[1] = 0; box.b[4] = 0.5f; }
        }
        if (id == 78) box.b[4] = float(2 * (1 + (meta & 7))) / 16.0f; // BlockSnow layer height
        if (id == 92) box = Box{{float(1 + meta * 2) / 16.0f, 0, 1.0f / 16.0f, 15.0f / 16.0f, 0.5f, 15.0f / 16.0f}}; // BlockCake: eaten from the west
        if (id == 90) { // BlockPortal.setBlockBoundsBasedOnState: a sheet 4 pixels thick, every side drawn
            const float th = 2.0f / 16.0f;
            bool alongX = pad.id[pidx(px - 1, py, pz)] == 90 || pad.id[pidx(px + 1, py, pz)] == 90;
            if (alongX) box = Box{{0, 0, 0.5f - th, 1, 1, 0.5f + th}};
            else box = Box{{0.5f - th, 0, 0, 0.5f + th, 1, 1}};
            emitBox(px, py, pz, id, box, t, false);
            break;
        }
        emitBox(px, py, pz, id, box, t);
        break;
    }
    case RenderType::Cross:
        emitCross(px, py, pz, tex[0]);
        break;
    case RenderType::Crops:
        emitCrops(px, py, pz, tex[0]);
        break;
    case RenderType::Torch: {
        // RenderBlocks.renderBlockTorch: tilted against a wall for meta 1-4
        const float s = 1.0f / 16.0f;
        float ox = 0, oz = 0, oy = 0;
        if (meta == 1) { ox = -0.4f; oy = 0.2f; } else if (meta == 2) { ox = 0.4f; oy = 0.2f; }
        else if (meta == 3) { oz = -0.4f; oy = 0.2f; } else if (meta == 4) { oz = 0.4f; oy = 0.2f; }
        Box tb{{0.5f - s + ox, oy, 0.5f - s + oz, 0.5f + s + ox, 10 * s + oy, 0.5f + s + oz}};
        emitBoxSameTex(px, py, pz, id, tb, tex[0], false);
        break;
    }
    case RenderType::Liquid: {
        emitFluid(px, py, pz, id, meta, tex);
        break;
    }
    case RenderType::Stairs: {
        Box boxes[6];
        int n = stairBoxes(px, py, pz, boxes);
        for (int k = 0; k < n; k++) emitBox(px, py, pz, id, boxes[k], tex);
        break;
    }
    case RenderType::Fence: {
        const float a = 6 / 16.0f, b = 10 / 16.0f;
        emitBoxSameTex(px, py, pz, id, Box{{a, 0, a, b, 1, b}}, tex[0]);
        // bars towards neighbouring fences (+X, +Z only so each bar is drawn once)
        const float lo[2][2] = {{6 / 16.0f, 9 / 16.0f}, {12 / 16.0f, 15 / 16.0f}};
        const float t0 = 7 / 16.0f, t1 = 9 / 16.0f;
        for (auto& yr : lo) {
            if (BLOCKS[pad.id[pidx(px + 1, py, pz)]].render == RenderType::Fence)
                emitBoxSameTex(px, py, pz, id, Box{{b, yr[0], t0, 1 + a, yr[1], t1}}, tex[0], false);
            if (BLOCKS[pad.id[pidx(px, py, pz + 1)]].render == RenderType::Fence)
                emitBoxSameTex(px, py, pz, id, Box{{t0, yr[0], b, t1, yr[1], 1 + a}}, tex[0], false);
        }
        break;
    }
    case RenderType::Door: {
        // BlockDoor.func_313_b(func_312_c(meta)); top half uses the top texture
        int rot = (meta & 4) == 0 ? ((meta - 1) & 3) : (meta & 3);
        const float t = 3 / 16.0f;
        Box db{{0, 0, 0, 1, 1, 1}};
        if (rot == 0) db.b[5] = t; else if (rot == 1) db.b[0] = 1 - t; else if (rot == 2) db.b[2] = 1 - t; else db.b[3] = t;
        emitBox(px, py, pz, id, db, tex);
        break;
    }
    case RenderType::Ladder: {
        const float t = 2 / 16.0f;
        Box lb{{0, 0, 0, 1, 1, 1}};
        if (meta == 2) lb.b[2] = 1 - t; else if (meta == 3) lb.b[5] = t; else if (meta == 4) lb.b[0] = 1 - t; else lb.b[3] = t;
        emitBoxSameTex(px, py, pz, id, lb, tex[0], false);
        break;
    }
    case RenderType::Wire:
        if (id == 55) { emitRedstone(px, py, pz, tex[1]); break; }
        [[fallthrough]];
    case RenderType::Rail: {
        const float y = 1 / 64.0f;
        const float q[4][3] = {{0, y, 1}, {1, y, 1}, {1, y, 0}, {0, y, 0}};
        emitFlat(px, py, pz, q, tex[1], 1.0f, 1, true);
        break;
    }
    case 34: // BlockLapisWire.RENDER_TYPE
        emitLapis(px, py, pz, id, meta);
        break;
    case 35: // BlockPowerRepeater.RENDER_TYPE
        emitRepeater(px, py, pz, id, meta);
        break;
    case RenderType::Fire: {
        // RenderBlocks.renderBlockFire: a tent of leaning flames on solid or burnable ground,
        // otherwise sheets leaning out from each burnable wall (and under a burnable ceiling)
        auto burnable = [&](int dx, int dy, int dz) {
            uint8_t n = pad.id[pidx(px + dx, py + dy, pz + dz)];
            return n == 5 || n == 17 || n == 18 || n == 47 || n == 46 || n == 35;
        };
        int ta = tex[0], tb = tex[0] + 16;
        const float f = 1.4f;
        if (blockOpaque(pad.id[pidx(px, py - 1, pz)]) || burnable(0, -1, 0)) {
            // outer pair leans in to the middle, inner pair from the edges
            const float A = 0.7f, B = 0.2f, C = 0.3f, D = 0.8f;
            const float q1[4][3] = {{A, 0, 0}, {A, 0, 1}, {B, f, 1}, {B, f, 0}};
            const float q2[4][3] = {{C, 0, 1}, {C, 0, 0}, {D, f, 0}, {D, f, 1}};
            const float q3[4][3] = {{1, 0, A}, {0, 0, A}, {0, f, B}, {1, f, B}};
            const float q4[4][3] = {{0, 0, C}, {1, 0, C}, {1, f, D}, {0, f, D}};
            const float q5[4][3] = {{0, 0, 0}, {0, 0, 1}, {0.9f, f, 1}, {0.9f, f, 0}};
            const float q6[4][3] = {{1, 0, 1}, {1, 0, 0}, {0.1f, f, 0}, {0.1f, f, 1}};
            const float q7[4][3] = {{1, 0, 0}, {0, 0, 0}, {0, f, 0.9f}, {1, f, 0.9f}};
            const float q8[4][3] = {{0, 0, 1}, {1, 0, 1}, {1, f, 0.1f}, {0, f, 0.1f}};
            emitFlat(px, py, pz, q1, ta, 1.0f, 6, true);
            emitFlat(px, py, pz, q2, ta, 1.0f, 6, true);
            emitFlat(px, py, pz, q3, tb, 1.0f, 6, true);
            emitFlat(px, py, pz, q4, tb, 1.0f, 6, true);
            emitFlat(px, py, pz, q5, ta, 1.0f, 6, true);
            emitFlat(px, py, pz, q6, ta, 1.0f, 6, true);
            emitFlat(px, py, pz, q7, tb, 1.0f, 6, true);
            emitFlat(px, py, pz, q8, tb, 1.0f, 6, true);
        } else {
            const float o = 1.0f / 16.0f, w = 0.2f, h = f + o;
            int t = ((px + py + pz) & 1) ? tb : ta;
            if (burnable(-1, 0, 0)) { const float q[4][3] = {{0, o, 1}, {0, o, 0}, {w, h, 0}, {w, h, 1}}; emitFlat(px, py, pz, q, t, 1.0f, 6, true); }
            if (burnable(1, 0, 0)) { const float q[4][3] = {{1, o, 0}, {1, o, 1}, {1 - w, h, 1}, {1 - w, h, 0}}; emitFlat(px, py, pz, q, t, 1.0f, 6, true); }
            if (burnable(0, 0, -1)) { const float q[4][3] = {{0, o, 0}, {1, o, 0}, {1, h, w}, {0, h, w}}; emitFlat(px, py, pz, q, t, 1.0f, 6, true); }
            if (burnable(0, 0, 1)) { const float q[4][3] = {{1, o, 1}, {0, o, 1}, {0, h, 1 - w}, {1, h, 1 - w}}; emitFlat(px, py, pz, q, t, 1.0f, 6, true); }
            if (burnable(0, 1, 0)) {
                const float y0 = 1.0f - 0.2f; // up under the ceiling
                const float q[4][3] = {{0, y0, 0}, {1, y0, 0}, {1, 1, 1}, {0, 1, 1}};
                const float r[4][3] = {{1, y0, 1}, {0, y0, 1}, {0, 1, 0}, {1, 1, 0}};
                emitFlat(px, py, pz, q, ta, 1.0f, 6, true);
                emitFlat(px, py, pz, r, tb, 1.0f, 6, true);
            }
        }
        break;
    }
    case RenderType::Cactus: {
        // renderBlockCactus: sides inset by 1/16
        const float s = 1 / 16.0f;
        emitBox(px, py, pz, id, Box{{s, 0, s, 1 - s, 1, 1 - s}}, tex);
        break;
    }
    default:
        // TODO(port): remaining RenderBlocks shapes (fire, lever, repeater, pistons,
        // hopper, flower pot, icicle, Axeolon mixed slabs) and tile-entity models
        // (chests, signs...) from the .bbmodel files. Bounds box for now.
        emitBox(px, py, pz, id, box, tex);
        break;
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

    const int bx = cx * 16, by = sy * 16, bz = cz * 16;
    curWorld = &world;
    secX = bx; secY = by; secZ = bz;
    curSub = world.skylightSubtracted;
    const bool animating = BlockModels::anyActive();
    bool any = false;
    for (int z = 0; z < P; z++) {
        int wz = bz + z - 1;
        for (int x = 0; x < P; x++) {
            int wx = bx + x - 1;
            const Chunk* c = world.getChunk(wx >> 4, wz >> 4);
            const bool dyn = dynlight::any();
            for (int y = 0; y < P; y++) {
                int wy = by + y - 1, i = pidx(x, y, z);
                if (!c) { pad.id[i] = 0; pad.meta[i] = 0; pad.light[i] = 0xF0; pad.unloaded[i] = 1; continue; }
                pad.unloaded[i] = 0;
                if (wy < 0 || wy >= Chunk::HEIGHT) {
                    pad.id[i] = 0; pad.meta[i] = 0;
                    pad.light[i] = wy < 0 ? 0 : 0xF0;
                    continue;
                }
                int lx = wx & 15, lz = wz & 15;
                uint8_t id = c->getBlockID(lx, wy, lz);
                // AxeolonBlockAnims.hides: an animating block is drawn on its own and doesn't fill its cell
                if (animating && id && BlockModels::hides(wx, wy, wz)) id = 0;
                pad.id[i] = id;
                pad.meta[i] = uint8_t(c->getMetadata(lx, wy, lz));
                pad.light[i] = uint8_t(c->getSkyLight(lx, wy, lz) << 4 | c->getBlockLight(lx, wy, lz));
                if (dyn) { // AxeolonDynamicLights.lightAt: the brighter of the real and the moving light
                    int d = dynlight::lightAt(wx, wy, wz);
                    if (d > (pad.light[i] & 15)) pad.light[i] = uint8_t((pad.light[i] & 0xF0) | d);
                }
                if (id && x > 0 && x < 17 && y > 0 && y < 17 && z > 0 && z < 17) any = true;
            }
        }
    }
    // Beta getBlockLightValue_do: slabs (stairSingle) and farmland take the brightest neighbour
    if (any) {
        for (int y = 1; y <= 16; y++)
            for (int z = 1; z <= 16; z++)
                for (int x = 1; x <= 16; x++) {
                    int i = pidx(x, y, z);
                    uint8_t id = pad.id[i];
                    if (!(BLOCKS[id].flags & BF_SLAB) && id != 60) continue;
                    // the brightest of the five, in sky and block separately (max commutes with
                    // the shader's max(sky - subtracted, block))
                    const int n[5] = {pidx(x, y + 1, z), pidx(x + 1, y, z), pidx(x - 1, y, z), pidx(x, y, z + 1),
                                      pidx(x, y, z - 1)};
                    int sky = 0, blk = 0;
                    for (int k : n) {
                        sky = std::max(sky, pad.light[k] >> 4);
                        blk = std::max(blk, pad.light[k] & 15);
                    }
                    pad.light[i] = uint8_t(sky << 4 | blk);
                }
    }

    if (any) {
        for (int y = 1; y <= 16 && !full; y++)
            for (int z = 1; z <= 16 && !full; z++)
                for (int x = 1; x <= 16 && !full; x++)
                    if (pad.id[pidx(x, y, z)]) renderBlock(x, y, z);
    }

    for (int f = 0; f < 7; f++) {
        data.faceStart[f] = uint32_t(data.indices.size());
        data.indices.insert(data.indices.end(), groups[f].begin(), groups[f].end());
    }
    data.faceStart[7] = uint32_t(data.indices.size());
}
