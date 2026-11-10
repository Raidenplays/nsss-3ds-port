// ChunkProviderGenerate.populate and the WorldGen* classes it uses, plus Axeolon's additions
// (AxeolonStone.patches, AxeolonWorldGen, ruined portals). Written against World's Java-style
// calls so the blocks come out the same as the mod's for the same seed and chunk order.
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdio>
#include "ChunkGenerator.h"
#include "world/BlockProps.h"
#include "world/Blocks.h"
#include "world/MathHelper.h"
#include "world/World.h"
#include "world/Bosses.h"

namespace {
using JR = JavaRandom;
const float PI_F = 3.1415927f;
const double PI = 3.141592653589793;
inline int64_t mul(int64_t a, int64_t b) { return int64_t(uint64_t(a) * uint64_t(b)); }
inline int64_t add(int64_t a, int64_t b) { return int64_t(uint64_t(a) + uint64_t(b)); }
inline int jfloor(double v) { return int(std::floor(v)); }
inline double sq(double v) { return v * v; } // Math.pow(v, 2.0)

enum : int {
    AIR = 0, STONE = 1, GRASS = 2, DIRT = 3, COBBLE = 4, WATER = 8, WATER_STILL = 9, LAVA = 10, SAND = 12, GRAVEL = 13,
    GOLD_ORE = 14, IRON_ORE = 15, COAL_ORE = 16, LOG = 17, LEAVES = 18, LAPIS_ORE = 21, SANDSTONE = 24, TALLGRASS = 31,
    DEADBUSH = 32, FLOWER_Y = 37, FLOWER_R = 38, MUSH_B = 39, MUSH_R = 40, GOLD_BLOCK = 41, SLAB = 44, MOSSY = 48,
    OBSIDIAN = 49, SPAWNER = 52, CRATE = 54, DIAMOND_ORE = 56, REDSTONE_ORE = 73, SNOW = 78, ICE = 79, SNOW_BLOCK = 80,
    CACTUS = 81, CLAY = 82, REED = 83, PUMPKIN = 86, BLOODSTONE = 87, GLOWSTONE = 89, CHARRED_COBBLE = 101,
    CHARRED_STONE = 102, CHARRED_STONE_STAIRS = 105, CHARRED_COBBLE_STAIRS = 107, CHARRED_MOSSY = 109,
    DOUBLE_PLANT = 113, MANABLOOM = 114, WISP_LILY = 115, AX_FLOWERS = 116, DOUBLE_GLOW = 135, DOUBLE_PLANT2 = 136,
    ICICLE = 144, COMPACTED_ICE = 156, BONE_PILE = 170, STONE_TYPES = 171, STONE_COBBLE = 178, ORE_VARIANTS = 217,
};
enum { FROST = 0, OCHRE = 1, DEEP = 2, VERDANT = 3, GRANITE = 4, DIORITE = 5, ANDESITE = 6 };

// ================================================================ loot (TileEntityChest)

struct Stack {
    int id, count, damage;
};

void putInChest(World& w, int x, int y, int z, int slot, const Stack& s) { // setInventorySlotContents
    nbt::Tag* te = w.tileEntityAt(x, y, z);
    if (!te) return;
    nbt::Tag* items = te->getList("Items");
    if (!items) {
        te->set("Items", nbt::makeList(nbt::Compound));
        items = te->getList("Items");
    }
    items->listType = nbt::Compound;
    auto& v = items->items;
    v.erase(std::remove_if(v.begin(), v.end(), [&](const nbt::TagPtr& t) { return t->getByte("Slot") == slot; }), v.end());
    nbt::TagPtr t = nbt::makeCompound();
    t->setByte("Slot", int8_t(slot));
    t->setShort("id", int16_t(s.id));
    t->setByte("Count", int8_t(s.count));
    t->setShort("Damage", int16_t(s.damage));
    auto it = v.begin();
    while (it != v.end() && (*it)->getByte("Slot") < slot) ++it; // TileEntityChest writes slots in order
    v.insert(it, std::move(t));
}

Stack spellPage(JR& r) { // Spell.randomPage
    static const int W[] = {40, 8, 15, 15, 30, 2};
    int total = 0;
    for (int v : W) total += v;
    int pick = r.nextInt(total);
    for (int i = 0; i < 6; ++i) {
        pick -= W[i];
        if (pick < 0) return {1002, 1, i};
    }
    return {0, 0, 0};
}

// ================================================================ WorldGenMinable / Clay / Lakes

void minable(World& w, JR& r, int id, int n, int x, int y, int z) {
    float a = r.nextFloat() * PI_F;
    double x0 = double(float(x + 8) + MathHelper::sin(a) * float(n) / 8.0f);
    double x1 = double(float(x + 8) - MathHelper::sin(a) * float(n) / 8.0f);
    double z0 = double(float(z + 8) + MathHelper::cos(a) * float(n) / 8.0f);
    double z1 = double(float(z + 8) - MathHelper::cos(a) * float(n) / 8.0f);
    double y0 = double(y + r.nextInt(3) + 2);
    double y1 = double(y + r.nextInt(3) + 2);
    int kind = Blocks::oreKind(id);
    for (int i = 0; i <= n; ++i) {
        double cx = x0 + (x1 - x0) * double(i) / double(n);
        double cy = y0 + (y1 - y0) * double(i) / double(n);
        double cz = z0 + (z1 - z0) * double(i) / double(n);
        double s = r.nextDouble() * double(n) / 16.0;
        double rh = double(MathHelper::sin(float(i) * PI_F / float(n)) + 1.0f) * s + 1.0;
        double rv = rh;
        int ax = int(cx - rh / 2.0), ay = int(cy - rv / 2.0), az = int(cz - rh / 2.0);
        int bx = int(cx + rh / 2.0), by = int(cy + rv / 2.0), bz = int(cz + rh / 2.0);
        for (int px = ax; px <= bx; ++px) {
            double dx = (double(px) + 0.5 - cx) / (rh / 2.0);
            if (dx * dx >= 1.0) continue;
            for (int py = ay; py <= by; ++py) {
                double dy = (double(py) + 0.5 - cy) / (rv / 2.0);
                if (dx * dx + dy * dy >= 1.0) continue;
                for (int pz = az; pz <= bz; ++pz) {
                    double dz = (double(pz) + 0.5 - cz) / (rh / 2.0);
                    if (!(dx * dx + dy * dy + dz * dz < 1.0)) continue;
                    int here = w.blockAt(px, py, pz);
                    if (here != STONE && !Blocks::isNaturalStone(here)) continue;
                    if (kind >= 0 && Blocks::isNaturalStone(here)) w.setBlockAndMetadataJ(px, py, pz, ORE_VARIANTS + kind, here - STONE_TYPES);
                    else w.setBlock(px, py, pz, id);
                }
            }
        }
    }
}

bool clay(World& w, JR& r, int n, int x, int y, int z) {
    if (w.materialAt(x, y, z) != Mat::water) return false;
    float a = r.nextFloat() * PI_F;
    double x0 = double(float(x + 8) + MathHelper::sin(a) * float(n) / 8.0f);
    double x1 = double(float(x + 8) - MathHelper::sin(a) * float(n) / 8.0f);
    double z0 = double(float(z + 8) + MathHelper::cos(a) * float(n) / 8.0f);
    double z1 = double(float(z + 8) - MathHelper::cos(a) * float(n) / 8.0f);
    double y0 = double(y + r.nextInt(3) + 2);
    double y1 = double(y + r.nextInt(3) + 2);
    for (int i = 0; i <= n; ++i) {
        double cx = x0 + (x1 - x0) * double(i) / double(n);
        double cy = y0 + (y1 - y0) * double(i) / double(n);
        double cz = z0 + (z1 - z0) * double(i) / double(n);
        double s = r.nextDouble() * double(n) / 16.0;
        double rh = double(MathHelper::sin(float(i) * PI_F / float(n)) + 1.0f) * s + 1.0;
        double rv = rh;
        for (int px = int(cx - rh / 2.0); px <= int(cx + rh / 2.0); ++px)
            for (int py = int(cy - rv / 2.0); py <= int(cy + rv / 2.0); ++py)
                for (int pz = int(cz - rh / 2.0); pz <= int(cz + rh / 2.0); ++pz) {
                    double dx = (double(px) + 0.5 - cx) / (rh / 2.0);
                    double dy = (double(py) + 0.5 - cy) / (rv / 2.0);
                    double dz = (double(pz) + 0.5 - cz) / (rh / 2.0);
                    if (dx * dx + dy * dy + dz * dz < 1.0 && w.blockAt(px, py, pz) == SAND) w.setBlock(px, py, pz, CLAY);
                }
    }
    return true;
}

bool lakes(World& w, JR& r, int id, int x, int y, int z) {
    x -= 8;
    for (z -= 8; y > 0 && w.isAirBlock(x, y, z); --y) {}
    y -= 4;
    static bool hole[2048];
    std::fill(hole, hole + 2048, false);
    int blobs = r.nextInt(4) + 4;
    for (int i = 0; i < blobs; ++i) {
        double sx = r.nextDouble() * 6.0 + 3.0;
        double sy = r.nextDouble() * 4.0 + 2.0;
        double sz = r.nextDouble() * 6.0 + 3.0;
        double px = r.nextDouble() * (16.0 - sx - 2.0) + 1.0 + sx / 2.0;
        double py = r.nextDouble() * (8.0 - sy - 4.0) + 2.0 + sy / 2.0;
        double pz = r.nextDouble() * (16.0 - sz - 2.0) + 1.0 + sz / 2.0;
        for (int a = 1; a < 15; ++a)
            for (int c = 1; c < 15; ++c)
                for (int b = 1; b < 7; ++b) {
                    double dx = (double(a) - px) / (sx / 2.0);
                    double dy = (double(b) - py) / (sy / 2.0);
                    double dz = (double(c) - pz) / (sz / 2.0);
                    if (dx * dx + dy * dy + dz * dz < 1.0) hole[(a * 16 + c) * 8 + b] = true;
                }
    }
    auto H = [&](int a, int c, int b) { return hole[(a * 16 + c) * 8 + b]; };
    auto edge = [&](int a, int c, int b) {
        return !H(a, c, b) && ((a < 15 && H(a + 1, c, b)) || (a > 0 && H(a - 1, c, b)) || (c < 15 && H(a, c + 1, b)) ||
                               (c > 0 && H(a, c - 1, b)) || (b < 7 && H(a, c, b + 1)) || (b > 0 && H(a, c, b - 1)));
    };
    for (int a = 0; a < 16; ++a)
        for (int c = 0; c < 16; ++c)
            for (int b = 0; b < 8; ++b) {
                if (!edge(a, c, b)) continue;
                Mat m = w.materialAt(x + a, y + b, z + c);
                if (b >= 4 && matLiquid(m)) return false;
                if (b < 4 && !matSolid(m) && w.blockAt(x + a, y + b, z + c) != id) return false;
            }
    for (int a = 0; a < 16; ++a)
        for (int c = 0; c < 16; ++c)
            for (int b = 0; b < 8; ++b)
                if (H(a, c, b)) w.setBlock(x + a, y + b, z + c, b >= 4 ? 0 : id);
    for (int a = 0; a < 16; ++a)
        for (int c = 0; c < 16; ++c)
            for (int b = 4; b < 8; ++b)
                if (H(a, c, b) && w.blockAt(x + a, y + b - 1, z + c) == DIRT &&
                    w.getSavedLightValue(Sky::Sky, x + a, y + b, z + c) > 0)
                    w.setBlock(x + a, y + b - 1, z + c, GRASS);
    if (BP[id].mat == Mat::lava) {
        for (int a = 0; a < 16; ++a)
            for (int c = 0; c < 16; ++c)
                for (int b = 0; b < 8; ++b) {
                    if (!edge(a, c, b)) continue;
                    if ((b < 4 || r.nextInt(2) != 0) && matSolid(w.materialAt(x + a, y + b, z + c)))
                        w.setBlock(x + a, y + b, z + c, STONE);
                }
    }
    return true;
}

// ================================================================ WorldGenDungeons

bool dungeon(World& w, JR& r, int x, int y, int z) {
    const int H = 3;
    int rx = r.nextInt(2) + 2;
    int rz = r.nextInt(2) + 2;
    int doors = 0;
    for (int a = x - rx - 1; a <= x + rx + 1; ++a)
        for (int b = y - 1; b <= y + H + 1; ++b)
            for (int c = z - rz - 1; c <= z + rz + 1; ++c) {
                Mat m = w.materialAt(a, b, c);
                if (b == y - 1 && !matSolid(m)) return false;
                if (b == y + H + 1 && !matSolid(m)) return false;
                if ((a == x - rx - 1 || a == x + rx + 1 || c == z - rz - 1 || c == z + rz + 1) && b == y &&
                    w.isAirBlock(a, b, c) && w.isAirBlock(a, b + 1, c))
                    ++doors;
            }
    if (doors < 1 || doors > 5) return false;
    for (int a = x - rx - 1; a <= x + rx + 1; ++a)
        for (int b = y + H; b >= y - 1; --b)
            for (int c = z - rz - 1; c <= z + rz + 1; ++c) {
                if (a != x - rx - 1 && b != y - 1 && c != z - rz - 1 && a != x + rx + 1 && b != y + H + 1 && c != z + rz + 1) {
                    w.setBlockWithNotify(a, b, c, 0);
                } else if (b >= 0 && !matSolid(w.materialAt(a, b - 1, c))) {
                    w.setBlockWithNotify(a, b, c, 0);
                } else if (matSolid(w.materialAt(a, b, c))) {
                    if (b == y - 1 && r.nextInt(4) != 0) w.setBlockWithNotify(a, b, c, MOSSY);
                    else w.setBlockWithNotify(a, b, c, COBBLE);
                }
            }
    for (int chest = 0; chest < 2; ++chest) {
        for (int tries = 0; tries < 3; ++tries) {
            int cx = x + r.nextInt(rx * 2 + 1) - rx;
            int cz = z + r.nextInt(rz * 2 + 1) - rz;
            if (!w.isAirBlock(cx, y, cz)) continue;
            int walls = 0;
            if (matSolid(w.materialAt(cx - 1, y, cz))) ++walls;
            if (matSolid(w.materialAt(cx + 1, y, cz))) ++walls;
            if (matSolid(w.materialAt(cx, y, cz - 1))) ++walls;
            if (matSolid(w.materialAt(cx, y, cz + 1))) ++walls;
            if (walls != 1) continue;
            w.setBlockWithNotify(cx, y, cz, CRATE);
            for (int k = 0; k < 8; ++k) {
                // pickCheckLootItem
                Stack s{0, 0, 0};
                int pick = r.nextInt(12);
                if (pick == 11) {
                    if (r.nextInt(3) == 0) {
                        static const int CHARMS[] = {1007, 1008, 1009, 1014};
                        s = {CHARMS[r.nextInt(4)], 1, 0};
                    }
                } else if (pick == 0) s = {329, 1, 0};
                else if (pick == 1) s = {265, r.nextInt(4) + 1, 0};
                else if (pick == 2) s = {297, 1, 0};
                else if (pick == 3) s = {296, r.nextInt(4) + 1, 0};
                else if (pick == 4) s = {289, r.nextInt(4) + 1, 0};
                else if (pick == 5) s = {287, r.nextInt(4) + 1, 0};
                else if (pick == 6) s = {325, 1, 0};
                else if (pick == 7 && r.nextInt(100) == 0) s = {322, 1, 0};
                else if (pick == 8 && r.nextInt(2) == 0) s = {331, r.nextInt(4) + 1, 0};
                else if (pick == 9 && r.nextInt(10) == 0) s = {2256 + r.nextInt(2), 1, 0};
                else if (pick == 10) s = spellPage(r);
                if (s.id != 0) putInChest(w, cx, y, cz, r.nextInt(27), s);
            }
            break;
        }
    }
    w.setBlockWithNotify(x, y, z, SPAWNER);
    static const char* MOBS[] = {"Skeleton", "Zombie", "Zombie", "Spider"};
    const char* mob = MOBS[r.nextInt(4)];
    if (nbt::Tag* te = w.tileEntityAt(x, y, z)) te->setString("EntityId", mob);
    return true;
}

// ================================================================ trees

bool roomForTree(World& w, int x, int y, int z, int h, int (*radius)(int dy, int h, int a, int b), int a, int b,
                 bool stopEarly) {
    bool ok = true;
    for (int yy = y; yy <= y + 1 + h && (!stopEarly || ok); ++yy) {
        int rr = radius(yy - y, h, a, b);
        for (int xx = x - rr; xx <= x + rr && ok; ++xx)
            for (int zz = z - rr; zz <= z + rr && ok; ++zz) {
                if (yy >= 0 && yy < 256) {
                    int id = w.blockAt(xx, yy, zz);
                    if (id != 0 && id != LEAVES) ok = false;
                } else {
                    ok = false;
                }
            }
    }
    return ok;
}

int oakRadius(int dy, int h, int, int) { return dy == 0 ? 0 : (dy >= 1 + h - 2 ? 2 : 1); }

bool oakTree(World& w, JR& r, int x, int y, int z, int meta) { // WorldGenTrees (meta 0) / WorldGenForest (meta 2)
    int h = r.nextInt(3) + (meta == 2 ? 5 : 4);
    if (!(y >= 1 && y + h + 1 <= 256)) return false;
    if (!roomForTree(w, x, y, z, h, oakRadius, 0, 0, false)) return false;
    int below = w.blockAt(x, y - 1, z);
    if (!((below == GRASS || below == DIRT) && y < 256 - h - 1)) return false;
    w.setBlock(x, y - 1, z, DIRT);
    for (int yy = y - 3 + h; yy <= y + h; ++yy) {
        int dy = yy - (y + h);
        int rr = 1 - dy / 2;
        for (int xx = x - rr; xx <= x + rr; ++xx) {
            int ax = xx - x;
            for (int zz = z - rr; zz <= z + rr; ++zz) {
                int az = zz - z;
                if ((std::abs(ax) != rr || std::abs(az) != rr || (r.nextInt(2) != 0 && dy != 0)) &&
                    !BP[w.blockAt(xx, yy, zz)].opaqueLookup) {
                    if (meta == 0) w.setBlock(xx, yy, zz, LEAVES);
                    else w.setBlockAndMetadataJ(xx, yy, zz, LEAVES, meta);
                }
            }
        }
    }
    for (int i = 0; i < h; ++i) {
        int id = w.blockAt(x, y + i, z);
        if (id == 0 || id == LEAVES) {
            if (meta == 0) w.setBlock(x, y + i, z, LOG);
            else w.setBlockAndMetadataJ(x, y + i, z, LOG, meta);
        }
    }
    return true;
}

int taigaRadius(int dy, int, int start, int wide) { return dy < start ? 0 : wide; }

bool taiga1(World& w, JR& r, int x, int y, int z) {
    int h = r.nextInt(5) + 7;
    int bare = h - r.nextInt(2) - 3;
    int top = h - bare;
    int wide = 1 + r.nextInt(top + 1);
    if (!(y >= 1 && y + h + 1 <= 256)) return false;
    if (!roomForTree(w, x, y, z, h, taigaRadius, bare, wide, true)) return false;
    int below = w.blockAt(x, y - 1, z);
    if (!((below == GRASS || below == DIRT) && y < 256 - h - 1)) return false;
    w.setBlock(x, y - 1, z, DIRT);
    int rr = 0;
    for (int yy = y + h; yy >= y + bare; --yy) {
        for (int xx = x - rr; xx <= x + rr; ++xx) {
            int ax = xx - x;
            for (int zz = z - rr; zz <= z + rr; ++zz) {
                int az = zz - z;
                if ((std::abs(ax) != rr || std::abs(az) != rr || rr <= 0) && !BP[w.blockAt(xx, yy, zz)].opaqueLookup)
                    w.setBlockAndMetadataJ(xx, yy, zz, LEAVES, 1);
            }
        }
        if (rr >= 1 && yy == y + bare + 1) --rr;
        else if (rr < wide) ++rr;
    }
    for (int i = 0; i < h - 1; ++i) {
        int id = w.blockAt(x, y + i, z);
        if (id == 0 || id == LEAVES) w.setBlockAndMetadataJ(x, y + i, z, LOG, 1);
    }
    return true;
}

bool taiga2(World& w, JR& r, int x, int y, int z) {
    int h = r.nextInt(4) + 6;
    int bare = 1 + r.nextInt(2);
    int leafy = h - bare;
    int wide = 2 + r.nextInt(2);
    if (!(y >= 1 && y + h + 1 <= 256)) return false;
    if (!roomForTree(w, x, y, z, h, taigaRadius, bare, wide, true)) return false;
    int below = w.blockAt(x, y - 1, z);
    if (!((below == GRASS || below == DIRT) && y < 256 - h - 1)) return false;
    w.setBlock(x, y - 1, z, DIRT);
    int rr = r.nextInt(2);
    int lim = 1;
    int reset = 0;
    for (int i = 0; i <= leafy; ++i) {
        int yy = y + h - i;
        for (int xx = x - rr; xx <= x + rr; ++xx) {
            int ax = xx - x;
            for (int zz = z - rr; zz <= z + rr; ++zz) {
                int az = zz - z;
                if ((std::abs(ax) != rr || std::abs(az) != rr || rr <= 0) && !BP[w.blockAt(xx, yy, zz)].opaqueLookup)
                    w.setBlockAndMetadataJ(xx, yy, zz, LEAVES, 1);
            }
        }
        if (rr >= lim) {
            rr = reset;
            reset = 1;
            ++lim;
            if (lim > wide) lim = wide;
        } else {
            ++rr;
        }
    }
    int cut = r.nextInt(3);
    for (int i = 0; i < h - cut; ++i) {
        int id = w.blockAt(x, y + i, z);
        if (id == 0 || id == LEAVES) w.setBlockAndMetadataJ(x, y + i, z, LOG, 1);
    }
    return true;
}

// WorldGenBigTree
struct BigTree {
    static constexpr int8_t AXES[6] = {2, 0, 0, 1, 2, 1};
    JR rand;
    World* world = nullptr;
    int base[3] = {0, 0, 0};
    int total = 0; // field_878_e
    int height = 0;
    double hAtt = 0.618, branchDensity = 1.0, branchSlope = 0.381, scaleW = 1.0, leafDensity = 1.0;
    int trunkSize = 1, heightLimitLimit = 12, leafDistance = 4;
    std::vector<std::array<int, 4>> leafNodes;

