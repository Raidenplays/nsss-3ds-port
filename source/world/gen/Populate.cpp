// NSSS (Alpha a1.1.12) decoration: ChunkProviderGenerate.generateStructures and the WorldGen*
// classes it uses, ported line by line from the jar and checked against it (tools/host/alphapop.cpp
// vs tools/nsss/AlphaPopDump.java). Written against World's Java-style calls.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ChunkGenerator.h"
#include "world/BlockProps.h"
#include "world/Blocks.h"
#include "world/MathHelper.h"
#include "world/World.h"

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


// ================================================================ WorldGenBigTree

struct BigTree {
    static constexpr int8_t AXES[6] = {2, 0, 0, 1, 2, 1};
    JR rand;
    World* world = nullptr;
    int base[3] = {0, 0, 0};
    int total = 0; // field_878_e
    int height = 0;
    double hAtt = 0.618, branchDensity = 1.0, branchSlope = 0.381, scaleW = 1.0, leafDensity = 1.0;
    int trunkSize = 1, heightLimitLimit = 12, leafDistance = 4;
    bool fromSapling = false;
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
        if (!fromSapling) { // func_517_a(1, 1, 1) from populate (a sapling's tree keeps the defaults)
            heightLimitLimit = 12;
            leafDistance = 5;
            scaleW = 1.0;
            leafDensity = 1.0;
        }
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
} // namespace

