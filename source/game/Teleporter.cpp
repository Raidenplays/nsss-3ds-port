#include "Teleporter.h"
#include <cmath>
#include <memory>
#include <unordered_map>
#include "Player.h"
#include "world/World.h"
#include "world/MathHelper.h"
#include <3ds.h>

namespace teleporter {
namespace {
constexpr int OBSIDIAN = 49, PORTAL = 90;

int floorD(double v) { int i = int(v); return v < double(i) ? i - 1 : i; }

// Java's getBlockId here would generate every chunk 128 blocks around; a chunk that has never been
// made can't hold a portal, so only the loaded and saved ones are looked through (same answer).
struct ChunkView {
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> saved;
    std::unordered_map<int64_t, const Chunk*> found; // null: no portal block in it
    World& w;
    explicit ChunkView(World& w) : w(w) {}
    const Chunk* get(int cx, int cz) {
        int64_t k = (int64_t(cx) << 32) ^ uint32_t(cz);
        auto it = found.find(k);
        if (it != found.end()) return it->second;
        const Chunk* c = w.getChunk(cx, cz);
        if (!c) {
            std::unique_ptr<Chunk> d = w.readSavedChunk(cx, cz);
            if (d) { c = d.get(); saved[k] = std::move(d); }
        }
        bool any = false;
        if (c)
            for (int i = 0; i < Chunk::VOLUME && !any; ++i) any = c->blocks[i] == PORTAL;
        if (!any) {
            saved.erase(k);
            c = nullptr;
        }
        found[k] = c;
        return c;
    }
    int at(int x, int y, int z) {
        if (y < 0 || y > 255) return 0;
        const Chunk* c = get(x >> 4, z >> 4);
        return c ? c->getBlockID(x & 15, y, z & 15) : 0;
    }
};

bool toNearestPortal(World& w, Player& p) { // func_4106_b
    const int R = 128;
    double best = -1.0;
    int bx = 0, by = 0, bz = 0;
    int px = floorD(p.posX), pz = floorD(p.posZ);
    ChunkView v(w);
    for (int x = px - R; x <= px + R; ++x) {
        double dx = double(x) + 0.5 - p.posX;
        for (int z = pz - R; z <= pz + R; ++z) {
            if (!v.get(x >> 4, z >> 4)) { z |= 15; continue; } // the rest of this chunk's row
            double dz = double(z) + 0.5 - p.posZ;
            for (int y = 255; y >= 0; --y) {
                if (v.at(x, y, z) != PORTAL) continue;
                while (v.at(x, y - 1, z) == PORTAL) --y;
                double dy = double(y) + 0.5 - p.posY;
                double d = dx * dx + dy * dy + dz * dz;
                if (best < 0.0 || d < best) { best = d; bx = x; by = y; bz = z; }
            }
        }
    }
    if (best < 0.0) return false;
    double tx = bx + 0.5, ty = by + 0.5, tz = bz + 0.5;
    if (v.at(bx - 1, by, bz) == PORTAL) tx -= 0.5;
    if (v.at(bx + 1, by, bz) == PORTAL) tx += 0.5;
    if (v.at(bx, by, bz - 1) == PORTAL) tz -= 0.5;
    if (v.at(bx, by, bz + 1) == PORTAL) tz += 0.5;
    // setLocationAndAngles: posY gets the eye height added; looking level
    p.place(tx, ty + double(Player::YOFFSET), tz, p.rotationYaw, 0.0f);
    p.motionX = p.motionY = p.motionZ = 0.0;
    return true;
}

bool solidAt(World& w, int x, int y, int z) { return matSolid(w.materialAt(x, y, z)); }

void buildPortal(World& w, Player& p) { // func_4108_c
    const int R = 16;
    double best = -1.0;
    int px = floorD(p.posX), py = floorD(p.posY), pz = floorD(p.posZ);
    int bx = px, by = py, bz = pz, dir = 0;
    JavaRandom rnd(int64_t(osGetTime()) * 0x5DEECE66DLL);
    int start = rnd.nextInt(4);
    // a flat spot 3 wide and 4 deep with room above, any of the four ways round
    for (int x = px - R; x <= px + R; ++x) {
        double dx = double(x) + 0.5 - p.posX;
        for (int z = pz - R; z <= pz + R; ++z) {
            double dz = double(z) + 0.5 - p.posZ;
            for (int y = 255; y >= 0; --y) {
                if (!w.isAirBlock(x, y, z)) continue;
                while (y > 0 && w.isAirBlock(x, y - 1, z)) --y;
                for (int d = start; d < start + 4; ++d) {
                    int ax = d % 2, az = 1 - ax;
                    if (d % 4 >= 2) { ax = -ax; az = -az; }
                    bool ok = true;
                    for (int i = 0; i < 3 && ok; ++i)
                        for (int j = 0; j < 4 && ok; ++j)
                            for (int k = -1; k < 4 && ok; ++k) {
                                int qx = x + (j - 1) * ax + i * az, qy = y + k, qz = z + (j - 1) * az - i * ax;
                                if ((k < 0 && !solidAt(w, qx, qy, qz)) || (k >= 0 && !w.isAirBlock(qx, qy, qz))) ok = false;
                            }
                    if (!ok) break; // Java's "continue label": on to the next height
                    double dy = double(y) + 0.5 - p.posY;
                    double dist = dx * dx + dy * dy + dz * dz;
                    if (best < 0.0 || dist < best) { best = dist; bx = x; by = y; bz = z; dir = d % 4; }
                }
            }
        }
    }
    if (best < 0.0) { // anywhere just wide enough for the frame
        for (int x = px - R; x <= px + R; ++x) {
            double dx = double(x) + 0.5 - p.posX;
            for (int z = pz - R; z <= pz + R; ++z) {
                double dz = double(z) + 0.5 - p.posZ;
                for (int y = 255; y >= 0; --y) {
                    if (!w.isAirBlock(x, y, z)) continue;
                    while (y > 0 && w.isAirBlock(x, y - 1, z)) --y; // (Java has no floor check; bedrock is always there)
                    for (int d = start; d < start + 2; ++d) {
                        int ax = d % 2, az = 1 - ax;
                        bool ok = true;
                        for (int j = 0; j < 4 && ok; ++j)
                            for (int k = -1; k < 4 && ok; ++k) {
                                int qx = x + (j - 1) * ax, qy = y + k, qz = z + (j - 1) * az;
                                if ((k < 0 && !solidAt(w, qx, qy, qz)) || (k >= 0 && !w.isAirBlock(qx, qy, qz))) ok = false;
                            }
                        if (!ok) break;
                        double dy = double(y) + 0.5 - p.posY;
                        double dist = dx * dx + dy * dy + dz * dz;
                        if (best < 0.0 || dist < best) { best = dist; bx = x; by = y; bz = z; dir = d % 2; }
                    }
                }
            }
        }
    }
    int ax = dir % 2, az = 1 - ax;
    if (dir % 4 >= 2) { ax = -ax; az = -az; }
    if (best < 0.0) { // nowhere: hang it in the air on an obsidian ledge
        if (by < 70) by = 70;
        if (by > 118) by = 118;
        for (int i = -1; i <= 1; ++i)
            for (int j = 1; j < 3; ++j)
                for (int k = -1; k < 3; ++k)
                    w.setBlockWithNotify(bx + (j - 1) * ax + i * az, by + k, bz + (j - 1) * az - i * ax, k < 0 ? OBSIDIAN : 0);
    }
    for (int pass = 0; pass < 4; ++pass) {
        w.editingBlocks = true;
        for (int j = 0; j < 4; ++j)
            for (int k = -1; k < 4; ++k) {
                bool frame = j == 0 || j == 3 || k == -1 || k == 3;
                w.setBlockWithNotify(bx + (j - 1) * ax, by + k, bz + (j - 1) * az, frame ? OBSIDIAN : PORTAL);
            }
        w.editingBlocks = false;
        for (int j = 0; j < 4; ++j)
            for (int k = -1; k < 4; ++k) {
                int x = bx + (j - 1) * ax, y = by + k, z = bz + (j - 1) * az;
                w.notifyBlocksOfNeighborChange(x, y, z, w.blockAt(x, y, z));
            }
    }
}
} // namespace

void place(World& w, Player& p) {
    if (!toNearestPortal(w, p)) {
        buildPortal(w, p);
        toNearestPortal(w, p);
    }
}
} // namespace teleporter