    float layerSize(int y) { // func_528_a
        if (double(y) < double(float(total)) * 0.3) return -1.618f;
        float half = float(total) / 2.0f;
        float d = float(total) / 2.0f - float(y);
        float s;
        if (d == 0.0f) s = half;
        else if (std::fabs(d) >= half) s = 0.0f;
        else s = float(std::sqrt(sq(double(std::fabs(half))) - sq(double(std::fabs(d)))));
        s *= 0.5f;
        return s;
    }
    float leafSize(int y) { return y >= 0 && y < leafDistance ? (y != 0 && y != leafDistance - 1 ? 3.0f : 2.0f) : -1.0f; }

    void crossSection(int x, int y, int z, float radius, int8_t axis, int id) { // func_523_a
        int r = int(double(radius) + 0.618);
        int8_t a1 = AXES[axis], a2 = AXES[axis + 3];
        int c[3] = {x, y, z};
        int p[3] = {0, 0, 0};
        p[axis] = c[axis];
        for (int i = -r; i <= r; ++i) {
            p[a1] = c[a1] + i;
            for (int j = -r; j <= r;) {
                double d = std::sqrt(sq(double(std::abs(i)) + 0.5) + sq(double(std::abs(j)) + 0.5));
                if (d > double(radius)) { ++j; continue; }
                p[a2] = c[a2] + j;
                int b = world->blockAt(p[0], p[1], p[2]);
                if (b != 0 && b != 18) { ++j; continue; }
                world->setBlock(p[0], p[1], p[2], id);
                ++j;
            }
        }
    }
    void leafNode(int x, int y, int z) { // func_520_a
        for (int yy = y; yy < y + leafDistance; ++yy) crossSection(x, yy, z, leafSize(yy - y), 1, 18);
    }
    void limb(const int* a, const int* b, int id) { // func_522_a
        int d[3] = {0, 0, 0};
        int8_t major = 0;
        for (int8_t i = 0; i < 3; ++i) {
            d[i] = b[i] - a[i];
            if (std::abs(d[i]) > std::abs(d[major])) major = i;
        }
        if (d[major] == 0) return;
        int8_t s1 = AXES[major], s2 = AXES[major + 3];
        int dir = d[major] > 0 ? 1 : -1;
        double r1 = double(d[s1]) / double(d[major]);
        double r2 = double(d[s2]) / double(d[major]);
        int p[3] = {0, 0, 0};
        for (int i = 0, end = d[major] + dir; i != end; i += dir) {
            p[major] = MathHelper::floor_double(double(a[major] + i) + 0.5);
            p[s1] = MathHelper::floor_double(double(a[s1]) + double(i) * r1 + 0.5);
            p[s2] = MathHelper::floor_double(double(a[s2]) + double(i) * r2 + 0.5);
            world->setBlock(p[0], p[1], p[2], id);
        }
    }
    int checkLine(const int* a, const int* b) { // func_524_a
        int d[3] = {0, 0, 0};
        int8_t major = 0;
        for (int8_t i = 0; i < 3; ++i) {
            d[i] = b[i] - a[i];
            if (std::abs(d[i]) > std::abs(d[major])) major = i;
        }
        if (d[major] == 0) return -1;
        int8_t s1 = AXES[major], s2 = AXES[major + 3];
        int dir = d[major] > 0 ? 1 : -1;
        double r1 = double(d[s1]) / double(d[major]);
        double r2 = double(d[s2]) / double(d[major]);
        int p[3] = {0, 0, 0};
        int i = 0, end = d[major] + dir;
        for (; i != end; i += dir) {
            p[major] = a[major] + i;
            p[s1] = int(double(a[s1]) + double(i) * r1);
            p[s2] = int(double(a[s2]) + double(i) * r2);
            int id = world->blockAt(p[0], p[1], p[2]);
            if (id != 0 && id != 18) break;
        }
        return i == end ? -1 : std::abs(i);
    }
    void makeLeafNodes() { // func_521_a
        height = int(double(total) * hAtt);
        if (height >= total) height = total - 1;
        int per = int(1.382 + sq(leafDensity * double(total) / 13.0));
        if (per < 1) per = 1;
        std::vector<std::array<int, 4>> nodes(size_t(per) * total);
        int y = base[1] + total - leafDistance;
        int n = 1;
        int trunkTop = base[1] + height;
        int rel = y - base[1];
        nodes[0] = {base[0], y, base[2], trunkTop};
        --y;
        while (rel >= 0) {
            float size = layerSize(rel);
            if (size < 0.0f) {
                --y;
                --rel;
                continue;
            }
            for (int i = 0; i < per; ++i) {
                double dist = scaleW * double(size) * (double(rand.nextFloat()) + 0.328);
                double ang = double(rand.nextFloat()) * 2.0 * 3.14159;
                int nx = int(dist * std::sin(ang) + double(base[0]) + 0.5);
                int nz = int(dist * std::cos(ang) + double(base[2]) + 0.5);
                int a[3] = {nx, y, nz};
                int b[3] = {nx, y + leafDistance, nz};
                if (checkLine(a, b) != -1) continue;
                int t[3] = {base[0], base[1], base[2]};
                double h = std::sqrt(sq(double(std::abs(base[0] - a[0]))) + sq(double(std::abs(base[2] - a[2]))));
                double drop = h * branchSlope;
                if (double(a[1]) - drop > double(trunkTop)) t[1] = trunkTop;
                else t[1] = int(double(a[1]) - drop);
                if (checkLine(t, a) == -1) {
                    nodes[n] = {nx, y, nz, t[1]};
                    ++n;
                }
            }
            --y;
            --rel;
        }
        leafNodes.assign(nodes.begin(), nodes.begin() + n);
    }
    bool validLocation() { // func_519_e
        int a[3] = {base[0], base[1], base[2]};
        int b[3] = {base[0], base[1] + total - 1, base[2]};
        int below = world->blockAt(base[0], base[1] - 1, base[2]);
        if (below != 2 && below != 3) return false;
        int l = checkLine(a, b);
        if (l == -1) return true;
        if (l < 6) return false;
        total = l;
        return true;
    }
    bool generate(World& w, JR& r, int x, int y, int z) {
        // func_517_a(1, 1, 1) from populate
        heightLimitLimit = 12;
        leafDistance = 5;
        scaleW = 1.0;
        leafDensity = 1.0;
        world = &w;
        rand.setSeed(r.nextLong());
        base[0] = x;
        base[1] = y;
        base[2] = z;
        if (total == 0) total = 5 + rand.nextInt(heightLimitLimit);
        if (!validLocation()) return false;
        makeLeafNodes();
        for (auto& n : leafNodes) leafNode(n[0], n[1], n[2]);
        int a[3] = {base[0], base[1], base[2]};
        int b[3] = {base[0], base[1] + height, base[2]};
        limb(a, b, 17); // trunkSize is 1
        int t[3] = {base[0], base[1], base[2]};
        for (auto& n : leafNodes) {
            int e[3] = {n[0], n[1], n[2]};
            t[1] = n[3];
            if (double(t[1] - base[1]) >= double(total) * 0.2) limb(t, e, 17);
        }
        return true;
    }
};
constexpr int8_t BigTree::AXES[6];

bool drogwood(World& w, JR& r, int x, int y, int z, bool flowers) {
    const int TYPE = 3;
    int h = 6 + r.nextInt(3);
    if (y < 1 || y + h + 2 > 256) return false;
    int g = w.blockAt(x, y - 1, z);
    if (g != GRASS && g != DIRT) return false;
    for (int dy = 0; dy <= h + 1; ++dy) {
        int rr = dy >= h - 1 ? 3 : 0;
        for (int dx = -rr; dx <= rr; ++dx)
            for (int dz = -rr; dz <= rr; ++dz) {
                int id = w.blockAt(x + dx, y + dy, z + dz);
                if (id != 0 && id != LEAVES && !(dx == 0 && dz == 0 && dy == 0 && id == 6)) return false;
            }
    }
    w.setBlock(x, y - 1, z, DIRT);
    for (int layer = 0; layer < 2; ++layer) {
        int ly = y + h - 1 + layer;
        int rad = layer == 0 ? 3 : 2;
        for (int dx = -rad; dx <= rad; ++dx)
            for (int dz = -rad; dz <= rad; ++dz) {
                bool corner = std::abs(dx) == rad && std::abs(dz) == rad;
                if (corner && (layer == 0 || r.nextBoolean())) continue;
                if (w.blockAt(x + dx, ly, z + dz) == 0) w.setBlockAndMetadataJ(x + dx, ly, z + dz, LEAVES, TYPE);
            }
    }
    for (int dy = 0; dy < h; ++dy) w.setBlockAndMetadataJ(x, y + dy, z, LOG, TYPE);
    if (flowers) {
        for (int i = 0; i < 10; ++i) {
            int fx = x + r.nextInt(7) - 3;
            int fz = z + r.nextInt(7) - 3;
            if (w.blockAt(fx, y, fz) == 0 && w.blockAt(fx, y - 1, fz) == GRASS) w.setBlockAndMetadataJ(fx, y, fz, AX_FLOWERS, 4);
        }
    }
    return true;
}

bool thramwood(World& w, JR& r, int x, int y, int z) {
    const int TYPE = 4;
    int h = 6 + r.nextInt(3);
    if (y < 1 || y + h + 3 > 256) return false;
    for (int dx = 0; dx <= 1; ++dx)
        for (int dz = 0; dz <= 1; ++dz) {
            int g = w.blockAt(x + dx, y - 1, z + dz);
            if (g != GRASS && g != DIRT) return false;
            for (int dy = 0; dy < h; ++dy) {
                int id = w.blockAt(x + dx, y + dy, z + dz);
                if (id != 0 && id != LEAVES && id != 6 && id != TALLGRASS) return false;
            }
        }
    int leafMeta = (TYPE & 3) | ((TYPE & 4) << 1);
    double cx = double(x) + 1.0, cz = double(z) + 1.0;
    int top = y + h;
    for (int dy = -2; dy <= 2; ++dy) {
        int rad = dy == 2 ? 2 : (dy == -2 ? 3 : 4);
        for (int lx = x - rad; lx <= x + 1 + rad; ++lx)
            for (int lz = z - rad; lz <= z + 1 + rad; ++lz) {
                double ddx = double(lx) + 0.5 - cx, ddz = double(lz) + 0.5 - cz;
                double dist = std::sqrt(ddx * ddx + ddz * ddz);
                if (dist <= double(rad) + 0.5 - (r.nextInt(4) == 0 ? 0.6 : 0.0) && w.blockAt(lx, top + dy, lz) == 0)
                    w.setBlockAndMetadataJ(lx, top + dy, lz, LEAVES, leafMeta);
            }
    }
    for (int lx = x - 4; lx <= x + 5; ++lx)
        for (int lz = z - 4; lz <= z + 5; ++lz) {
            double ddx = double(lx) + 0.5 - cx, ddz = double(lz) + 0.5 - cz;
            double dist = std::sqrt(ddx * ddx + ddz * ddz);
            if (dist > 2.5 && dist <= 4.5 && r.nextInt(3) == 0) {
                int sy = top - 2;
                if (w.blockAt(lx, sy, lz) != LEAVES) continue;
                int len = 1 + r.nextInt(4);
                for (int i = 1; i <= len; ++i) {
                    if (w.blockAt(lx, sy - i, lz) != 0) break;
                    w.setBlockAndMetadataJ(lx, sy - i, lz, LEAVES, leafMeta);
                }
            }
        }
    for (int dx = 0; dx <= 1; ++dx)
        for (int dz = 0; dz <= 1; ++dz) {
            w.setBlock(x + dx, y - 1, z + dz, DIRT);
            for (int dy = 0; dy < h + 1; ++dy) w.setBlockAndMetadataJ(x + dx, y + dy, z + dz, LOG, TYPE);
        }
    return true;
}

// MobSpawnerBase.getRandomWorldGenForTrees, then generate
enum class Tree { Oak, Big, Birch, Taiga1, Taiga2, Drogwood };
Tree pickTree(Biome b, JR& r) {
    switch (b) {
    case Biome::Rainforest: case Biome::DenseForest: return r.nextInt(3) == 0 ? Tree::Big : Tree::Oak;
    case Biome::Forest:
        if (r.nextInt(5) == 0) return Tree::Birch;
        return r.nextInt(3) == 0 ? Tree::Big : Tree::Oak;
    case Biome::Taiga: case Biome::Peaks: return r.nextInt(3) == 0 ? Tree::Taiga1 : Tree::Taiga2;
    case Biome::BirchForest: return r.nextInt(6) == 0 ? Tree::Oak : Tree::Birch;
    case Biome::Thrams: {
        int roll = r.nextInt(10);
        return roll < 2 ? Tree::Birch : (roll < 3 ? Tree::Taiga2 : Tree::Oak);
    }
    case Biome::Drogwoods: return r.nextInt(5) == 0 ? Tree::Oak : Tree::Drogwood;
    default: return r.nextInt(10) == 0 ? Tree::Big : Tree::Oak;
    }
}
bool growTree(Tree t, World& w, JR& r, int x, int y, int z) {
    switch (t) {
    case Tree::Oak: return oakTree(w, r, x, y, z, 0);
    case Tree::Birch: return oakTree(w, r, x, y, z, 2);
    case Tree::Big: { BigTree b; return b.generate(w, r, x, y, z); }
    case Tree::Taiga1: return taiga1(w, r, x, y, z);
    case Tree::Taiga2: return taiga2(w, r, x, y, z);
    case Tree::Drogwood: return drogwood(w, r, x, y, z, true);
    }
    return false;
}

// ================================================================ plants

void flowers(World& w, JR& r, int id, int x, int y, int z) { // WorldGenFlowers
    for (int i = 0; i < 64; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.isAirBlock(px, py, pz) && Blocks::canBlockStay(w, id, px, py, pz)) w.setBlock(px, py, pz, id);
    }
}