// ================================================================ NSSS (Alpha a1.1.12) populate
// ChunkProviderGenerate.generateStructures and its WorldGen* classes from the a1.1.12 jar, line by
// line. Checked against the jar with tools/host/alphapop.cpp vs tools/nsss/AlphaPopDump.java.
namespace nsss {
constexpr int BRICK = 45, MOSSY_COBBLE = 48, CRATE_ID = 54, SPONGE = 19, BRICK_MOSSY = 93, WEB = 95, CLAY_RED = 103,
              DIRT_ID = 3, GRAVEL_ID = 13, SAND_ID = 12, CLAY_ID = 82, COPPER_ORE = 89, PYRAMIDION = 99,
              BLUE_ROSE = 88, ITEM_RECORD13 = 2256;
inline float asin_(float v) { return alpha::sin(v); }
inline float acos_(float v) { return alpha::cos(v); }

void minable(World& w, JR& r, int id, int n, int x, int y, int z) { // WorldGenMinable
    float f = r.nextFloat() * 3.141593f;
    double d = double(float(x + 8) + asin_(f) * float(n) / 8.0f);
    double d1 = double(float(x + 8) - asin_(f) * float(n) / 8.0f);
    double d2 = double(float(z + 8) + acos_(f) * float(n) / 8.0f);
    double d3 = double(float(z + 8) - acos_(f) * float(n) / 8.0f);
    double d4 = y + r.nextInt(3) + 2;
    double d5 = y + r.nextInt(3) + 2;
    for (int l = 0; l <= n; ++l) {
        double d6 = d + (d1 - d) * l / n, d7 = d4 + (d5 - d4) * l / n, d8 = d2 + (d3 - d2) * l / n;
        double d9 = r.nextDouble() * n / 16.0;
        double d10 = double(asin_(float(l) * 3.141593f / float(n)) + 1.0f) * d9 + 1.0;
        double d11 = double(asin_(float(l) * 3.141593f / float(n)) + 1.0f) * d9 + 1.0;
        for (double i1 = d6 - d10 / 2.0; i1 <= d6 + d10 / 2.0; i1++)
            for (double j1 = d7 - d11 / 2.0; j1 <= int(d7 + d11 / 2.0); j1++)
                for (double k1 = d8 - d10 / 2.0; k1 <= int(d8 + d10 / 2.0); k1++) {
                    double d12 = (std::floor(i1) + 0.5 - d6) / (d10 / 2.0);
                    double d13 = (std::floor(j1) + 0.5 - d7) / (d11 / 2.0);
                    double d14 = (std::floor(k1) + 0.5 - d8) / (d10 / 2.0);
                    int bx = int(std::floor(i1)), by = int(std::floor(j1)), bz = int(std::floor(k1));
                    if (d12 * d12 + d13 * d13 + d14 * d14 < 1.0 && w.blockAt(bx, by, bz) == STONE) w.setBlock(bx, by, bz, id);
                }
    }
}

// WorldGenClay (sand/gravel -> clay, under sand) and WorldGenClayRed (dirt -> red clay, in the ground):
// the same shape; Alpha's check is "< 100", not "< 1", so the blobs are boxes
void clayLike(World& w, JR& r, int n, int x, int y, int z, Mat need, int out, bool red) {
    if (w.materialAt(x, y, z) != need) return;
    float f = r.nextFloat() * 3.141593f;
    double d = double(float(x + 8) + asin_(f) * float(n) / 8.0f);
    double d1 = double(float(x + 8) - asin_(f) * float(n) / 8.0f);
    double d2 = double(float(z + 8) + acos_(f) * float(n) / 8.0f);
    double d3 = double(float(z + 8) - acos_(f) * float(n) / 8.0f);
    double d4 = y + r.nextInt(3) + 2;
    double d5 = y + r.nextInt(3) + 2;
    for (int l = 0; l <= n; ++l) {
        double d6 = d + (d1 - d) * l / n, d7 = d4 + (d5 - d4) * l / n, d8 = d2 + (d3 - d2) * l / n;
        double d9 = r.nextDouble() * n / 16.0;
        double d10 = double(asin_(float(l) * 3.141593f / float(n)) + 1.0f) * d9 + 1.0;
        double d11 = double(asin_(float(l) * 3.141593f / float(n)) + 1.0f) * d9 + 1.0;
        for (double i1 = d6 - d10 / 2.0; i1 <= d6 + d10 / 2.0; i1++)
            for (double j1 = d7 - d11 / 2.0; j1 <= d7 + d11 / 2.0; j1++)
                for (double k1 = d8 - d10 / 2.0; k1 <= d8 + d10 / 2.0; k1++) {
                    double d12 = (std::floor(i1) + 0.5 - d6) / (d10 / 2.0);
                    double d13 = (std::floor(j1) + 0.5 - d7) / (d11 / 2.0);
                    double d14 = (std::floor(k1) + 0.5 - d8) / (d10 / 2.0);
                    if (!(d12 * d12 + d13 * d13 + d14 * d14 >= 100.0)) {
                        int bx = int(std::floor(i1)), by = int(std::floor(j1)), bz = int(std::floor(k1));
                        int here = w.blockAt(bx, by, bz);
                        bool take = red ? here == DIRT_ID : (here == GRAVEL_ID || here == SAND_ID);
                        if (take) w.setBlock(bx, by, bz, out);
                    }
                }
    }
}

const char* spawnerMob(JR& r) { // func_531_b
    int i = r.nextInt(4);
    if (i == 0) return "Skeleton";
    if (i == 1 || i == 2) return "Zombie";
    return i == 3 ? "Spider" : "";
}

Stack dungeonLoot(JR& r) { // WorldGenDungeons.chestLoot
    int i = r.nextInt(11);
    if (i == 0) return {329, 1, 0};                                 // saddle
    if (i == 1) return {265, r.nextInt(4) + 1, 0};                  // iron
    if (i == 2) return {297, 1, 0};                                 // bread
    if (i == 3) return {296, r.nextInt(4) + 1, 0};                  // wheat
    if (i == 4) return {289, r.nextInt(4) + 1, 0};                  // gunpowder
    if (i == 5) return {287, r.nextInt(4) + 1, 0};                  // string
    if (i == 6) return {325, 1, 0};                                 // bucket
    if (i == 7 && r.nextInt(100) == 0) return {322, 1, 0};          // golden apple
    if (i == 8 && r.nextInt(2) == 0) return {331, r.nextInt(4) + 1, 0}; // redstone
    if (i == 9 && r.nextInt(10) == 0) return {ITEM_RECORD13 + r.nextInt(13), 1, 0};
    return i == 10 ? Stack{364, 3, 0} : Stack{0, 0, 0};             // cocoa
}

Stack waterLoot(JR& r) { // WorldGenWaterDungeons.chestLoot
    int i = r.nextInt(11);
    if (i == 0) return {SPONGE, r.nextInt(4) + 1, 0};
    if (i == 1 && r.nextInt(10) == 0) return {266, r.nextInt(4) + 1, 0}; // gold
    if (i == 2) return {339, r.nextInt(4) + 1, 0};                  // paper
    if (i == 3) return {338, r.nextInt(4) + 1, 0};                  // reeds
    if (i == 4) return {318, r.nextInt(4) + 1, 0};                  // flint
    if (i == 5) {
        int roll = r.nextInt(4);
        return {302 + roll, 1, 0};                                  // chain helmet/plate/legs/boots
    }
    if (i == 6) return {326, 1, 0};                                 // water bucket
    if (i == 7 && r.nextInt(100) == 0) return {322, 1, 0};
    if (i == 8 && r.nextInt(2) == 0) return {331, r.nextInt(4) + 1, 0};
    return i == 9 && r.nextInt(1000) == 0 ? Stack{264, 1, 0} : Stack{0, 0, 0}; // diamond
}

void fillChest(World& w, JR& r, int x, int y, int z, bool water) {
    for (int k = 0; k < 8; ++k) {
        Stack s = water ? waterLoot(r) : dungeonLoot(r);
        if (s.id != 0) putInChest(w, x, y, z, r.nextInt(27), s);
    }
}

bool dungeon(World& w, JR& r, int i, int j, int k) { // WorldGenDungeons
    const int H = 3;
    int l = r.nextInt(2) + 2, i1 = r.nextInt(2) + 2, spots = 0;
    for (int a = i - l - 1; a <= i + l + 1; ++a)
        for (int b = j - 1; b <= j + H + 1; ++b)
            for (int c = k - i1 - 1; c <= k + i1 + 1; ++c) {
                Mat m = w.materialAt(a, b, c);
                if (b == j - 1 && !matSolid(m)) return false;
                if (b == j + H + 1 && !matSolid(m)) return false;
                if ((a == i - l - 1 || a == i + l + 1 || c == k - i1 - 1 || c == k + i1 + 1) && b == j &&
                    w.blockAt(a, b, c) == 0 && w.blockAt(a, b + 1, c) == 0)
                    ++spots;
            }
    if (!(spots >= 1 && spots <= 5)) return false;
    for (int a = i - l - 1; a <= i + l + 1; ++a)
        for (int b = j + H; b >= j - 1; --b)
            for (int c = k - i1 - 1; c <= k + i1 + 1; ++c) {
                if (a != i - l - 1 && b != j - 1 && c != k - i1 - 1 && a != i + l + 1 && b != j + H + 1 && c != k + i1 + 1)
                    w.setBlockWithNotify(a, b, c, 0);
                else if (b >= 0 && !matSolid(w.materialAt(a, b - 1, c)))
                    w.setBlockWithNotify(a, b, c, 0);
                else if (matSolid(w.materialAt(a, b, c))) {
                    if (b == j - 1 && r.nextInt(4) != 0) w.setBlockWithNotify(a, b, c, MOSSY_COBBLE);
                    else w.setBlockWithNotify(a, b, c, COBBLE);
                }
            }
    for (int i2 = 0; i2 < 2; ++i2)
        for (int l2 = 0; l2 < 3; ++l2) {
            int k3 = i + r.nextInt(l * 2 + 1) - l;
            int i4 = k + r.nextInt(i1 * 2 + 1) - i1;
            if (w.blockAt(k3, j, i4) != 0) continue;
            int n = 0;
            if (matSolid(w.materialAt(k3 - 1, j, i4))) ++n;
            if (matSolid(w.materialAt(k3 + 1, j, i4))) ++n;
            if (matSolid(w.materialAt(k3, j, i4 - 1))) ++n;
            if (matSolid(w.materialAt(k3, j, i4 + 1))) ++n;
            if (n == 1) {
                w.setBlockWithNotify(k3, j, i4, CRATE_ID);
                fillChest(w, r, k3, j, i4, false);
                break;
            }
        }
    w.setBlockWithNotify(i, j, k, SPAWNER);
    const char* mob = spawnerMob(r);
    if (nbt::Tag* te = w.tileEntityAt(i, j, k)) te->setString("EntityId", mob);
    return true;
}

bool waterDungeon(World& w, JR& r, int x, int y, int z) { // WorldGenWaterDungeons
    const int H = 3;
    int rx = r.nextInt(2) + 2, rz = r.nextInt(2) + 2, spots = 0;
    for (int a = x - rx - 1; a <= x + rx + 1; ++a)
        for (int b = y - 1; b <= y + H + 1; ++b)
            for (int c = z - rz - 1; c <= z + rz + 1; ++c)
                if ((a == x - rx - 1 || a == x + rx + 1 || c == z - rz - 1 || c == z + rz + 1) && b == y &&
                    w.blockAt(a, b, c) != 9 && w.blockAt(a, b + 1, c) == 9 && w.blockAt(a, b + 2, c) == 9 &&
                    w.blockAt(a, b + 3, c) == 9 && w.blockAt(a, b + 4, c) == 9 && w.blockAt(a, b + 5, c) == 9 &&
                    w.blockAt(a, b + 6, c) == 9)
                    ++spots;
    if (!(spots >= 1 && spots <= 50 && r.nextInt(100) <= 25)) return false;
    auto brick = [&](int a, int b, int c) { w.setBlockWithNotify(a, b, c, r.nextInt(4) < 2 ? BRICK_MOSSY : BRICK); };
    for (int a = x - rx - 1; a <= x + rx + 1; ++a)
        for (int b = y + H; b >= y - 1; --b)
            for (int c = z - rz - 1; c <= z + rz + 1; ++c) {
                if (a != x - rx - 1 && b != y - 1 && c != z - rz - 1 && a != x + rx + 1 && b != y + H + 1 && c != z + rz + 1) {
                    if (w.blockAt(a, b, c) != 0 && b < 64) {
                        w.setBlockWithNotify(a, b, c, 0);
                        brick(a, y + H, c);
                    }
                } else if (b >= 0 && !matSolid(w.materialAt(a, b - 1, c))) {
                    if (w.blockAt(a, b, c) != 0 && b < 64) brick(a, b, c);
                } else if (w.materialAt(a, b, c) == Mat::water && b == y - 1) {
                    brick(a, b, c);
                }
            }
    for (int i2 = 0; i2 < 2; ++i2)
        for (int l2 = 0; l2 < 3; ++l2) {
            int k3 = x + r.nextInt(rx * 2 + 1) - rx;
            int i4 = z + r.nextInt(rz * 2 + 1) - rz;
            if (w.blockAt(k3, y, i4) == 9) continue;
            int n = 0;
            if (matSolid(w.materialAt(k3 - 1, y, i4))) ++n;
            if (matSolid(w.materialAt(k3 + 1, y, i4))) ++n;
            if (matSolid(w.materialAt(k3, y, i4 - 1))) ++n;
            if (matSolid(w.materialAt(k3, y, i4 + 1))) ++n;
            if (n == 1) {
                w.setBlockWithNotify(k3, y, i4, CRATE_ID);
                fillChest(w, r, k3, y, i4, true);
                break;
            }
        }
    w.setBlockWithNotify(x, y, z, SPAWNER);
    w.setBlockWithNotify(x, y - 1, z, SPONGE);
    w.notifyBlocksOfNeighborChange(x, y - 1, z, 1);
    const char* mob = spawnerMob(r);
    if (nbt::Tag* te = w.tileEntityAt(x, y, z)) te->setString("EntityId", mob);
    return true;
}

bool cobweb(World& w, int x, int y, int z) { // WorldGenCobWebs
    if (w.blockAt(x, y, z) == 0 && w.blockAt(x, y + 1, z) == STONE) {
        w.setBlockWithNotify(x, y, z, WEB);
        return true;
    }
    return false;
}

bool oakTree(World& w, JR& r, int i, int j, int k) { // WorldGenTrees
    int l = r.nextInt(3) + 4;
    bool flag = true;
    if (!(j >= 1 && j + l + 1 <= 128)) return false;
    for (int i1 = j; i1 <= j + 1 + l; ++i1) {
        int b = 1;
        if (i1 == j) b = 0;
        if (i1 >= j + 1 + l - 2) b = 2;
        for (int i2 = i - b; i2 <= i + b && flag; ++i2)
            for (int l2 = k - b; l2 <= k + b && flag; ++l2) {
                if (i1 >= 0 && i1 < 128) {
                    int id = w.blockAt(i2, i1, l2);
                    if (id != 0 && id != LEAVES) flag = false;
                } else flag = false;
            }
    }
    if (!flag) return false;
    int below = w.blockAt(i, j - 1, k);
    if (!((below == GRASS || below == DIRT_ID) && j < 128 - l - 1)) return false;
    w.setBlock(i, j - 1, k, DIRT_ID);
    for (int k1 = j - 3 + l; k1 <= j + l; ++k1) {
        int j2 = k1 - (j + l);
        int i3 = 1 - j2 / 2;
        for (int k3 = i - i3; k3 <= i + i3; ++k3) {
            int l3 = k3 - i;
            for (int i4 = k - i3; i4 <= k + i3; ++i4) {
                int j4 = i4 - k;
                bool place = std::abs(l3) != i3 || std::abs(j4) != i3;
                if (!place) place = r.nextInt(2) != 0 && j2 != 0; // Java: a || (rand && j2 != 0), rand only if !a
                if (place && !BP[w.blockAt(k3, k1, i4)].opaqueLookup) w.setBlock(k3, k1, i4, LEAVES);
            }
        }
    }
    for (int l1 = 0; l1 < l; ++l1) {
        int id = w.blockAt(i, j + l1, k);
        if (id == 0 || id == LEAVES) w.setBlock(i, j + l1, k, LOG);
    }
    return true;
}

void flowers(World& w, JR& r, int id, int i, int j, int k) { // WorldGenFlowers
    for (int l = 0; l < 64; ++l) {
        int a = r.nextInt(8); int i1 = i + a - r.nextInt(8);
        a = r.nextInt(4); int j1 = j + a - r.nextInt(4);
        a = r.nextInt(8); int k1 = k + a - r.nextInt(8);
        if (w.blockAt(i1, j1, k1) == 0 && Blocks::canBlockStay(w, id, i1, j1, k1)) w.setBlock(i1, j1, k1, id);
    }
}

void reeds(World& w, JR& r, int i, int j, int k) { // WorldGenReed
    for (int l = 0; l < 20; ++l) {
        int a = r.nextInt(4); int i1 = i + a - r.nextInt(4);
        a = r.nextInt(4); int k1 = k + a - r.nextInt(4);
        if (w.blockAt(i1, j, k1) == 0 &&
            (w.materialAt(i1 - 1, j - 1, k1) == Mat::water || w.materialAt(i1 + 1, j - 1, k1) == Mat::water ||
             w.materialAt(i1, j - 1, k1 - 1) == Mat::water || w.materialAt(i1, j - 1, k1 + 1) == Mat::water)) {
            int n = 2 + r.nextInt(r.nextInt(3) + 1);
            for (int i2 = 0; i2 < n; ++i2)
                if (Blocks::canBlockStay(w, REED, i1, j + i2, k1)) w.setBlock(i1, j + i2, k1, REED);
        }
    }
}

void cactus(World& w, JR& r, int i, int j, int k) { // WorldGenCactus
    for (int l = 0; l < 10; ++l) {
        int a = r.nextInt(8); int i1 = i + a - r.nextInt(8);
        a = r.nextInt(4); int j1 = j + a - r.nextInt(4);
        a = r.nextInt(8); int k1 = k + a - r.nextInt(8);
        if (w.blockAt(i1, j1, k1) == 0) {
            int n = 1 + r.nextInt(r.nextInt(3) + 1);
            for (int i2 = 0; i2 < n; ++i2)
                if (Blocks::canBlockStay(w, CACTUS, i1, j1 + i2, k1)) w.setBlock(i1, j1 + i2, k1, CACTUS);
        }
    }
}

void liquids(World& w, int id, int i, int j, int k) { // WorldGenLiquids: Alpha only places the source
    if (w.blockAt(i, j + 1, k) != STONE) return;
    if (w.blockAt(i, j - 1, k) != STONE) return;
    if (w.blockAt(i, j, k) != 0 && w.blockAt(i, j, k) != STONE) return;
    int l = 0, i1 = 0;
    if (w.blockAt(i - 1, j, k) == STONE) ++l;
    if (w.blockAt(i + 1, j, k) == STONE) ++l;
    if (w.blockAt(i, j, k - 1) == STONE) ++l;
    if (w.blockAt(i, j, k + 1) == STONE) ++l;
    if (w.blockAt(i - 1, j, k) == 0) ++i1;
    if (w.blockAt(i + 1, j, k) == 0) ++i1;
    if (w.blockAt(i, j, k - 1) == 0) ++i1;
    if (w.blockAt(i, j, k + 1) == 0) ++i1;
    if (l == 3 && i1 == 1) w.setBlockWithNotify(i, j, k, id);
}
} // namespace nsss

