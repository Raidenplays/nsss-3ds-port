#include "Physics.h"
#include <cmath>
#include "BlockTable.h"
#include "PhysicsTable.h"
#include "World.h"

double AABB::calculateXOffset(const AABB& b, double d) const {
    if (b.maxY > minY && b.minY < maxY && b.maxZ > minZ && b.minZ < maxZ) {
        if (d > 0.0 && b.maxX <= minX) { double v = minX - b.maxX; if (v < d) d = v; }
        if (d < 0.0 && b.minX >= maxX) { double v = maxX - b.minX; if (v > d) d = v; }
    }
    return d;
}

double AABB::calculateYOffset(const AABB& b, double d) const {
    if (b.maxX > minX && b.minX < maxX && b.maxZ > minZ && b.minZ < maxZ) {
        if (d > 0.0 && b.maxY <= minY) { double v = minY - b.maxY; if (v < d) d = v; }
        if (d < 0.0 && b.minY >= maxY) { double v = maxY - b.minY; if (v > d) d = v; }
    }
    return d;
}

double AABB::calculateZOffset(const AABB& b, double d) const {
    if (b.maxX > minX && b.minX < maxX && b.maxY > minY && b.minY < maxY) {
        if (d > 0.0 && b.maxZ <= minZ) { double v = minZ - b.maxZ; if (v < d) d = v; }
        if (d < 0.0 && b.minZ >= maxZ) { double v = maxZ - b.minZ; if (v > d) d = v; }
    }
    return d;
}