void flowersMeta(World& w, JR& r, int id, int meta, int tries, int x, int y, int z) {
    for (int i = 0; i < tries; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.isAirBlock(px, py, pz)) {
            w.setBlockAndMetadataJ(px, py, pz, id, meta);
            if (!Blocks::canBlockStay(w, id, px, py, pz)) w.setBlockAndMetadataJ(px, py, pz, 0, 0);
        }
    }
}

void scatterOnGround(World& w, JR& r, int id, int meta, int tries, int x, int y, int z, bool withMeta) {
    // WorldGenTallGrass (128 tries, with metadata) / WorldGenDeadBush (4 tries)
    while (true) {
        int b = w.blockAt(x, y, z);
        if ((b != 0 && b != LEAVES) || y <= 0) break;
        --y;
    }
    for (int i = 0; i < tries; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.isAirBlock(px, py, pz) && Blocks::canBlockStay(w, id, px, py, pz)) {
            if (withMeta) w.setBlockAndMetadataJ(px, py, pz, id, meta);
            else w.setBlock(px, py, pz, id);
        }
    }
}

void doublePlant(World& w, JR& r, int id, int type, int tries, int x, int y, int z) {
    while (y > 0) {
        int b = w.blockAt(x, y, z);
        if (b != 0 && b != LEAVES) break;
        --y;
    }
    for (int i = 0; i < tries; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (py > 0 && py < 254 && w.isAirBlock(px, py, pz) && w.isAirBlock(px, py + 1, pz) && !w.isAirBlock(px, py - 1, pz)) {
            w.setBlockAndMetadataJ(px, py, pz, id, type);
            w.setBlockAndMetadataJ(px, py + 1, pz, id, type | 8);
            if (!Blocks::canBlockStay(w, id, px, py, pz)) {
                w.setBlock(px, py + 1, pz, 0);
                w.setBlock(px, py, pz, 0);
            }
        }
    }
}

void cattails(World& w, JR& r, int x, int z) {
    for (int i = 0; i < 20; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        int py = w.getHeightValue(px, pz);
        Blocks::placeCattail(w, px, py, pz, false);
    }
}

void reeds(World& w, JR& r, int x, int y, int z) {
    for (int i = 0; i < 20; ++i) {
        int a = r.nextInt(4);
        int px = x + a - r.nextInt(4);
        a = r.nextInt(4);
        int pz = z + a - r.nextInt(4);
        if (w.isAirBlock(px, y, pz) &&
            (w.materialAt(px - 1, y - 1, pz) == Mat::water || w.materialAt(px + 1, y - 1, pz) == Mat::water ||
             w.materialAt(px, y - 1, pz - 1) == Mat::water || w.materialAt(px, y - 1, pz + 1) == Mat::water)) {
            int n = 2 + r.nextInt(r.nextInt(3) + 1);
            for (int k = 0; k < n; ++k)
                if (Blocks::canBlockStay(w, REED, px, y + k, pz)) w.setBlock(px, y + k, pz, REED);
        }
    }
}

void pumpkinPatch(World& w, JR& r, int x, int y, int z) {
    for (int i = 0; i < 64; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.isAirBlock(px, py, pz) && w.blockAt(px, py - 1, pz) == GRASS && Blocks::canPlaceBlockAt(w, PUMPKIN, px, py, pz))
            w.setBlockAndMetadataJ(px, py, pz, PUMPKIN, r.nextInt(4));
    }
}

void cactus(World& w, JR& r, int x, int y, int z) {
    for (int i = 0; i < 10; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.isAirBlock(px, py, pz)) {
            int n = 1 + r.nextInt(r.nextInt(3) + 1);
            for (int k = 0; k < n; ++k)
                if (Blocks::canBlockStay(w, CACTUS, px, py + k, pz)) w.setBlock(px, py + k, pz, CACTUS);
        }
    }
}

void liquids(World& w, JR& r, int id, int x, int y, int z) { // WorldGenLiquids
    if (w.blockAt(x, y + 1, z) != STONE) return;
    if (w.blockAt(x, y - 1, z) != STONE) return;
    if (w.blockAt(x, y, z) != 0 && w.blockAt(x, y, z) != STONE) return;
    int stone = 0, air = 0;
    if (w.blockAt(x - 1, y, z) == STONE) ++stone;
    if (w.blockAt(x + 1, y, z) == STONE) ++stone;
    if (w.blockAt(x, y, z - 1) == STONE) ++stone;
    if (w.blockAt(x, y, z + 1) == STONE) ++stone;
    if (w.isAirBlock(x - 1, y, z)) ++air;
    if (w.isAirBlock(x + 1, y, z)) ++air;
    if (w.isAirBlock(x, y, z - 1)) ++air;
    if (w.isAirBlock(x, y, z + 1)) ++air;
    if (stone == 3 && air == 1) {
#ifdef GEN_DEBUG
        fprintf(stderr, "LIQ %d %d %d %d %lld\n", id, x, y, z, (long long)w.rand.state());
#endif
        w.setBlockWithNotify(x, y, z, id);
        w.scheduledUpdatesAreImmediate = true;
        Blocks::updateTick(w, x, y, z, id, r);
        w.scheduledUpdatesAreImmediate = false;
    }
}