// NSSS BlockSapling: a new WorldGenTrees, or one time in ten a new WorldGenBigTree with its defaults
bool nsssSaplingTree(World& w, JavaRandom& r, int x, int y, int z) {
    if (r.nextInt(10) == 0) {
        BigTree b;
        b.fromSapling = true;
        return b.generate(w, r, x, y, z);
    }
    return nsss::oakTree(w, r, x, y, z);
}

void ChunkGenerator::alphaPopulate(World& w, int i, int j) { // ChunkProviderGenerate.generateStructures
    using namespace nsss;
    Blocks::sandFallInstantly = true;
    JR& r = alphaTerrain->genRandom;
    int tryX = i * 16, tryZ = j * 16;
    r.setSeed(seed);
    int64_t l1 = r.nextLong() / 2 * 2 + 1;
    int64_t l2 = r.nextLong() / 2 * 2 + 1;
    r.setSeed(add(mul(i, l1), mul(j, l2)) ^ seed);
    int x, y, z;
    auto at8 = [&](int range) { x = tryX + r.nextInt(16) + 8; y = r.nextInt(range); z = tryZ + r.nextInt(16) + 8; };
    auto at0 = [&](int range) { x = tryX + r.nextInt(16); y = r.nextInt(range); z = tryZ + r.nextInt(16); };
    for (int n = 0; n < 8; ++n) { at8(128); nsss::dungeon(w, r, x, y, z); }
    if ((tryX > 3000 || tryX < -3000 || tryZ > 3000 || tryZ < -3000) && r.nextInt(10000) == 2) {
        at8(128);
        // aaa.generate: the shell (random picks mossy bricks one time in four), then the pyramidion
        for (int py = 95; py > 0; --py)
            for (int px = x - py; px < x + py - 1; ++px)
                for (int pz = z - py; pz < z + py - 1; ++pz)
                    if (!BP[w.blockAt(px, 128 - py, pz)].opaqueCube &&
                        (px == x - py || px == x + py - 2 || pz == z - py || pz == z + py - 2))
                        w.setBlock(px, 128 - py, pz, r.nextInt(4) != 0 ? BRICK : BRICK_MOSSY);
        w.setBlock(x - 1, 127, z - 1, PYRAMIDION);
    }
    for (int n = 0; n < 8; ++n) { at8(512); nsss::waterDungeon(w, r, x, y, z); }
    for (int n = 0; n < 128; ++n) { at8(128); nsss::cobweb(w, x, y, z); }
    for (int n = 0; n < 10; ++n) { at0(64); nsss::clayLike(w, r, 8, x, y, z, Mat::sand, CLAY_ID, false); }
    for (int n = 0; n < 10; ++n) { at0(64); nsss::clayLike(w, r, 4, x, y, z, Mat::ground, CLAY_RED, true); }
    for (int n = 0; n < 20; ++n) { at0(128); nsss::minable(w, r, DIRT_ID, 32, x, y, z); }
    for (int n = 0; n < 10; ++n) { at0(128); nsss::minable(w, r, GRAVEL_ID, 32, x, y, z); }
    for (int n = 0; n < 20; ++n) { at0(128); nsss::minable(w, r, COAL_ORE, 16, x, y, z); }
    for (int n = 0; n < 20; ++n) { at0(64); nsss::minable(w, r, IRON_ORE, 8, x, y, z); }
    for (int n = 0; n < 2; ++n) { at0(32); nsss::minable(w, r, GOLD_ORE, 8, x, y, z); }
    for (int n = 0; n < 6; ++n) { at0(32); nsss::minable(w, r, COPPER_ORE, 8, x, y, z); }
    for (int n = 0; n < 8; ++n) { at0(16); nsss::minable(w, r, REDSTONE_ORE, 7, x, y, z); }
    for (int n = 0; n < 1; ++n) { at0(16); nsss::minable(w, r, DIAMOND_ORE, 7, x, y, z); }
    double d = 0.5;
    int trees = int((alphaTerrain->treeNoise.getVal(tryX * d, tryZ * d) / 8.0 + r.nextDouble() * 4.0 + 4.0) / 3.0);
    if (trees < 0) trees = 0;
    if (r.nextInt(10) == 0) ++trees;
    bool big = r.nextInt(10) == 0; // one generator object for every tree in the chunk
    BigTree bigTree;
    for (int n = 0; n < trees; ++n) {
        int tx = tryX + r.nextInt(16) + 8;
        int tz = tryZ + r.nextInt(16) + 8;
        if (big) bigTree.generate(w, r, tx, w.getHeightValue(tx, tz), tz);
        else nsss::oakTree(w, r, tx, w.getHeightValue(tx, tz), tz);
    }
    for (int n = 0; n < 2; ++n) { at8(128); nsss::flowers(w, r, FLOWER_Y, x, y, z); }
    if (r.nextInt(2) == 0) { at8(128); nsss::flowers(w, r, FLOWER_R, x, y, z); }
    if (r.nextInt(1) == 0) { at8(128); nsss::flowers(w, r, BLUE_ROSE, x, y, z); }
    if (r.nextInt(4) == 0) { at8(128); nsss::flowers(w, r, MUSH_B, x, y, z); }
    if (r.nextInt(8) == 0) { at8(128); nsss::flowers(w, r, MUSH_R, x, y, z); }
    for (int n = 0; n < 10; ++n) { at8(128); nsss::reeds(w, r, x, y, z); }
    for (int n = 0; n < 1; ++n) { at8(128); nsss::cactus(w, r, x, y, z); }
    for (int n = 0; n < 50; ++n) {
        x = tryX + r.nextInt(16) + 8;
        y = r.nextInt(r.nextInt(120) + 8);
        z = tryZ + r.nextInt(16) + 8;
        nsss::liquids(w, WATER, x, y, z);
    }
    for (int n = 0; n < 20; ++n) {
        x = tryX + r.nextInt(16) + 8;
        y = r.nextInt(r.nextInt(r.nextInt(112) + 8) + 8);
        z = tryZ + r.nextInt(16) + 8;
        nsss::liquids(w, LAVA, x, y, z);
    }
    // the snow layer only falls in winter worlds (snowCovered / winter after the first snow)
    Blocks::sandFallInstantly = false;
}


void ChunkGenerator::populate(World& w, int cx, int cz) { alphaPopulate(w, cx, cz); }