namespace Physics {
namespace {

inline int floorD(double v) { return int(std::floor(v)); }

bool chunksExist(const World& w, int x0, int y0, int z0, int x1, int y1, int z1) {
    if (!(y1 >= 0 && y0 < 256)) return false;
    for (int cx = x0 >> 4; cx <= x1 >> 4; ++cx)
        for (int cz = z0 >> 4; cz <= z1 >> 4; ++cz)
            if (!w.isChunkLoaded(cx, cz)) return false;
    return true;
}

void add(std::vector<AABB>& out, const AABB& area, double x0, double y0, double z0, double x1, double y1, double z1) {
    AABB b{x0, y0, z0, x1, y1, z1};
    if (area.intersects(b)) out.push_back(b);
}

// ---- BlockStairs.getStairBoxes (Axeolon modern stairs) ----
const int DIR_X[4] = {1, -1, 0, 0};
const int DIR_Z[4] = {0, 0, 1, -1};
int stairMeta(const World& w, int x, int y, int z) {
    int id = w.getBlockId(x, y, z);
    return BLOCKS[id].render == RenderType::Stairs ? w.getBlockMetadata(x, y, z) : -1;
}
bool sameHalf(int a, int b) { return (a & 4) == (b & 4); }
bool perpendicular(int a, int b) { return (a >> 1) != (b >> 1); }
bool canTakeShape(const World& w, int x, int y, int z, int meta, int dir) {
    int o = stairMeta(w, x + DIR_X[dir], y, z + DIR_Z[dir]);
    return o < 0 || (o & 3) != (meta & 3) || !sameHalf(meta, o);
}
void halfOn(int f, bool q[2][2]) {
    for (int xi = 0; xi < 2; xi++)
        for (int zi = 0; zi < 2; zi++)
            q[xi][zi] = (DIR_X[f] > 0 && xi == 1) || (DIR_X[f] < 0 && xi == 0) || (DIR_Z[f] > 0 && zi == 1) ||
                        (DIR_Z[f] < 0 && zi == 0);
}
void stairBoxes(const World& w, int x, int y, int z, const AABB& area, std::vector<AABB>& out) {
    int meta = w.getBlockMetadata(x, y, z);
    int facing = meta & 3;
    bool up = (meta & 4) != 0;
    double baseMin = up ? 0.5 : 0.0, stepMin = up ? 0.0 : 0.5;
    add(out, area, x, y + baseMin, z, x + 1, y + baseMin + 0.5, z + 1);
    bool step[2][2];
    halfOn(facing, step);
    bool shaped = false;
    int behind = stairMeta(w, x + DIR_X[facing], y, z + DIR_Z[facing]);
    if (behind >= 0 && sameHalf(meta, behind)) {
        int bf = behind & 3;
        if (perpendicular(facing, bf) && canTakeShape(w, x, y, z, meta, bf ^ 1)) {
            bool h[2][2];
            halfOn(bf, h);
            for (int a = 0; a < 2; a++) for (int c = 0; c < 2; c++) step[a][c] = step[a][c] && h[a][c];
            shaped = true;
        }
    }
    if (!shaped) {
        int front = stairMeta(w, x - DIR_X[facing], y, z - DIR_Z[facing]);
        if (front >= 0 && sameHalf(meta, front)) {
            int ff = front & 3;
            if (perpendicular(facing, ff) && canTakeShape(w, x, y, z, meta, ff)) {
                bool h[2][2];
                halfOn(ff, h);
                for (int a = 0; a < 2; a++) for (int c = 0; c < 2; c++) step[a][c] = step[a][c] || h[a][c];
            }
        }
    }
    double y0 = y + stepMin, y1 = y0 + 0.5;
    for (int xi = 0; xi < 2; xi++)
        if (step[xi][0] && step[xi][1]) {
            add(out, area, x + xi * 0.5, y0, z, x + xi * 0.5 + 0.5, y1, z + 1);
            step[xi][0] = step[xi][1] = false;
        }
    for (int zi = 0; zi < 2; zi++)
        if (step[0][zi] && step[1][zi]) {
            add(out, area, x, y0, z + zi * 0.5, x + 1, y1, z + zi * 0.5 + 0.5);
            step[0][zi] = step[1][zi] = false;
        }
    for (int xi = 0; xi < 2; xi++)
        for (int zi = 0; zi < 2; zi++)
            if (step[xi][zi]) add(out, area, x + xi * 0.5, y0, z + zi * 0.5, x + xi * 0.5 + 0.5, y1, z + zi * 0.5 + 0.5);
}

// BlockFence: a post plus arms towards other fences, 1.5 tall
void fenceBoxes(const World& w, int x, int y, int z, const AABB& area, std::vector<AABB>& out) {
    const double MIN = 6.0f / 16.0f, MAX = 10.0f / 16.0f;
    int id = w.getBlockId(x, y, z);
    bool west = w.getBlockId(x - 1, y, z) == id, east = w.getBlockId(x + 1, y, z) == id;
    bool north = w.getBlockId(x, y, z - 1) == id, south = w.getBlockId(x, y, z + 1) == id;
    add(out, area, x + (west ? 0.0 : MIN), y, z + MIN, x + (east ? 1.0 : MAX), y + 1.5, z + MAX);
    if (north || south) add(out, area, x + MIN, y, z + (north ? 0.0 : MIN), x + MAX, y + 1.5, z + (south ? 1.0 : MAX));
}

// BlockFluids.func_289_b: the flow level of a liquid block of the same material, -1 otherwise
int flowLevel(const World& w, int x, int y, int z, int material) {
    if (BLOCK_PHYSICS[w.getBlockId(x, y, z)].material != material) return -1;
    int m = w.getBlockMetadata(x, y, z);
    return m >= 8 ? 0 : m;
}

struct V3 { double x, y, z; };
V3 normalize(V3 v) {
    double l = double(float(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z))); // MathHelper.sqrt_double
    return l < 1.0E-4 ? V3{0, 0, 0} : V3{v.x / l, v.y / l, v.z / l};
}

// Block.shouldSideBeRendered for a full fluid block, as BlockFluids uses it
bool fluidSide(const World& w, int x, int y, int z, int side, int material) {
    const BlockPhysics& p = BLOCK_PHYSICS[w.getBlockId(x, y, z)];
    if (p.material == material) return false;
    if (p.ice) return false;
    if (side == 1) return true;
    return !p.opaque;
}

// BlockFluids.func_291_e: the direction the liquid pushes
V3 flowVector(const World& w, int x, int y, int z, int material) {
    V3 v{0, 0, 0};
    int own = flowLevel(w, x, y, z, material);
    for (int i = 0; i < 4; ++i) {
        int nx = x, nz = z;
        if (i == 0) nx = x - 1;
        if (i == 1) nz = z - 1;
        if (i == 2) ++nx;
        if (i == 3) ++nz;
        int l = flowLevel(w, nx, y, nz, material);
        if (l < 0) {
            if (!BLOCK_PHYSICS[w.getBlockId(nx, y, nz)].solid) {
                l = flowLevel(w, nx, y - 1, nz, material);
                if (l >= 0) {
                    int d = l - (own - 8);
                    v.x += (nx - x) * d;
                    v.z += (nz - z) * d;
                }
            }
        } else {
            int d = l - own;
            v.x += (nx - x) * d;
            v.z += (nz - z) * d;
        }
    }
    if (w.getBlockMetadata(x, y, z) >= 8) {
        bool fall = fluidSide(w, x, y, z - 1, 2, material) || fluidSide(w, x, y, z + 1, 3, material) ||
                    fluidSide(w, x - 1, y, z, 4, material) || fluidSide(w, x + 1, y, z, 5, material) ||
                    fluidSide(w, x, y + 1, z - 1, 2, material) || fluidSide(w, x, y + 1, z + 1, 3, material) ||
                    fluidSide(w, x - 1, y + 1, z, 4, material) || fluidSide(w, x + 1, y + 1, z, 5, material);
        if (fall) {
            v = normalize(v);
            v.y += -6.0;
        }
    }
    return normalize(v);
}

} // namespace