// ================================================================ ruined portals

int findTopSolid(World& w, int x, int z) { return w.findTopSolidBlock(x, z); }

int ruinBlock(JR& r) {
    switch (r.nextInt(8)) {
    case 0: return COBBLE;
    case 1: return MOSSY;
    case 2: return CHARRED_STONE;
    case 3: return CHARRED_MOSSY;
    case 4: return BLOODSTONE;
    default: return CHARRED_COBBLE;
    }
}

bool ruinedPortal(World& w, JR& r, int x, int, int z) {
    int y = findTopSolid(w, x, z);
    if (y < 4 || y > 120 || matLiquid(w.materialAt(x, y - 1, z))) return false;
    int variant = r.nextInt(3);
    bool alongX = r.nextBoolean();
    // platform
    for (int dx = -3; dx <= 3; ++dx)
        for (int dz = -3; dz <= 3; ++dz) {
            int d = dx * dx + dz * dz;
            if (d > 12 || (d > 6 && r.nextInt(3) == 0)) continue;
            int g = findTopSolid(w, x + dx, z + dz) - 1;
            if (g < y - 4) g = y - 4;
            for (int fy = g; fy < y - 1; ++fy) w.setBlockWithNotify(x + dx, fy, z + dz, CHARRED_COBBLE);
            w.setBlockWithNotify(x + dx, y - 1, z + dz, ruinBlock(r));
            for (int cy = y; cy < y + 6; ++cy)
                if (w.blockAt(x + dx, cy, z + dz) != 0 && !matSolid(w.materialAt(x + dx, cy, z + dz)))
                    w.setBlockWithNotify(x + dx, cy, z + dz, 0);
        }
    if (variant == 2) {
        for (int across = 0; across < 4; ++across)
            for (int len = 0; len < 5; ++len) {
                bool edge = across == 0 || across == 3 || len == 0 || len == 4;
                if (!edge || r.nextInt(4) == 0) continue;
                int bx = alongX ? x - 1 + across : x - 2 + len;
                int bz = alongX ? z - 2 + len : z - 1 + across;
                w.setBlockWithNotify(bx, y, bz, OBSIDIAN);
            }
    } else {
        int by0 = variant == 1 ? y - 2 : y;
        for (int across = 0; across < 4; ++across)
            for (int up = 0; up < 5; ++up) {
                bool edge = across == 0 || across == 3 || up == 0 || up == 4;
                if (!edge) continue;
                int bx = alongX ? x - 1 + across : x;
                int bz = alongX ? z : z - 1 + across;
                if (r.nextInt(4) == 0) continue;
                w.setBlockWithNotify(bx, by0 + up, bz, r.nextInt(8) == 0 ? CHARRED_STONE : OBSIDIAN);
            }
    }
    // decorations
    for (int i = 0; i < 24; ++i) {
        int fx = x + r.nextInt(9) - 4;
        int fy = y + r.nextInt(3) - 1;
        int fz = z + r.nextInt(9) - 4;
        if (w.blockAt(fx, fy, fz) == 0) {
            w.setBlockAndMetadataJ(fx, fy, fz, AX_FLOWERS, 0);
            if (!Blocks::canBlockStay(w, AX_FLOWERS, fx, fy, fz)) w.setBlockAndMetadataJ(fx, fy, fz, 0, 0);
        }
    }
    int count = 3 + r.nextInt(4);
    for (int i = 0; i < count; ++i) {
        int dx = r.nextInt(7) - 3;
        int dz = r.nextInt(7) - 3;
        if ((std::abs(dx) <= 1 && std::abs(dz) <= 1) || w.blockAt(x + dx, y, z + dz) != 0 ||
            !matSolid(w.materialAt(x + dx, y - 1, z + dz)))
            continue;
        int roll = r.nextInt(10);
        if (roll < 4) {
            w.setBlockAndMetadataWithNotify(x + dx, y, z + dz, SLAB, r.nextBoolean() ? 2 : 5);
        } else if (roll < 7) {
            int id = r.nextBoolean() ? CHARRED_STONE_STAIRS : CHARRED_COBBLE_STAIRS;
            w.setBlockAndMetadataWithNotify(x + dx, y, z + dz, id, r.nextInt(4));
        } else if (roll < 9) {
            w.setBlockWithNotify(x + dx, y, z + dz, BLOODSTONE);
        } else {
            w.setBlockWithNotify(x + dx, y, z + dz, GOLD_BLOCK);
        }
    }
    // chest
    int cx, cz;
    if (alongX) {
        cx = x + r.nextInt(3) - 1;
        cz = z + (r.nextBoolean() ? 2 : -2);
    } else {
        cx = x + (r.nextBoolean() ? 2 : -2);
        cz = z + r.nextInt(3) - 1;
    }
    if (!matSolid(w.materialAt(cx, y - 1, cz))) w.setBlockWithNotify(cx, y - 1, cz, CHARRED_COBBLE);
    w.setBlockWithNotify(cx, y, cz, CRATE);
    if (!w.tileEntityAt(cx, y, cz)) return true;
    if (r.nextInt(2) == 0) {
        Stack p = spellPage(r);
        putInChest(w, cx, y, cz, r.nextInt(27), p);
    }
    int items = 4 + r.nextInt(5);
    for (int i = 0; i < items; ++i) {
        Stack s{0, 0, 0};
        switch (r.nextInt(12)) {
        case 0: s = {259, 1, 0}; break;
        case 1: s = {OBSIDIAN, 1 + r.nextInt(2), 0}; break;
        case 2: case 3: s = {266, 2 + r.nextInt(7), 0}; break;
        case 4: s = {318, 1 + r.nextInt(4), 0}; break;
        case 5: s = {265, 1 + r.nextInt(3), 0}; break;
        case 6: s = {283, 1, 0}; break;
        case 7: s = {285, 1, 0}; break;
        case 8: s = {347, 1, 0}; break;
        case 9: s = r.nextInt(10) == 0 ? Stack{322, 1, 0} : Stack{260, 1, 0}; break;
        case 10: s = {CHARRED_MOSSY, 2 + r.nextInt(6), 0}; break;
        default: s = {BLOODSTONE, 4 + r.nextInt(8), 0}; break;
        }
        putInChest(w, cx, y, cz, r.nextInt(27), s);
    }
    return true;
}

// ================================================================ AxeolonStone.patches

void stonePatches(World& w, JR& r, int x, int z) {
    for (int t = GRANITE; t <= ANDESITE; ++t)
        for (int i = 0; i < 6; ++i) {
            int bx0 = x + r.nextInt(16);
            int by0 = r.nextInt(80);
            int bz0 = z + r.nextInt(16);
            const int size = 30;
            int id = STONE_TYPES + t;
            float a = r.nextFloat() * PI_F;
            double x0 = double(float(bx0 + 8) + MathHelper::sin(a) * float(size) / 8.0f);
            double x1 = double(float(bx0 + 8) - MathHelper::sin(a) * float(size) / 8.0f);
            double z0 = double(float(bz0 + 8) + MathHelper::cos(a) * float(size) / 8.0f);
            double z1 = double(float(bz0 + 8) - MathHelper::cos(a) * float(size) / 8.0f);
            double y0 = double(by0 + r.nextInt(3) + 2);
            double y1 = double(by0 + r.nextInt(3) + 2);
            for (int k = 0; k <= size; ++k) {
                double cx = x0 + (x1 - x0) * double(k) / double(size);
                double cy = y0 + (y1 - y0) * double(k) / double(size);
                double cz = z0 + (z1 - z0) * double(k) / double(size);
                double s = r.nextDouble() * double(size) / 16.0;
                double rh = double(MathHelper::sin(float(k) * PI_F / float(size)) + 1.0f) * s + 1.0;
                for (int bx = MathHelper::floor_double(cx - rh / 2.0); bx <= MathHelper::floor_double(cx + rh / 2.0); ++bx)
                    for (int by = MathHelper::floor_double(cy - rh / 2.0); by <= MathHelper::floor_double(cy + rh / 2.0); ++by)
                        for (int bz = MathHelper::floor_double(cz - rh / 2.0); bz <= MathHelper::floor_double(cz + rh / 2.0); ++bz) {
                            double dx = (double(bx) + 0.5 - cx) / (rh / 2.0), dy = (double(by) + 0.5 - cy) / (rh / 2.0),
                                   dz = (double(bz) + 0.5 - cz) / (rh / 2.0);
                            if (dx * dx + dy * dy + dz * dz < 1.0 && w.blockAt(bx, by, bz) == STONE) w.setBlock(bx, by, bz, id);
                        }
            }
        }
}

// ================================================================ AxeolonWorldGen / AxeolonCaveBiomes

double caveHash(int64_t seed, int x, int y, int z, int salt) {
    uint64_t h = uint64_t(seed) ^ uint64_t(int64_t(salt)) * 0x632BE59BD9B4E019ULL;
    h ^= uint64_t(int64_t(x)) * 0x9E3779B97F4A7C15ULL;
    h = (h ^ (h >> 29)) * 0xBF58476D1CE4E5B9ULL;
    h ^= uint64_t(int64_t(y)) * 0xC2B2AE3D27D4EB4FULL;
    h = (h ^ (h >> 32)) * 0x94D049BB133111EBULL;
    h ^= uint64_t(int64_t(z)) * 0xD6E8FEB86659FD93ULL;
    h = (h ^ (h >> 31)) * 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 29;
    return double(int64_t(h >> 11)) / double(1LL << 53) * 2.0 - 1.0;
}
inline double fade3(double f) { return f * f * (3.0 - 2.0 * f); }
inline double lerp(double t, double a, double b) { return a + (b - a) * t; }
double caveValue(int64_t seed, double x, double y, double z, int salt) {
    int xi = jfloor(x), yi = jfloor(y), zi = jfloor(z);
    double u = fade3(x - xi), v = fade3(y - yi), t = fade3(z - zi);
    double c000 = caveHash(seed, xi, yi, zi, salt), c100 = caveHash(seed, xi + 1, yi, zi, salt);
    double c010 = caveHash(seed, xi, yi + 1, zi, salt), c110 = caveHash(seed, xi + 1, yi + 1, zi, salt);
    double c001 = caveHash(seed, xi, yi, zi + 1, salt), c101 = caveHash(seed, xi + 1, yi, zi + 1, salt);
    double c011 = caveHash(seed, xi, yi + 1, zi + 1, salt), c111 = caveHash(seed, xi + 1, yi + 1, zi + 1, salt);
    double a = lerp(u, c000, c100), b = lerp(u, c010, c110), c = lerp(u, c001, c101), d = lerp(u, c011, c111);
    return lerp(t, lerp(v, a, b), lerp(v, c, d));
}
double caveNoise(int64_t seed, double x, double y, double z, int salt) {
    return caveValue(seed, x, y, z, salt) * 0.72 + caveValue(seed, x * 2.03, y * 2.03, z * 2.03, salt + 100) * 0.28;
}
enum { CB_NONE = 0, CB_FROZEN = 1, CB_LUSH = 2, CB_MUSHROOM = 3, CB_DEEP = 4 };
bool isCold(Biome b) { return b == Biome::Taiga || b == Biome::Tundra || b == Biome::IceDesert || b == Biome::Peaks; }
int caveBiome(int64_t seed, int x, int y, int z, Biome surface, int surfaceY) {
    if (y < 1 || y > surfaceY - 10) return CB_NONE;
    int cx = x >> 2, cy = y >> 2, cz = z >> 2;
    double deepTop = 22 + caveNoise(seed, cx / 10.0, 0.0, cz / 10.0, 9) * 5.0;
    if (y < deepTop) return CB_DEEP;
    double cold = caveNoise(seed, cx / 22.0, cy / 10.0, cz / 22.0, 1) + (isCold(surface) ? 0.55 : -0.12);
    if (cold > 0.42) return CB_FROZEN;
    if (caveNoise(seed, cx / 18.0, cy / 8.0, cz / 18.0, 2) > 0.30) return CB_LUSH;
    if (caveNoise(seed, cx / 15.0, cy / 7.0, cz / 15.0, 3) > 0.45) return CB_MUSHROOM;
    return CB_NONE;
}

struct AxGen {
    World& w;
    int64_t seed;
    ChunkGenerator& g;

    int ground(int x, int z) {
        for (int y = std::min(w.getHeightValue(x, z), 255); y > 0; --y) {
            Mat m = w.materialAt(x, y, z);
            if (m == Mat::ground || m == Mat::sand || m == Mat::rock || m == Mat::builtSnow || m == Mat::clay) return y;
            if (m == Mat::water || m == Mat::lava || m == Mat::ice) return -1;
        }
        return -1;
    }
    bool replaceable(int x, int y, int z) {
        Mat m = w.materialAt(x, y, z);
        return m == Mat::air || m == Mat::plants || m == Mat::snow;
    }
    void freeze(int x1, int z1, int size) {
        for (int x = x1; x < x1 + size; ++x)
            for (int z = z1; z < z1 + size; ++z) {
                int h = w.getHeightValue(x, z);
                for (int y = std::min(h + 1, 254); y > h - 4 && y > 0; --y)
                    if (w.materialAt(x, y, z) == Mat::water && w.isAirBlock(x, y + 1, z)) {
                        w.setBlock(x, y, z, ICE);
                        break;
                    }
            }
    }
    void lakeExtra(JR& r, int x0, int z0, Biome b) {
        if (b == Biome::Desert || b == Biome::Savanna) return;
        bool peaks = b == Biome::Peaks;
        bool cold = isCold(b);
        int chance = peaks ? 3 : (b == Biome::Swampland ? 3 : 6);
        if (r.nextInt(chance) != 0) return;
        int x = x0 + r.nextInt(16);
        int z = z0 + r.nextInt(16);
        int gy = ground(x, z);
        if (gy < 4 || gy > 236) return;
        if (lakes(w, r, WATER, x, gy + 1, z) && (cold || peaks)) freeze(x - 8, z - 8, 16);
    }
    void boulder(JR& r, int x, int z) {
        int gy = ground(x, z);
        if (gy < 2 || w.materialAt(x, gy, z) == Mat::sand) return;
        int blobs = 2 + r.nextInt(2);
        for (int i = 0; i < blobs; ++i) {
            double rad = 1.1 + r.nextDouble() * 1.1;
            double cx = double(x) + 0.5 + double(r.nextInt(3) - 1);
            double cz = double(z) + 0.5 + double(r.nextInt(3) - 1);
            double cy = double(gy) + 0.5 + rad * 0.4;
            int ri = int(std::ceil(rad));
            for (int dx = -ri; dx <= ri; ++dx)
                for (int dy = -ri; dy <= ri; ++dy)
                    for (int dz = -ri; dz <= ri; ++dz) {
                        int bx = jfloor(cx) + dx, by = jfloor(cy) + dy, bz = jfloor(cz) + dz;
                        double ex = double(bx) + 0.5 - cx, ey = double(by) + 0.5 - cy, ez = double(bz) + 0.5 - cz;
                        if (ex * ex + ey * ey * 1.3 + ez * ez > rad * rad) continue;
                        if (by <= gy - 1 || !(replaceable(bx, by, bz) || (by <= gy + 1 && w.materialAt(bx, by, bz) == Mat::ground))) continue;
                        w.setBlock(bx, by, bz, r.nextInt(5) < 3 ? MOSSY : COBBLE);
                    }
        }
    }
    void fallenLog(JR& r, int x, int z, Biome b) {
        int gy = ground(x, z);
        if (gy < 2) return;
        int len = 3 + r.nextInt(4);
        bool alongX = r.nextBoolean();
        for (int i = 0; i < len; ++i) {
            int bx = alongX ? x + i : x, bz = alongX ? z : z + i;
            if (ground(bx, bz) != gy || !replaceable(bx, gy + 1, bz)) return;
        }
        int meta = b == Biome::Taiga ? 1 : (b == Biome::BirchForest ? 2 : 0);
        for (int i = 0; i < len; ++i) {
            int bx = alongX ? x + i : x, bz = alongX ? z : z + i;
            w.setBlockAndMetadataJ(bx, gy + 1, bz, LOG, meta);
        }
    }
    void outcrop(JR& r, int x, int z, Biome b) {
        int id, alt;
        if (b == Biome::Desert || b == Biome::Savanna) {
            id = SANDSTONE;
            alt = STONE_TYPES + OCHRE;
        } else if (b == Biome::Tundra || b == Biome::IceDesert || b == Biome::Peaks) {
            id = STONE_TYPES + FROST;
            alt = STONE_TYPES + ANDESITE;
        } else {
            id = STONE;
            alt = STONE_TYPES + ANDESITE;
        }
        int pillars = 2 + r.nextInt(4);
        for (int i = 0; i < pillars; ++i) {
            int px = x + r.nextInt(5) - 2;
            int pz = z + r.nextInt(5) - 2;
            int gy = ground(px, pz);
            if (gy < 2) continue;
            int h = i == 0 ? 2 + r.nextInt(3) : 1 + r.nextInt(2);
            for (int y = gy; y <= gy + h; ++y) {
                if (y > gy && !replaceable(px, y, pz)) break;
                int pick;
                if (r.nextInt(3) == 0) pick = alt;
                else pick = r.nextInt(4) == 0 ? COBBLE : id;
                w.setBlock(px, y, pz, pick);
            }
        }
    }
    void pond(JR& r, int x, int z) {
        int gy = ground(x, z);
        if (gy < 3) return;
        int rad = 2 + r.nextInt(2);
        for (int dx = -rad - 1; dx <= rad + 1; ++dx)
            for (int dz = -rad - 1; dz <= rad + 1; ++dz) {
                if (dx * dx + dz * dz > (rad + 1) * (rad + 1)) continue;
                if (ground(x + dx, z + dz) != gy || w.materialAt(x + dx, gy, z + dz) != Mat::ground) return;
            }
        for (int dx = -rad; dx <= rad; ++dx)
            for (int dz = -rad; dz <= rad; ++dz) {
                int d = dx * dx + dz * dz;
                if (d >= rad * rad) continue;
                int bx = x + dx, bz = z + dz;
                bool deep = d < (rad - 1) * (rad - 1) + 1;
                if (replaceable(bx, gy + 1, bz)) w.setBlock(bx, gy + 1, bz, 0);
                w.setBlock(bx, gy, bz, WATER);
                int bottom = gy - 1;
                if (deep) {
                    w.setBlock(bx, gy - 1, bz, WATER);
                    bottom = gy - 2;
                }
                w.setBlock(bx, bottom, bz, r.nextInt(3) == 0 ? CLAY : SAND);
            }
        cattails(w, r, x, z);
    }
    void pumpkins(JR& r, int x, int z) {
        int n = 6 + r.nextInt(7);
        for (int i = 0; i < n; ++i) {
            int px = x + r.nextInt(7) - 3;
            int pz = z + r.nextInt(7) - 3;
            int gy = ground(px, pz);
            if (gy < 2 || w.blockAt(px, gy, pz) != GRASS || !replaceable(px, gy + 1, pz)) continue;
            w.setBlockAndMetadataJ(px, gy + 1, pz, PUMPKIN, r.nextInt(4));
        }
    }
    static bool halloween() {
        time_t t = time(nullptr);
        struct tm* lt = localtime(&t);
        return lt && lt->tm_mon == 9 && lt->tm_mday == 31;
    }
    void surface(JR& r, int x0, int z0, Biome b) {
        bool wooded = b == Biome::Forest || b == Biome::DenseForest || b == Biome::Taiga || b == Biome::BirchForest ||
                      b == Biome::Rainforest || b == Biome::SeasonalForest || b == Biome::Thrams || b == Biome::Drogwoods;
        bool open = b == Biome::Plains || b == Biome::Shrubland || b == Biome::Savanna || b == Biome::Tundra ||
                    b == Biome::Desert || b == Biome::Peaks || b == Biome::IceDesert;
        bool pondy = b == Biome::Plains || b == Biome::Forest || b == Biome::Swampland || b == Biome::SeasonalForest ||
                     b == Biome::BirchForest || b == Biome::Rainforest || b == Biome::Shrubland;
        bool pumpkiny = b == Biome::Plains || b == Biome::Forest || b == Biome::SeasonalForest || b == Biome::Shrubland;
        auto spot = [&](int off, int range, int& x, int& z) {
            x = x0 + off + r.nextInt(range);
            z = z0 + off + r.nextInt(range);
        };
        int x, z;
        if (wooded && r.nextInt(5) == 0) { spot(2, 12, x, z); boulder(r, x, z); }
        if (b == Biome::Peaks && r.nextInt(4) == 0) { spot(2, 12, x, z); boulder(r, x, z); }
        if (wooded && r.nextInt(3) == 0) { spot(2, 12, x, z); fallenLog(r, x, z, b); }
        if (open && r.nextInt(7) == 0) { spot(3, 10, x, z); outcrop(r, x, z, b); }
        if (pondy && r.nextInt(b == Biome::Swampland ? 3 : 8) == 0) { spot(4, 8, x, z); pond(r, x, z); }
        if (pumpkiny && r.nextInt(halloween() ? 6 : 28) == 0) { spot(4, 8, x, z); pumpkins(r, x, z); }
    }
    void peakOres(JR& r, int x0, int z0) {
        if (g.peaks->lift(x0 + 8, z0 + 8) < 20.0) return;
        for (int i = 0; i < 18; ++i) {
            int x = x0 - 8 + r.nextInt(16);
            int y = 128 + r.nextInt(100);
            int z = z0 - 8 + r.nextInt(16);
            minable(w, r, COAL_ORE, 16, x, y, z);
        }
        for (int i = 0; i < 8; ++i) {
            int x = x0 - 8 + r.nextInt(16);
            int y = 128 + r.nextInt(70);
            int z = z0 - 8 + r.nextInt(16);
            minable(w, r, IRON_ORE, 8, x, y, z);
        }
    }
    static bool stoneLike(int id) { return id == STONE || Blocks::isNaturalStone(id) || id == DIRT || id == GRAVEL; }
    double patch(int x, int y, int z, int salt) { return caveNoise(seed, x / 4.0, y / 4.0, z / 4.0, 40 + salt); }
    void wall(int x, int y, int z, int cb) {
        double n = patch(x, y, z, cb);
        switch (cb) {
        case CB_FROZEN:
            if (n > -0.25) w.setBlock(x, y, z, COMPACTED_ICE);
            else if (n > -0.45) w.setBlock(x, y, z, SNOW_BLOCK);
            else w.setBlock(x, y, z, STONE_TYPES + FROST);
            break;
        case CB_LUSH:
            if (n > 0.0) w.setBlock(x, y, z, MOSSY);
            else if (n < -0.3) w.setBlock(x, y, z, STONE_TYPES + VERDANT);
            break;
        case CB_MUSHROOM:
            if (n > 0.25) w.setBlock(x, y, z, DIRT);
            break;
        case CB_DEEP:
            if (n > 0.5) w.setBlockAndMetadataJ(x, y, z, STONE_COBBLE, DEEP);
            break;
        }
    }
    void iceOre(int x, int y, int z, int id) {
        int kind = Blocks::oreKind(id);
        if (kind < 0 && BP[id].cls == BC::OreVariant) kind = id == 223 ? 3 : id - ORE_VARIANTS; // BlockOreVariant.ore
        if (kind < 0 || patch(x, y, z, CB_FROZEN) <= -0.25) return;
        w.setBlockAndMetadataJ(x, y, z, ORE_VARIANTS + kind, 7);
    }
    void icicle(JR& r, int x, int y, int z, bool rising) {
        int len = 1 + r.nextInt(rising ? 2 : 3);
        for (int i = 0; i < len; ++i) {
            int yy = rising ? y + i : y - i;
            if (!w.isAirBlock(x, yy, z)) return;
            w.setBlockAndMetadataJ(x, yy, z, ICICLE, rising ? 1 : 0);
        }
    }
    void glowCluster(JR& r, int x, int y, int z) {
        w.setBlock(x, y, z, GLOWSTONE);
        int n = 2 + r.nextInt(4);
        for (int i = 0; i < n; ++i) {
            int bx = x + r.nextInt(3) - 1;
            int by = y + r.nextInt(2);
            int bz = z + r.nextInt(3) - 1;
            if (stoneLike(w.blockAt(bx, by, bz))) w.setBlock(bx, by, bz, GLOWSTONE);
        }
    }
    void dress(JR& r, int x, int y, int z, int cb) {
        int below = w.blockAt(x, y - 1, z), above = w.blockAt(x, y + 1, z);
        static const int SIDE[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto& s : SIDE) {
            int nx = x + s[0], nz = z + s[1];
            int id = w.blockAt(nx, y, nz);
            if (stoneLike(id)) wall(nx, y, nz, cb);
            else if (cb == CB_FROZEN) iceOre(nx, y, nz, id);
        }
        if (stoneLike(above)) wall(x, y + 1, z, cb);
        else if (cb == CB_FROZEN) iceOre(x, y + 1, z, above);
        switch (cb) {
        case CB_FROZEN:
            if (below == WATER || below == WATER_STILL) {
                w.setBlock(x, y - 1, z, ICE);
            } else if (stoneLike(below)) {
                bool snowy = patch(x, y - 1, z, 7) > 0.05;
                w.setBlock(x, y - 1, z, snowy ? SNOW_BLOCK : COMPACTED_ICE);
                if (snowy && r.nextInt(3) == 0) w.setBlock(x, y, z, SNOW);
                else if (r.nextInt(28) == 0) icicle(r, x, y, z, true);
            }
            if (w.isBlockOpaqueCube(x, y + 1, z) && r.nextInt(9) == 0) icicle(r, x, y, z, false);
            break;
        case CB_LUSH:
            if (stoneLike(below)) {
                w.setBlock(x, y - 1, z, GRASS);
                if (stoneLike(w.blockAt(x, y - 2, z))) w.setBlock(x, y - 2, z, DIRT);
                if (r.nextInt(9) == 0) w.setBlock(x, y, z, WISP_LILY);
            }
            break;
        case CB_MUSHROOM:
            if (stoneLike(below)) {
                w.setBlock(x, y - 1, z, DIRT);
                if (r.nextInt(3) == 0) w.setBlock(x, y, z, r.nextInt(5) < 3 ? MUSH_B : MUSH_R);
            }
            if (r.nextInt(70) == 0 && w.isBlockOpaqueCube(x, y + 1, z)) glowCluster(r, x, y + 1, z);
            break;
        case CB_DEEP:
            if (stoneLike(below)) {
                if (patch(x, y - 1, z, 8) > 0.30) w.setBlockAndMetadataJ(x, y - 1, z, STONE_COBBLE, DEEP);
                else if (patch(x, y - 1, z, 9) > 0.45) w.setBlock(x, y - 1, z, GRAVEL);
                if (r.nextInt(30) == 0) w.setBlock(x, y, z, BONE_PILE);
            }
            break;
        }
    }
    void caves(JR& r, int x0, int z0, const std::vector<Biome>& biomes) {
        for (int dx = 0; dx < 16; ++dx)
            for (int dz = 0; dz < 16; ++dz) {
                int x = x0 + dx, z = z0 + dz;
                Biome b = biomes[dx * 16 + dz];
                int surf = w.getHeightValue(x, z);
                int cell = -1, cb = CB_NONE;
                for (int y = 2; y < surf - 10; ++y) {
                    if (w.blockAt(x, y, z) != 0) continue;
                    if (y >> 2 != cell) {
                        cell = y >> 2;
                        cb = caveBiome(seed, x, y, z, b, surf);
                    }
                    if (cb != CB_NONE) dress(r, x, y, z, cb);
                }
            }
    }
    void populate(int cx, int cz) {
        JR r(seed ^ add(add(mul(cx, 0x4F1BBCDCBFA53E0BLL), mul(cz, 0x2545F4914F6CDD1DLL)), 0xA7E01LL));
        int x0 = cx * 16 + 8, z0 = cz * 16 + 8;
        std::vector<Biome> biomes;
        g.manager->load(biomes, x0, z0, 16, 16);
        Biome mid = biomes[8 * 16 + 8];
        lakeExtra(r, x0, z0, mid);
        surface(r, x0, z0, mid);
        peakOres(r, x0, z0);
        caves(r, x0, z0, biomes);
    }
};
} // namespace