float slipperiness(int id) { return BLOCK_PHYSICS[id & 255].slipperiness; }

void blockBoxes(const World& w, int x, int y, int z, const AABB& area, std::vector<AABB>& out) {
    int id = w.getBlockId(x, y, z);
    if (!id) return;
    const BlockInfo& bi = BLOCKS[id];
    if (bi.render == RenderType::Stairs) { stairBoxes(w, x, y, z, area, out); return; }
    if (bi.render == RenderType::Fence) { fenceBoxes(w, x, y, z, area, out); return; }
    BoxRef r = BLOCK_PHYSICS[id].boxes[w.getBlockMetadata(x, y, z) & 15];
    for (int i = 0; i < r.count; i++) {
        const float* b = COLLISION_BOXES[r.start + i];
        add(out, area, x + double(b[0]), y + double(b[1]), z + double(b[2]), x + double(b[3]), y + double(b[4]),
            z + double(b[5]));
    }
}

void (*entityBoxes)(const AABB& area, std::vector<AABB>& out) = nullptr;

void collidingBoxes(const World& w, const AABB& a, std::vector<AABB>& out) {
    out.clear();
    int x0 = floorD(a.minX), x1 = floorD(a.maxX + 1.0), y0 = floorD(a.minY), y1 = floorD(a.maxY + 1.0);
    int z0 = floorD(a.minZ), z1 = floorD(a.maxZ + 1.0);
    for (int x = x0; x < x1; ++x)
        for (int z = z0; z < z1; ++z) {
            if (!w.isChunkLoaded(x >> 4, z >> 4)) continue; // blockExists(x, 64, z)
            for (int y = y0 - 1; y < y1; ++y) blockBoxes(w, x, y, z, a, out);
        }
    if (entityBoxes) entityBoxes(a, out);
}

bool isMaterialInBB(const World& w, const AABB& a, bool lava) {
    int x0 = floorD(a.minX), x1 = floorD(a.maxX + 1.0), y0 = floorD(a.minY), y1 = floorD(a.maxY + 1.0);
    int z0 = floorD(a.minZ), z1 = floorD(a.maxZ + 1.0);
    for (int x = x0; x < x1; ++x)
        for (int y = y0; y < y1; ++y)
            for (int z = z0; z < z1; ++z) {
                const BlockPhysics& p = BLOCK_PHYSICS[w.getBlockId(x, y, z)];
                if (lava ? p.lava : p.water) return true;
            }
    return false;
}

bool isAnyLiquid(const World& w, const AABB& a) {
    int x0 = floorD(a.minX), x1 = floorD(a.maxX + 1.0), y0 = floorD(a.minY), y1 = floorD(a.maxY + 1.0);
    int z0 = floorD(a.minZ), z1 = floorD(a.maxZ + 1.0);
    if (a.minX < 0.0) --x0;
    if (a.minY < 0.0) --y0;
    if (a.minZ < 0.0) --z0;
    for (int x = x0; x < x1; ++x)
        for (int y = y0; y < y1; ++y)
            for (int z = z0; z < z1; ++z)
                if (BLOCK_PHYSICS[w.getBlockId(x, y, z)].liquid) return true;
    return false;
}

bool waterAcceleration(const World& w, const AABB& a, double& mx, double& my, double& mz) {
    int x0 = floorD(a.minX), x1 = floorD(a.maxX + 1.0), y0 = floorD(a.minY), y1 = floorD(a.maxY + 1.0);
    int z0 = floorD(a.minZ), z1 = floorD(a.maxZ + 1.0);
    if (!chunksExist(w, x0, y0, z0, x1, y1, z1)) return false;
    bool in = false;
    V3 v{0, 0, 0};
    int material = -1;
    for (int x = x0; x < x1; ++x)
        for (int y = y0; y < y1; ++y)
            for (int z = z0; z < z1; ++z) {
                int id = w.getBlockId(x, y, z);
                const BlockPhysics& p = BLOCK_PHYSICS[id];
                if (!p.water) continue;
                int m = w.getBlockMetadata(x, y, z);
                float h = float((m >= 8 ? 0 : m) + 1) / 9.0f; // BlockFluids.setFluidHeight
                double top = double(float(y + 1) - h);
                if (double(y1) >= top) {
                    in = true;
                    material = p.material;
                    V3 f = flowVector(w, x, y, z, material);
                    v.x += f.x; v.y += f.y; v.z += f.z;
                }
            }
    double len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); // Vec3D.lengthVector
    if (len > 0.0) {
        v = normalize(v);
        mx += v.x * 0.004;
        my += v.y * 0.004;
        mz += v.z * 0.004;
    }
    return in;
}

} // namespace Physics