// BlockSapling.func_21028_c: the sapling becomes a tree (put back if there's no room)
bool growSapling(World& w, JavaRandom& r, int x, int y, int z, int id, int meta) {
    int type = meta & 7;
    bool charred = id == 119;
    if (charred) { // WorldGenCharredTree: a burnt, leafless trunk with a few crooked branches
        w.setBlock(x, y, z, 0);
        int height = 4 + r.nextInt(3);
        bool ok = !(y < 1 || y + height + 2 > 256);
        for (int dy = 1; ok && dy <= height + 1; ++dy)
            if (w.blockAt(x, y + dy, z) != 0) ok = false;
        if (ok) {
            const int LOG = 117;
            for (int dy = 0; dy < height; ++dy) w.setBlockAndMetadataJ(x, y + dy, z, LOG, type);
            int branches = 2 + r.nextInt(3);
            static const int DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int b = 0; b < branches; ++b) {
                const int* dir = DIRS[r.nextInt(4)];
                int by = y + height - 1 - r.nextInt(2), bx = x, bz = z;
                int length = 1 + r.nextInt(2);
                for (int step = 0; step < length; ++step) {
                    bx += dir[0];
                    bz += dir[1];
                    if (step > 0 || r.nextBoolean()) ++by;
                    if (w.blockAt(bx, by, bz) == 0) w.setBlockAndMetadataJ(bx, by, bz, LOG, type);
                }
            }
        } else {
            w.setBlockAndMetadataJ(x, y, z, id, meta);
        }
        return ok;
    }
    auto isThram = [&](int ax, int az) { return w.blockAt(ax, y, az) == id && (w.metaAt(ax, y, az) & 7) == 4; };
    if (type == 4) { // thramwood: four saplings in a square, like dark oak
        for (int dx = -1; dx <= 0; ++dx)
            for (int dz = -1; dz <= 0; ++dz) {
                int cx = x + dx, cz = z + dz;
                if (isThram(cx, cz) && isThram(cx + 1, cz) && isThram(cx, cz + 1) && isThram(cx + 1, cz + 1)) {
                    int m = w.metaAt(x, y, z);
                    w.setBlock(cx, y, cz, 0);
                    w.setBlock(cx + 1, y, cz, 0);
                    w.setBlock(cx, y, cz + 1, 0);
                    w.setBlock(cx + 1, y, cz + 1, 0);
                    if (!thramwood(w, r, cx, y, cz)) {
                        w.setBlockAndMetadataJ(cx, y, cz, id, m);
                        w.setBlockAndMetadataJ(cx + 1, y, cz, id, m);
                        w.setBlockAndMetadataJ(cx, y, cz + 1, id, m);
                        w.setBlockAndMetadataJ(cx + 1, y, cz + 1, id, m);
                        return false;
                    }
                    return true;
                }
            }
        return false;
    }
    w.setBlock(x, y, z, 0);
    bool ok;
    if (type == 1) ok = r.nextInt(3) == 0 ? taiga1(w, r, x, y, z) : taiga2(w, r, x, y, z);
    else if (type == 2) ok = oakTree(w, r, x, y, z, 2);
    else if (type == 3) ok = drogwood(w, r, x, y, z, false);
    else if (r.nextInt(10) == 0) { BigTree b; ok = b.generate(w, r, x, y, z); }
    else ok = oakTree(w, r, x, y, z, 0);
    if (!ok) w.setBlockAndMetadataJ(x, y, z, id, meta);
    return ok;
}

// ================================================================ Tabish's lair (StructureIceCave)

namespace {
struct Loot { // AxeolonLoot
    struct E { int id, min, max, weight; };
    int minRolls, maxRolls;
    std::vector<E> e;
    void fill(World& w, JR& r, int x, int y, int z) const {
        int total = 0;
        for (const E& x0 : e) total += x0.weight;
        int rolls = minRolls + (maxRolls > minRolls ? r.nextInt(maxRolls - minRolls + 1) : 0);
        for (int i = 0; i < rolls; ++i) {
            int k = r.nextInt(total);
            for (const E& en : e) {
                k -= en.weight;
                if (k < 0) {
                    int n = en.min + (en.max > en.min ? r.nextInt(en.max - en.min + 1) : 0);
                    putInChest(w, x, y, z, r.nextInt(27), Stack{en.id, n, 0});
                    break;
                }
            }
        }
    }
};
const Loot HOARD{6, 9, {{350, 2, 6, 10}, {352, 2, 8, 10}, {265, 2, 6, 8}, {266, 1, 4, 5}, {264, 1, 3, 3}, {322, 1, 1, 1}}};
const Loot LEFTOVERS{2, 5, {{352, 1, 4, 10}, {262, 2, 8, 8}, {332, 2, 8, 6}, {265, 1, 3, 4}, {298, 1, 1, 2}, {301, 1, 1, 2}, {263, 2, 6, 5}}};

struct IceCave {
    World& w;
    JR& r;
    int floorY = 0;
    void set(int x, int y, int z, int id, int meta = 0) {
        if (y < 1 || y > 254) return;
        w.setBlockAndMetadataJ(x, y, z, id, meta);
    }
    int get(int x, int y, int z) { return (y >= 0 && y < 256) ? w.blockAt(x, y, z) : 0; }
    bool solid(int x, int y, int z) { int id = get(x, y, z); return id != 0 && matSolid(BP[id].mat); }
    void wallBlock(int x, int y, int z) {
        int k = r.nextInt(20);
        if (k < 10) set(x, y, z, COMPACTED_ICE, 0);
        else if (k < 16) set(x, y, z, SNOW_BLOCK);
        else if (k < 18) set(x, y, z, ICE);
        else set(x, y, z, STONE);
    }
    void carve(double cx, double cy, double cz, double rx, double ry, double rz, int id, float rough) {
        for (int x = int(std::floor(cx - rx - 1)); x <= int(std::ceil(cx + rx + 1)); ++x)
            for (int y = int(std::floor(cy - ry - 1)); y <= int(std::ceil(cy + ry + 1)); ++y)
                for (int z = int(std::floor(cz - rz - 1)); z <= int(std::ceil(cz + rz + 1)); ++z) {
                    double dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry, dz = (z + 0.5 - cz) / rz;
                    if (dx * dx + dy * dy + dz * dz < 1.0 - double(r.nextFloat() * rough)) set(x, y, z, id, 0);
                }
    }
    void tunnel(double x0, double y0, double z0, double x1, double y1, double z1, double radius, int id) {
        double len = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + (z1 - z0) * (z1 - z0));
        int steps = std::max(1, int(len / 0.8));
        for (int i = 0; i <= steps; ++i) {
            double t = double(i) / double(steps);
            double rr = radius + (r.nextDouble() - 0.5) * 0.6;
            carve(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t, rr, rr, rr, id, 0.1f);
        }
    }
    void shell(double cx, double cy, double cz, double rx, double ry, double rz, double thick) {
        for (int x = int(std::floor(cx - rx - thick - 1)); x <= int(std::ceil(cx + rx + thick + 1)); ++x)
            for (int y = int(std::floor(cy - ry - thick - 1)); y <= int(std::ceil(cy + ry + thick + 1)); ++y)
                for (int z = int(std::floor(cz - rz - thick - 1)); z <= int(std::ceil(cz + rz + thick + 1)); ++z) {
                    double dx = (x + 0.5 - cx) / (rx + thick), dy = (y + 0.5 - cy) / (ry + thick), dz = (z + 0.5 - cz) / (rz + thick);
                    if (dx * dx + dy * dy + dz * dz < 1.0 && solid(x, y, z)) wallBlock(x, y, z);
                }
    }
    void flooring(double cx, double cz, double rad, int iceChance) {
        for (int x = int(cx - rad - 1); x <= int(cx + rad + 1); ++x)
            for (int z = int(cz - rad - 1); z <= int(cz + rad + 1); ++z) {
                double d = std::sqrt((x + 0.5 - cx) * (x + 0.5 - cx) + (z + 0.5 - cz) * (z + 0.5 - cz)) / rad;
                if (d >= 1.0) continue;
                for (int y = floorY - 3; y < floorY; ++y) set(x, y, z, COMPACTED_ICE, 0);
                set(x, floorY, z, r.nextInt(100) < iceChance ? ICE : SNOW_BLOCK);
                if (d < 0.9)
                    for (int y = floorY + 1; y < floorY + 3; ++y) set(x, y, z, 0);
                if (get(x, floorY, z) == SNOW_BLOCK && r.nextInt(6) == 0) set(x, floorY + 1, z, SNOW);
            }
    }
    void icicles(double cx, double cz, int count, double keepClear) {
        for (int i = 0; i < count; ++i) {
            double a = r.nextDouble() * PI * 2.0, d = keepClear + r.nextDouble() * 12.0;
            int px = int(std::floor(cx + std::cos(a) * d)), pz = int(std::floor(cz + std::sin(a) * d));
            if (solid(px, floorY + 1, pz)) continue;
            int roof = floorY + 1;
            while (roof < floorY + 20 && get(px, roof, pz) == 0) ++roof;
            if (roof >= floorY + 20 || !solid(px, roof, pz)) continue;
            int gap = roof - floorY - 1;
            int kind = r.nextInt(10);
            if (kind < 6) {
                int len = 1 + r.nextInt(std::min(4, std::max(1, gap - 3)));
                for (int k = 1; k <= len; ++k) set(px, roof - k, pz, ICICLE, 0);
            } else if (kind < 9) {
                int len = 1 + r.nextInt(std::min(3, std::max(1, gap - 3)));
                for (int k = 1; k <= len; ++k) set(px, floorY + k, pz, ICICLE, 1);
            } else if (gap >= 3 && gap <= 9) {
                int up = gap / 2;
                for (int k = 1; k <= up; ++k) set(px, floorY + k, pz, ICICLE, 1);
                for (int k = 1; k <= gap - up; ++k) set(px, roof - k, pz, ICICLE, 0);
            }
        }
    }
    void bones(double cx, double cz, int count, double from, double to) {
        for (int i = 0; i < count; ++i) {
            double a = r.nextDouble() * PI * 2.0, d = from + r.nextDouble() * (to - from);
            int px = int(std::floor(cx + std::cos(a) * d)), pz = int(std::floor(cz + std::sin(a) * d));
            int y = floorY + 1;
            if (get(px, y, pz) == SNOW) set(px, y, pz, 0);
            if (get(px, y, pz) == 0 && solid(px, y - 1, pz)) set(px, y, pz, BONE_PILE);
        }
    }
    void lamps(double cx, double cz, double rad, int count) {
        for (int i = 0; i < count; ++i) {
            double a = double(i) / double(count) * PI * 2.0 + r.nextDouble() * 0.3;
            for (double d = rad - 3.0; d < rad + 4.0; d += 0.5) {
                int px = int(std::floor(cx + std::cos(a) * d)), pz = int(std::floor(cz + std::sin(a) * d));
                if (solid(px, floorY + 4, pz)) {
                    set(px, floorY + 4, pz, ICE);
                    int bx = int(std::floor(cx + std::cos(a) * (d + 1.0))), bz = int(std::floor(cz + std::sin(a) * (d + 1.0)));
                    set(bx, floorY + 4, bz, GLOWSTONE);
                    break;
                }
            }
        }
    }
    void chest(int x, int y, int z, const Loot& loot) {
        set(x, y, z, CRATE);
        loot.fill(w, r, x, y, z);
    }
    int surface(int x, int z) { return w.findTopSolidBlock(x, z); }

    void build(int x, int z) {
        r.nextInt(4); // AxeolonStructure.place: the rotation (unused by the cave)
        int top = std::max(64, std::min(115, surface(x, z)));
        floorY = std::max(14, std::min(top - 26, 42));
        bosses::setInt("lair.floor", floorY);
        double ang = r.nextDouble() * PI * 2.0;
        double fx = std::cos(ang), fz = std::sin(ang);
        double hx = x + 0.5, hz = z + 0.5, hy = floorY + 6.0;
        double ax = hx + fx * 31.0, az = hz + fz * 31.0;
        shell(hx, hy, hz, 18.0, 9.0, 18.0, 3.0);
        shell(ax, hy, az, 14.0, 9.0, 14.0, 3.0);
        carve(hx, hy, hz, 18.0, 9.0, 18.0, 0, 0.12f);
        carve(ax, hy, az, 14.0, 9.0, 14.0, 0, 0.06f);
        tunnel(hx + fx * 14.0, floorY + 4.0, hz + fz * 14.0, ax - fx * 11.0, floorY + 4.0, az - fz * 11.0, 3.6, 0);
        flooring(hx, hz, 18.0, 22);
        flooring(ax, az, 14.0, 8);
        flooring(hx + fx * 15.5, hz + fz * 15.5, 5.0, 0);
        int gx = int(std::floor(ax - fx * 13.5)), gz = int(std::floor(az - fz * 13.5));
        bool alongX = std::fabs(fx) < std::fabs(fz);
        for (int sdx = -5; sdx <= 5; ++sdx)
            for (int y = floorY; y <= floorY + 9; ++y) {
                int px = alongX ? gx + sdx : gx, pz = alongX ? gz : gz + sdx;
                bool opening = sdx >= -3 && sdx <= 3 && y >= floorY + 1 && y <= floorY + 7;
                set(px, y, pz, opening ? 0 : COMPACTED_ICE, opening ? 0 : 1);
            }
        for (int i = 0; i < 7; ++i) { // ice pillars
            double a = r.nextDouble() * PI * 2.0, d = 7.0 + r.nextDouble() * 7.0;
            double px = hx + std::cos(a) * d, pz = hz + std::sin(a) * d;
            if ((px - hx) * fx + (pz - hz) * fz > 10.0) continue;
            double rad = 1.2 + r.nextDouble() * 0.8;
            for (int bx = int(px - rad - 1); bx <= int(px + rad + 1); ++bx)
                for (int bz = int(pz - rad - 1); bz <= int(pz + rad + 1); ++bz) {
                    double dd = std::sqrt((bx + 0.5 - px) * (bx + 0.5 - px) + (bz + 0.5 - pz) * (bz + 0.5 - pz));
                    if (dd > rad) continue;
                    for (int y = floorY + 1; y < floorY + 20 && !solid(bx, y, bz); ++y)
                        set(bx, y, bz, dd < rad - 0.8 ? COMPACTED_ICE : ICE, 0);
                }
        }
        for (int i = 0; i < 3; ++i) { // side tunnels with old loot
            double a = ang + PI * 0.5 + double(i) * PI * 0.5 + (r.nextDouble() - 0.5) * 0.6;
            double px = hx + std::cos(a) * 16.0, pz = hz + std::sin(a) * 16.0;
            int len = 12 + r.nextInt(8);
            for (int k = 0; k < len; ++k) {
                a += (r.nextDouble() - 0.5) * 0.5;
                px += std::cos(a);
                pz += std::sin(a);
                int bx = int(std::floor(px)), bz = int(std::floor(pz));
                set(bx, floorY + 1, bz, 0);
                set(bx, floorY + 2, bz, 0);
                if (!solid(bx, floorY, bz)) set(bx, floorY, bz, SNOW_BLOCK);
            }
            int ex = int(std::floor(px)), ez = int(std::floor(pz));
            carve(ex + 0.5, floorY + 2.0, ez + 0.5, 2.2, 1.6, 2.2, 0, 0.1f);
            if (!solid(ex, floorY, ez)) set(ex, floorY, ez, SNOW_BLOCK);
            if (r.nextInt(3) > 0) chest(ex, floorY + 1, ez, LEFTOVERS);
            else set(ex, floorY + 1, ez, BONE_PILE);
        }
        // the way in
        double ex = hx - fx * 46.0, ez = hz - fz * 46.0;
        int ey = surface(int(std::floor(ex)), int(std::floor(ez)));
        double side = r.nextBoolean() ? 1.0 : -1.0;
        double m1x = hx - fx * 34.0 - fz * 8.0 * side, m1z = hz - fz * 34.0 + fx * 8.0 * side;
        double m2x = hx - fx * 24.0 + fz * 6.0 * side, m2z = hz - fz * 24.0 - fx * 6.0 * side;
        double m1y = ey - 12.0, m2y = floorY + 6.0;
        for (int k = 0; k < 6; ++k) carve(ex + fx * k * 0.8, ey - k * 1.4 + 1.0, ez + fz * k * 0.8, 1.1, 2.6, 1.1, 0, 0.05f);
        tunnel(ex + fx * 4.0, ey - 6.0, ez + fz * 4.0, m1x, m1y, m1z, 1.8, 0);
        tunnel(m1x, m1y, m1z, m2x, m2y, m2z, 2.0, 0);
        tunnel(m2x, m2y, m2z, hx - fx * 15.0, floorY + 3.0, hz - fz * 15.0, 2.2, 0);
        for (int dx = -3; dx <= 3; ++dx)
            for (int dz = -3; dz <= 3; ++dz) {
                int px = int(std::floor(ex)) + dx, pz = int(std::floor(ez)) + dz;
                int y = surface(px, pz);
                if (get(px, y, pz) == 0 && solid(px, y - 1, pz) && r.nextInt(3) > 0) set(px, y, pz, SNOW);
            }
        icicles(hx, hz, 60, 3.0);
        icicles(ax, az, 30, 7.0);
        bones(hx, hz, 10, 4.0, 16.0);
        bones(ax, az, 7, 6.0, 12.0);
        lamps(hx, hz, 17.0, 6);
        lamps(ax, az, 13.0, 4);
        chest(int(std::floor(ax + fx * 9.0)), floorY + 1, int(std::floor(az + fz * 9.0)), HOARD);
        // Tabish, asleep in the middle of her chamber, facing the gate (AxeolonStructure.boss)
        if (!bosses::hasSpawned("tabish")) {
            float yaw = float(std::atan2(fx, -fz) * 180.0 / PI);
            nbt::TagPtr t = nbt::makeCompound();
            t->setString("id", "Tabish");
            nbt::TagPtr pos = nbt::makeList(nbt::Double);
            pos->items.push_back(nbt::makeDouble(ax));
            pos->items.push_back(nbt::makeDouble(floorY + 1.0));
            pos->items.push_back(nbt::makeDouble(az));
            t->set("Pos", std::move(pos));
            nbt::TagPtr mo = nbt::makeList(nbt::Double);
            for (int i = 0; i < 3; ++i) mo->items.push_back(nbt::makeDouble(0.0));
            t->set("Motion", std::move(mo));
            nbt::TagPtr ro = nbt::makeList(nbt::Float);
            ro->items.push_back(nbt::makeFloat(yaw));
            ro->items.push_back(nbt::makeFloat(0.0f));
            t->set("Rotation", std::move(ro));
            t->setShort("Health", 200);
            t->setInt("ArenaX", int(std::floor(ax)));
            t->setInt("ArenaY", floorY + 1);
            t->setInt("ArenaZ", int(std::floor(az)));
            t->setInt("ArenaR", 13);
            int g[6];
            if (alongX) { g[0] = gx - 3; g[1] = floorY + 1; g[2] = gz; g[3] = gx + 3; g[4] = floorY + 7; g[5] = gz; }
            else { g[0] = gx; g[1] = floorY + 1; g[2] = gz - 3; g[3] = gx; g[4] = floorY + 7; g[5] = gz + 3; }
            for (int i = 0; i < 6; ++i) t->setInt(("Gate" + std::to_string(i)).c_str(), g[i]);
            t->setInt("GateBlock", COMPACTED_ICE);
            t->setInt("GateMeta", 1);
            w.spawnEntity(std::move(t));
            bosses::markSpawned("tabish");
        }
    }
};
} // namespace

// AxeolonLair.site: a snowy spot 500-1,500 blocks from spawn, picked from the seed
static bool lairSite(World& w, ChunkManager& cm, int64_t seed, int& sx, int& sz) {
    if (bosses::hasInt("lair.x")) {
        sx = bosses::getInt("lair.x");
        sz = bosses::getInt("lair.z");
        return true;
    }
    JR r(add(mul(seed, 31), 0x7AB15));
    bool snowy = false, cold = false, any = false;
    int s[2] = {0, 0}, c[2] = {0, 0}, a2[2] = {0, 0};
    for (int i = 0; i < 800 && !snowy; ++i) {
        double a = r.nextDouble() * PI * 2.0, d = 500.0 + r.nextDouble() * 1000.0;
        int x = w.info.spawnX + int(std::cos(a) * d), z = w.info.spawnZ + int(std::sin(a) * d);
        Biome b = cm.biomeAt(x, z);
        if (b == Biome::Tundra || b == Biome::IceDesert) { snowy = true; s[0] = x; s[1] = z; }
        else if (!cold && b == Biome::Taiga) { cold = true; c[0] = x; c[1] = z; }
        else if (!any) { any = true; a2[0] = x; a2[1] = z; }
    }
    const int* pick = snowy ? s : cold ? c : a2;
    sx = pick[0];
    sz = pick[1];
    bosses::setInt("lair.x", sx);
    bosses::setInt("lair.z", sz);
    return true;
}

// ================================================================ ChunkProviderGenerate.populate

void ChunkGenerator::populate(World& w, int cx, int cz) {
    if (hell) {
        populateHell(w, hell->hellRNG, cx, cz);
        return;
    }
    if (alphaTerrain) return; // NSSS: Alpha's populate step (ChunkProviderGenerate.generateStructures) is next to port
    Blocks::sandFallInstantly = true;
    Blocks::flowerGenerating = true;
    int bx = cx * 16, bz = cz * 16;
    Biome biome = manager->biomeAt(bx + 16, bz + 16);
    JR& r = rand;
    r.setSeed(seed);
    int64_t a = r.nextLong() / 2 * 2 + 1;
    int64_t b = r.nextLong() / 2 * 2 + 1;
    r.setSeed(add(mul(cx, a), mul(cz, b)) ^ seed);
    int x, y, z;
    auto at8 = [&](int range) {
        x = bx + r.nextInt(16) + 8;
        y = r.nextInt(range);
        z = bz + r.nextInt(16) + 8;
    };
    if (r.nextInt(4) == 0) {
        at8(128);
        lakes(w, r, WATER, x, y, z);
    }
    if (r.nextInt(8) == 0) {
        x = bx + r.nextInt(16) + 8;
        y = r.nextInt(r.nextInt(120) + 8);
        z = bz + r.nextInt(16) + 8;
        if (y < 64 || r.nextInt(10) == 0) lakes(w, r, LAVA, x, y, z);
    }
    for (int i = 0; i < 8; ++i) {
        at8(128);
        dungeon(w, r, x, y, z);
    }
    {
        JR pr(add(add(add(seed, mul(cx, 341873128712LL)), mul(cz, 132897987541LL)), 4242));
        if (pr.nextInt(1000) == 0) {
            int px = bx + 12 + pr.nextInt(8);
            int pz = bz + 12 + pr.nextInt(8);
            ruinedPortal(w, pr, px, 0, pz);
        }
    }
    // AxeolonStructures.populate: Tabish's lair is the one structure that generates on its own
    // (AxeolonLair.populate: when this chunk's decoration area holds the site)
    if (!bosses::hasSpawned("structure.ice_cave")) {
        int sx, sz;
        lairSite(w, *manager, seed, sx, sz);
        int dx0 = cx * 16 + 8, dz0 = cz * 16 + 8;
        if (sx >= dx0 && sx < dx0 + 16 && sz >= dz0 && sz < dz0 + 16) {
            JR lr(add(int64_t(std::time(nullptr)), mul(cx, 341873128712LL))); // new Random(): not from the seed
            IceCave cave{w, lr};
            cave.build(sx, sz);
            bosses::markSpawned("structure.ice_cave");
        }
    }
    {
        JR pr(seed ^ add(mul(cx, 91847263LL), mul(cz, 57264839LL)));
        stonePatches(w, pr, cx * 16, cz * 16);
    }
    auto at0 = [&](int range) {
        x = bx + r.nextInt(16);
        y = r.nextInt(range);
        z = bz + r.nextInt(16);
    };
    for (int i = 0; i < 10; ++i) { at0(128); clay(w, r, 32, x, y, z); }
    for (int i = 0; i < 20; ++i) { at0(128); minable(w, r, DIRT, 32, x, y, z); }
    for (int i = 0; i < 10; ++i) { at0(128); minable(w, r, GRAVEL, 32, x, y, z); }
    for (int i = 0; i < 20; ++i) { at0(128); minable(w, r, COAL_ORE, 16, x, y, z); }
    for (int i = 0; i < 20; ++i) { at0(64); minable(w, r, IRON_ORE, 8, x, y, z); }
    for (int i = 0; i < 2; ++i) { at0(32); minable(w, r, GOLD_ORE, 8, x, y, z); }
    for (int i = 0; i < 8; ++i) { at0(16); minable(w, r, REDSTONE_ORE, 7, x, y, z); }
    for (int i = 0; i < 1; ++i) { at0(16); minable(w, r, DIAMOND_ORE, 7, x, y, z); }
    for (int i = 0; i < 1; ++i) {
        x = bx + r.nextInt(16);
        y = r.nextInt(16);
        y += r.nextInt(16);
        z = bz + r.nextInt(16);
        minable(w, r, LAPIS_ORE, 6, x, y, z);
    }
    double treeNoise = mobNoise.value2D(double(bx) * 0.5, double(bz) * 0.5);
    int extra = int((treeNoise / 8.0 + r.nextDouble() * 4.0 + 4.0) / 3.0);
    int trees = 0;
    if (r.nextInt(10) == 0) ++trees;
    switch (biome) {
    case Biome::Forest: case Biome::BirchForest: case Biome::Rainforest: case Biome::Taiga: trees += extra + 5; break;
    case Biome::DenseForest: trees += extra + 12; break;
    case Biome::Thrams: trees += extra + 1; break;
    case Biome::Drogwoods: trees += 2; break;
    case Biome::SeasonalForest: case Biome::Peaks: trees += extra + 2; break;
    case Biome::Desert: case Biome::Tundra: case Biome::Plains: trees -= 20; break;
    default: break;
    }
    for (int i = 0; i < trees; ++i) {
        int tx = bx + r.nextInt(16) + 8;
        int tz = bz + r.nextInt(16) + 8;
        Tree t = pickTree(biome, r);
        growTree(t, w, r, tx, w.getHeightValue(tx, tz), tz);
    }
    for (int i = 0; i < (biome == Biome::Drogwoods ? 0 : 2); ++i) {
        at8(128);
        flowers(w, r, FLOWER_Y, x, y, z);
    }
    int grassPatches = 0;
    if (biome == Biome::Forest || biome == Biome::BirchForest || biome == Biome::DenseForest) grassPatches = 2;
    if (biome == Biome::Thrams) grassPatches = 6;
    if (biome == Biome::Drogwoods) grassPatches = 2;
    if (biome == Biome::Rainforest) grassPatches = 10;
    if (biome == Biome::SeasonalForest) grassPatches = 2;
    if (biome == Biome::Taiga) grassPatches = 1;
    if (biome == Biome::Plains) grassPatches = 10;
    for (int p = 0; p < grassPatches; ++p) {
        int type = 1;
        if (biome == Biome::Rainforest && r.nextInt(3) != 0) type = 2;
        at8(128);
        scatterOnGround(w, r, TALLGRASS, type, 128, x, y, z, true);
    }
    int dbl = 0, fern = 0;
    if (biome == Biome::Plains) dbl = 2;
    if (biome == Biome::Forest || biome == Biome::BirchForest || biome == Biome::SeasonalForest) dbl = r.nextInt(2);
    if (biome == Biome::Rainforest) fern = 2;
    if (biome == Biome::Taiga) fern = r.nextInt(3) == 0 ? 1 : 0;
    for (int p = 0; p < dbl + fern; ++p) {
        at8(128);
        doublePlant(w, r, DOUBLE_PLANT, p < dbl ? 0 : 1, 32, x, y, z);
    }
    if ((biome == Biome::Shrubland && r.nextInt(5) == 0) || (biome == Biome::SeasonalForest && r.nextInt(12) == 0)) {
        int tx = bx + r.nextInt(16) + 8;
        int tz = bz + r.nextInt(16) + 8;
        drogwood(w, r, tx, w.getHeightValue(tx, tz), tz, true);
    }
    if (biome == Biome::Thrams) {
        for (int i = 0; i < 3; ++i) {
            int tx = bx + r.nextInt(15) + 8;
            int tz = bz + r.nextInt(15) + 8;
            thramwood(w, r, tx, w.getHeightValue(tx, tz), tz);
        }
        if (r.nextInt(2) == 0) {
            int fx = bx + r.nextInt(16) + 8;
            int fz = bz + r.nextInt(16) + 8;
            flowersMeta(w, r, WISP_LILY, 0, 24, fx, w.getHeightValue(fx, fz), fz);
        }
    }
    auto onSurface = [&](int id, int type, int tries) {
        int fx = bx + r.nextInt(16) + 8;
        int fz = bz + r.nextInt(16) + 8;
        doublePlant(w, r, id, type, tries, fx, w.getHeightValue(fx, fz), fz);
    };
    bool flowery = biome == Biome::Forest || biome == Biome::BirchForest || biome == Biome::DenseForest ||
                   biome == Biome::Rainforest || biome == Biome::SeasonalForest || biome == Biome::Plains ||
                   biome == Biome::Shrubland;
    if (flowery && r.nextInt(16) == 0) {
        int fx = bx + r.nextInt(16) + 8;
        int fz = bz + r.nextInt(16) + 8;
        int type = r.nextBoolean() ? 3 : 4; // ROSE : DANDELION
        doublePlant(w, r, DOUBLE_PLANT, type, 8, fx, w.getHeightValue(fx, fz), fz);
    }
    bool woods = biome == Biome::Forest || biome == Biome::BirchForest || biome == Biome::DenseForest || biome == Biome::Rainforest;
    if (woods && r.nextInt(32) == 0) onSurface(DOUBLE_GLOW, 0, 6);
    if (biome == Biome::Plains && r.nextInt(12) == 0) onSurface(DOUBLE_PLANT2, 0, 8);
    if (biome == Biome::Drogwoods && r.nextInt(4) == 0) onSurface(DOUBLE_PLANT, 7, 8);
    if (r.nextInt(6) == 0) {
        int fx = bx + r.nextInt(16) + 8;
        int fy = 8 + r.nextInt(47);
        int fz = bz + r.nextInt(16) + 8;
        if (!w.canBlockSeeTheSky(fx, fy, fz)) doublePlant(w, r, DOUBLE_GLOW, 1, 6, fx, fy, fz);
    }
    int cattailPatches = biome == Biome::Swampland ? 6 : (r.nextInt(3) == 0 ? 1 : 0);
    for (int p = 0; p < cattailPatches; ++p) {
        int px = bx + r.nextInt(16) + 8;
        int pz = bz + r.nextInt(16) + 8;
        cattails(w, r, px, pz);
    }
    if (woods && r.nextInt(8) == 0) {
        at8(128);
        flowersMeta(w, r, MANABLOOM, 0, 16, x, y, z);
    }
    if (biome == Biome::Plains && r.nextInt(2) == 0) {
        at8(128);
        flowersMeta(w, r, AX_FLOWERS, 2, 32, x, y, z);
    }
    if ((biome == Biome::Plains || biome == Biome::Shrubland) && r.nextInt(3) == 0) {
        at8(128);
        doublePlant(w, r, DOUBLE_PLANT, 2, 32, x, y, z);
    }
    for (int i = 0; i < 2; ++i) {
        int fx = bx + r.nextInt(16) + 8;
        int fy = 8 + r.nextInt(47);
        int fz = bz + r.nextInt(16) + 8;
        if (!w.canBlockSeeTheSky(fx, fy, fz)) flowersMeta(w, r, WISP_LILY, 0, 12, fx, fy, fz);
    }
    if (biome == Biome::Desert || biome == Biome::Drogwoods) {
        for (int i = 0; i < (biome == Biome::Drogwoods ? 4 : 2); ++i) {
            at8(128);
            scatterOnGround(w, r, DEADBUSH, 0, 4, x, y, z, false);
        }
    }
    if (biome != Biome::Drogwoods && r.nextInt(2) == 0) { at8(128); flowers(w, r, FLOWER_R, x, y, z); }
    if (r.nextInt(4) == 0) { at8(128); flowers(w, r, MUSH_B, x, y, z); }
    if (r.nextInt(8) == 0) { at8(128); flowers(w, r, MUSH_R, x, y, z); }
    for (int i = 0; i < 10; ++i) {
        int rx = bx + r.nextInt(16) + 8;
        int rz = bz + r.nextInt(16) + 8;
        reeds(w, r, rx, w.getHeightValue(rx, rz), rz);
    }
    if (r.nextInt(32) == 0) { at8(128); pumpkinPatch(w, r, x, y, z); }
    int cacti = biome == Biome::Desert ? 10 : 0;
    for (int i = 0; i < cacti; ++i) { at8(128); cactus(w, r, x, y, z); }
    for (int i = 0; i < 50; ++i) {
        x = bx + r.nextInt(16) + 8;
        y = r.nextInt(r.nextInt(120) + 8);
        z = bz + r.nextInt(16) + 8;
        liquids(w, r, WATER, x, y, z);
    }
    for (int i = 0; i < 20; ++i) {
        x = bx + r.nextInt(16) + 8;
        y = r.nextInt(r.nextInt(r.nextInt(112) + 8) + 8);
        z = bz + r.nextInt(16) + 8;
        liquids(w, r, LAVA, x, y, z);
    }
    AxGen ax{w, seed, *this};
    ax.populate(cx, cz);
    // snow, except in the Thrams
    std::vector<Biome> snowBiomes;
    manager->load(snowBiomes, bx + 8, bz + 8, 16, 16);
    manager->getTemperatures(generatedTemperatures, bx + 8, bz + 8, 16, 16);
    for (int sx = bx + 8; sx < bx + 8 + 16; ++sx)
        for (int sz = bz + 8; sz < bz + 8 + 16; ++sz) {
            int i = sx - (bx + 8), k = sz - (bz + 8);
            int top = w.findTopSolidBlock(sx, sz);
            double t = generatedTemperatures[i * 16 + k] - double(top - 64) / 64.0 * 0.3;
            if (snowBiomes[i * 16 + k] != Biome::Thrams && t < 0.5 && top > 0 && top < 128 && w.isAirBlock(sx, top, sz) &&
                matSolid(w.materialAt(sx, top - 1, sz)) && w.materialAt(sx, top - 1, sz) != Mat::ice)
                w.setBlockWithNotify(sx, top, sz, SNOW);
        }
    Blocks::sandFallInstantly = false;
    Blocks::flowerGenerating = false;
}

// ================================================================ the Nether (ChunkProviderHell.populate)

namespace {
void hellLava(World& w, JR& r, int x, int y, int z) { // WorldGenHellLava ("lavaStill" in MCP names is the flowing id 10)
    if (w.blockAt(x, y + 1, z) != BLOODSTONE) return;
    if (w.blockAt(x, y, z) != 0 && w.blockAt(x, y, z) != BLOODSTONE) return;
    int rock = 0, air = 0;
    if (w.blockAt(x - 1, y, z) == BLOODSTONE) ++rock;
    if (w.blockAt(x + 1, y, z) == BLOODSTONE) ++rock;
    if (w.blockAt(x, y, z - 1) == BLOODSTONE) ++rock;
    if (w.blockAt(x, y, z + 1) == BLOODSTONE) ++rock;
    if (w.blockAt(x, y - 1, z) == BLOODSTONE) ++rock;
    if (w.isAirBlock(x - 1, y, z)) ++air;
    if (w.isAirBlock(x + 1, y, z)) ++air;
    if (w.isAirBlock(x, y, z - 1)) ++air;
    if (w.isAirBlock(x, y, z + 1)) ++air;
    if (w.isAirBlock(x, y - 1, z)) ++air;
    if (rock == 4 && air == 1) {
        w.setBlockWithNotify(x, y, z, LAVA);
        w.scheduledUpdatesAreImmediate = true;
        Blocks::updateTick(w, x, y, z, LAVA, r);
        w.scheduledUpdatesAreImmediate = false;
    }
}

void hellFire(World& w, JR& r, int x, int y, int z) { // WorldGenFire
    for (int i = 0; i < 64; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        a = r.nextInt(4);
        int py = y + a - r.nextInt(4);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.isAirBlock(px, py, pz) && w.blockAt(px, py - 1, pz) == BLOODSTONE) w.setBlockWithNotify(px, py, pz, 51);
    }
}

void glowstone(World& w, JR& r, int x, int y, int z) { // WorldGenLightStone1 / 2 (the same)
    if (!w.isAirBlock(x, y, z)) return;
    if (w.blockAt(x, y + 1, z) != BLOODSTONE) return;
    w.setBlockWithNotify(x, y, z, GLOWSTONE);
    for (int i = 0; i < 1500; ++i) {
        int a = r.nextInt(8);
        int px = x + a - r.nextInt(8);
        int py = y - r.nextInt(12);
        a = r.nextInt(8);
        int pz = z + a - r.nextInt(8);
        if (w.blockAt(px, py, pz) != 0) continue;
        int n = 0;
        for (int k = 0; k < 6; ++k) {
            int id = 0;
            if (k == 0) id = w.blockAt(px - 1, py, pz);
            if (k == 1) id = w.blockAt(px + 1, py, pz);
            if (k == 2) id = w.blockAt(px, py - 1, pz);
            if (k == 3) id = w.blockAt(px, py + 1, pz);
            if (k == 4) id = w.blockAt(px, py, pz - 1);
            if (k == 5) id = w.blockAt(px, py, pz + 1);
            if (id == GLOWSTONE) ++n;
        }
        if (n == 1) w.setBlockWithNotify(px, py, pz, GLOWSTONE);
    }
}
} // namespace

void populateHell(World& w, JavaRandom& r, int cx, int cz) {
    Blocks::sandFallInstantly = true;
    Blocks::flowerGenerating = true;
    int x0 = cx * 16, z0 = cz * 16;
    int x, y, z;
    auto at = [&](int range) {
        x = x0 + r.nextInt(16) + 8;
        y = r.nextInt(range);
        z = z0 + r.nextInt(16) + 8;
    };
    for (int i = 0; i < 8; ++i) {
        x = x0 + r.nextInt(16) + 8;
        y = r.nextInt(120) + 4;
        z = z0 + r.nextInt(16) + 8;
        hellLava(w, r, x, y, z);
    }
    int n = r.nextInt(r.nextInt(10) + 1) + 1;
    for (int i = 0; i < n; ++i) {
        x = x0 + r.nextInt(16) + 8;
        y = r.nextInt(120) + 4;
        z = z0 + r.nextInt(16) + 8;
        hellFire(w, r, x, y, z);
    }
    n = r.nextInt(r.nextInt(10) + 1);
    for (int i = 0; i < n; ++i) {
        x = x0 + r.nextInt(16) + 8;
        y = r.nextInt(120) + 4;
        z = z0 + r.nextInt(16) + 8;
        glowstone(w, r, x, y, z);
    }
    for (int i = 0; i < 10; ++i) {
        at(128);
        glowstone(w, r, x, y, z);
    }
    if (r.nextInt(1) == 0) {
        at(128);
        flowers(w, r, MUSH_B, x, y, z);
    }
    if (r.nextInt(1) == 0) {
        at(128);
        flowers(w, r, MUSH_R, x, y, z);
    }
    for (int patch = 0; patch < 2; ++patch) { // Axeolon: ember buds on netherrack and soul sand
        at(128);
        flowersMeta(w, r, AX_FLOWERS, 1, 32, x, y, z);
    }
    if (r.nextInt(8) == 0) { // the odd tall ember bud
        at(128);
        doublePlant(w, r, DOUBLE_PLANT, 6, 8, x, y, z);
    }
    Blocks::sandFallInstantly = false;
    Blocks::flowerGenerating = false;
}
