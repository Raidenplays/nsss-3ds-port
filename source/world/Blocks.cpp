// Block behaviour, as the NSSS jar's Block classes have it: onBlockAdded / onBlockRemoval /
// onNeighborBlockChange / updateTick / canPlace / canBlockStay, ported as one dispatcher (redstone
// lives in Redstone.cpp, rails in Rails.cpp). Blocks with nothing of their own act like the jar's
// plain Block (all no-ops). tools/nsss/sims runs scripts of block changes on both, byte for byte.
#include "Blocks.h"
#include "gen/ChunkGenerator.h"
#include "Rails.h"
#include "BlockProps.h"
#include "World.h"
#include "Redstone.h"
#include "game/Survival.h"

namespace Blocks {
bool sandFallInstantly = false;
void (*onTntIgnited)(int x, int y, int z) = nullptr;
void (*onPyramidionBroken)(int x, int y, int z) = nullptr;

namespace {
// block ids (waterMoving = 8 flowing, waterStill = 9, the same for lava)
enum : int {
    STONE = 1, GRASS = 2, DIRT = 3, COBBLE = 4, SAPLING = 6, WATER = 8, WATER_STILL = 9, LAVA = 10,
    LAVA_STILL = 11, SAND = 12, GRAVEL = 13, LOG = 17, LEAVES = 18, OBSIDIAN = 49, FIRE = 51, SIGN = 63,
    LADDER = 65, TILLED = 60, SNOW = 78, ICE = 79, CACTUS = 81, REED = 83, BRICK_MOSSY = 93,
};

inline bool isDoor(int id) { return id == 64 || id == 71; }
bool normalCube(World& w, int x, int y, int z) { return w.isBlockOpaqueCube(x, y, z); } // World.isBlockNormalCube
// the season, when the world's seasons turn (World.season == cycle) - -1 when they don't
int cycleSeason(World& w) { return w.season == World::CYCLE ? w.currentSeason() : -1; }

// ---------------------------------------------------------------- liquids (BlockFluids)

bool isLiquidBlock(int id) { return id >= WATER && id <= LAVA_STILL; }
Mat liquidMat(int id) { return id <= WATER_STILL ? Mat::water : Mat::lava; }

int flowDecay(World& w, Mat m, int x, int y, int z) { // func_290_h
    return w.materialAt(x, y, z) != m ? -1 : w.metaAt(x, y, z);
}

void checkForHarden(World& w, int x, int y, int z, int id) {
    if (w.blockAt(x, y, z) != id) return;
    if (liquidMat(id) != Mat::lava) return;
    bool water = w.materialAt(x, y, z - 1) == Mat::water || w.materialAt(x, y, z + 1) == Mat::water ||
                 w.materialAt(x - 1, y, z) == Mat::water || w.materialAt(x + 1, y, z) == Mat::water ||
                 w.materialAt(x, y + 1, z) == Mat::water;
    if (!water) return;
    int m = w.metaAt(x, y, z);
    if (m == 0) {
        w.setBlockWithNotify(x, y, z, OBSIDIAN);
        if (std::rand() % 500 == 1) w.setBlockWithNotify(x, y, z, 94); // NSSS: new Random().nextInt(500): bleeding obsidian
    } else if (m <= 4) {
        w.setBlockWithNotify(x, y, z, COBBLE);
    }
    // func_292_i: fizz sound + smoke (uses world.rand)
    w.rand.nextFloat();
    w.rand.nextFloat();
}

// BlockFlowing instance state (one per block type in Java)
struct FlowState {
    int sources = 0;     // field_460_a
    bool dirs[4];        // field_459_b
    int cost[4];         // field_461_c
};
FlowState flowState[2]; // water, lava

bool nearSponge(World& w, int x, int y, int z); // BlockSponge.getSurroundSponge

bool blocksFlow(World& w, int x, int y, int z) { // NSSS BlockFlowing.canFlowInto (named backwards: true = blocked)
    int id = w.blockAt(x, y, z);
    if (isDoor(id) || nearSponge(w, x, y, z) || id == SIGN || id == LADDER || id == REED) return true;
    if (id == 0) return false;
    return matSolid(BP[id].mat);
}

bool canFlowInto(World& w, Mat m, int x, int y, int z) { // func_298_m
    Mat t = w.materialAt(x, y, z);
    return t == m ? false : (t == Mat::lava ? false : !blocksFlow(w, x, y, z));
}

int smallestDecay(World& w, Mat m, FlowState& s, int x, int y, int z, int cur) { // func_296_f
    int d = flowDecay(w, m, x, y, z);
    if (d < 0) return cur;
    if (d == 0) ++s.sources;
    if (d >= 8) d = 0;
    return cur >= 0 && d >= cur ? cur : d;
}

int flowCost(World& w, Mat m, int x, int y, int z, int dist, int from) { // func_300_a
    int best = 1000;
    for (int i = 0; i < 4; ++i) {
        if ((i == 0 && from == 1) || (i == 1 && from == 0) || (i == 2 && from == 3) || (i == 3 && from == 2)) continue;
        int nx = x, nz = z;
        if (i == 0) nx = x - 1;
        if (i == 1) ++nx;
        if (i == 2) nz = z - 1;
        if (i == 3) ++nz;
        if (!blocksFlow(w, nx, y, nz) && (w.materialAt(nx, y, nz) != m || w.metaAt(nx, y, nz) != 0)) {
            if (!blocksFlow(w, nx, y - 1, nz)) return dist;
            if (dist < 4) {
                int c = flowCost(w, m, nx, y, nz, dist + 1, i);
                if (c < best) best = c;
            }
        }
    }
    return best;
}

bool* flowDirections(World& w, Mat m, FlowState& s, int x, int y, int z) { // func_297_k
    for (int i = 0; i < 4; ++i) {
        s.cost[i] = 1000;
        int nx = x, nz = z;
        if (i == 0) nx = x - 1;
        if (i == 1) ++nx;
        if (i == 2) nz = z - 1;
        if (i == 3) ++nz;
        if (!blocksFlow(w, nx, y, nz) && (w.materialAt(nx, y, nz) != m || w.metaAt(nx, y, nz) != 0)) {
            if (!blocksFlow(w, nx, y - 1, nz)) s.cost[i] = 0;
            else s.cost[i] = flowCost(w, m, nx, y, nz, 1, i);
        }
    }
    int best = s.cost[0];
    for (int i = 1; i < 4; ++i)
        if (s.cost[i] < best) best = s.cost[i];
    for (int i = 0; i < 4; ++i) s.dirs[i] = s.cost[i] == best;
    return s.dirs;
}

void flowInto(World& w, int id, Mat m, int x, int y, int z, int decay) { // func_299_g
    if (!canFlowInto(w, m, x, y, z)) return;
    int old = w.blockAt(x, y, z);
    if (old > 0) {
        if (m == Mat::lava) { w.rand.nextFloat(); w.rand.nextFloat(); } // fizz
        else dropBlockAsItem(w, x, y, z, old, w.metaAt(x, y, z));
    }
    w.setBlockAndMetadataWithNotify(x, y, z, id, decay);
}

void makeStill(World& w, int x, int y, int z, int id) { // func_20015_j
    int m = w.metaAt(x, y, z);
    w.setBlockAndMetadataJ(x, y, z, id + 1, m);
    w.markBlocksDirty(x, y, z, x, y, z);
    w.markBlockNeedsUpdate(x, y, z);
}

void flowingTick(World& w, int x, int y, int z, int id, JavaRandom& r) {
    Mat m = liquidMat(id);
    FlowState& s = flowState[m == Mat::lava ? 1 : 0];
    int decay = flowDecay(w, m, x, y, z);
    int step = m == Mat::lava ? 2 : 1; // fluidType
    bool still = true;
    int next;
    if (decay > 0) {
        s.sources = 0;
        int sm = smallestDecay(w, m, s, x - 1, y, z, -100);
        sm = smallestDecay(w, m, s, x + 1, y, z, sm);
        sm = smallestDecay(w, m, s, x, y, z - 1, sm);
        sm = smallestDecay(w, m, s, x, y, z + 1, sm);
        next = sm + step;
        if (next >= 8 || sm < 0) next = -1;
        if (flowDecay(w, m, x, y + 1, z) >= 0) {
            int above = flowDecay(w, m, x, y + 1, z);
            next = above >= 8 ? above : above + 8;
        }
        if (s.sources >= 2 && m == Mat::water) { // NSSS: also over water, never by a sponge
            if (w.isBlockOpaqueCube(x, y - 1, z) || w.materialAt(x, y - 1, z) == Mat::water) next = 0;
            else if (w.materialAt(x, y - 1, z) == m && w.metaAt(x, y, z) == 0 && !nearSponge(w, x, y, z)) next = 0;
        }
        if (m == Mat::lava && decay < 8 && next < 8 && next > decay && r.nextInt(4) != 0) {
            next = decay;
            still = false;
        }
        if (next != decay) {
            decay = next;
            if (next < 0) {
                w.setBlockWithNotify(x, y, z, 0);
            } else {
                w.setBlockMetadataWithNotify(x, y, z, next);
                w.scheduleBlockUpdate(x, y, z, id);
                w.notifyBlocksOfNeighborChange(x, y, z, id);
            }
        } else if (still) {
            makeStill(w, x, y, z, id);
        }
    } else {
        makeStill(w, x, y, z, id);
    }
    if (canFlowInto(w, m, x, y - 1, z)) {
        if (decay >= 8) w.setBlockAndMetadataWithNotify(x, y - 1, z, id, decay);
        else w.setBlockAndMetadataWithNotify(x, y - 1, z, id, decay + 8);
    } else if (decay >= 0 && (decay == 0 || blocksFlow(w, x, y - 1, z))) {
        bool* dirs = flowDirections(w, m, s, x, y, z);
        next = decay + step;
        if (decay >= 8) next = 1;
        if (next >= 8) return;
        if (dirs[0]) flowInto(w, id, m, x - 1, y, z, next);
        if (dirs[1]) flowInto(w, id, m, x + 1, y, z, next);
        if (dirs[2]) flowInto(w, id, m, x, y, z - 1, next);
        if (dirs[3]) flowInto(w, id, m, x, y, z + 1, next);
    }
}

void stationaryToFlowing(World& w, int x, int y, int z, int id) { // func_20016_j
    int m = w.metaAt(x, y, z);
    w.editingBlocks = true;
    w.setBlockAndMetadataJ(x, y, z, id - 1, m);
    w.markBlocksDirty(x, y, z, x, y, z);
    w.scheduleBlockUpdate(x, y, z, id - 1);
    w.editingBlocks = false;
}

bool burns(Mat m) { return m == Mat::wood || m == Mat::leaves || m == Mat::cloth || m == Mat::tnt; }

void stationaryTick(World& w, int x, int y, int z, int id, JavaRandom& r) {
    if (liquidMat(id) != Mat::lava) return;
    int n = r.nextInt(3);
    for (int i = 0; i < n; ++i) {
        x += r.nextInt(3) - 1;
        ++y;
        z += r.nextInt(3) - 1;
        int b = w.blockAt(x, y, z);
        if (b == 0) {
            if (burns(w.materialAt(x - 1, y, z)) || burns(w.materialAt(x + 1, y, z)) || burns(w.materialAt(x, y, z - 1)) ||
                burns(w.materialAt(x, y, z + 1)) || burns(w.materialAt(x, y - 1, z)) || burns(w.materialAt(x, y + 1, z))) {
                w.setBlockWithNotify(x, y, z, FIRE);
                return;
            }
        } else if (matSolid(BP[b].mat)) {
            return;
        }
    }
}

// ---------------------------------------------------------------- sponges (NSSS)

constexpr int SPONGE_ID = 19, WET_SPONGE = 87;

bool nearSponge(World& w, int x, int y, int z) { // BlockSponge.getSurroundSponge: a sponge within 2
    for (int x1 = x - 2; x1 <= x + 2; ++x1)
        for (int y1 = y - 2; y1 <= y + 2; ++y1)
            for (int z1 = z - 2; z1 <= z + 2; ++z1)
                if (w.blockAt(x1, y1, z1) == SPONGE_ID) return true;
    return false;
}

bool redPowerAround(World& w, int i, int j, int k, int l) { // BlockSponge.isReceivingRedPower (same in both)
    using Redstone::isBlockIndirectlyProvidingPowerTo;
    if (l != 0 && isBlockIndirectlyProvidingPowerTo(w, i, j - 1, k, 0)) return true;
    if (l != 1 && isBlockIndirectlyProvidingPowerTo(w, i, j + 1, k, 1)) return true;
    if (l != 2 && isBlockIndirectlyProvidingPowerTo(w, i, j, k - 1, 2)) return true;
    if (l != 3 && isBlockIndirectlyProvidingPowerTo(w, i, j, k + 1, 3)) return true;
    if (l != 5 && isBlockIndirectlyProvidingPowerTo(w, i + 1, j, k, 5)) return true;
    if (l != 4 && isBlockIndirectlyProvidingPowerTo(w, i - 1, j, k, 4)) return true;
    if (isBlockIndirectlyProvidingPowerTo(w, i, j, k, 0)) return true;
    if (isBlockIndirectlyProvidingPowerTo(w, i, j + 2, k, 1)) return true;
    if (isBlockIndirectlyProvidingPowerTo(w, i, j + 1, k - 1, 2)) return true;
    if (isBlockIndirectlyProvidingPowerTo(w, i, j + 1, k + 1, 3)) return true;
    return isBlockIndirectlyProvidingPowerTo(w, i - 1, j + 1, k, 4) || isBlockIndirectlyProvidingPowerTo(w, i + 1, j + 1, k, 5);
}

bool openWater(World& w, int x, int y, int z) { int id = w.blockAt(x, y, z); return id == 0 || id == WATER_STILL; }

void spongeFlowWater(World& w, int x, int y, int z, int sx, int sz) { // BlockSponge.flowWater (dropWater does nothing)
    const int radius = 3;
    if (!(openWater(w, x, y, z) && !nearSponge(w, x, y, z))) return;
    if (x + 1 < sx + radius && openWater(w, x + 1, y, z) && !nearSponge(w, x + 1, y, z)) w.notifyBlocksOfNeighborChange(x + 1, y, z, 1);
    if (z + 1 < sz + radius && openWater(w, x, y, z + 1) && !nearSponge(w, x, y, z + 1)) w.notifyBlocksOfNeighborChange(x, y, z + 1, 1);
    if (x - 1 > sx - radius && openWater(w, x - 1, y, z) && !nearSponge(w, x - 1, y, z)) w.notifyBlocksOfNeighborChange(x - 1, y, z, 1);
    if (z - 1 > sz - radius && openWater(w, x, y, z - 1) && !nearSponge(w, x, y, z - 1))
        w.notifyBlocksOfNeighborChange(x, y, z + 1, 1); // (z + 1 in the jar)
}

void spongeDestroyed(World& w, int a, int b, int c) { // BlockSponge.onBlockDestroyedByPlayer
    const int radius = 3;
    for (int x = a - radius; x <= a + radius; ++x)
        for (int y = b + radius; y > b - radius; --y)
            for (int z = c - radius; z <= c + radius; ++z) {
                int id = w.blockAt(x, y, z);
                if (id == WATER || id == WATER_STILL) spongeFlowWater(w, x, y, z, a, c);
            }
}

void spongeAdded(World& w, int a, int b, int c) { // BlockSponge.onBlockAdded: soak up water within 2
    for (int x = a - 2; x <= a + 2; ++x)
        for (int y = b - 2; y <= b + 2; ++y)
            for (int z = c - 2; z <= c + 2; ++z) {
                int id = w.blockAt(x, y, z);
                if (id == WATER || id == WATER_STILL) w.setBlockWithNotify(x, y, z, 0);
            }
}

void wetSpongeAdded(World& w, int a, int b, int c) { // BlockSpongeInactive.onBlockAdded: the water within 3 flows again
    for (int x = a - 3; x <= a + 3; ++x)
        for (int y = b - 3; y <= b + 3; ++y)
            for (int z = c - 3; z <= c + 3; ++z) {
                int id = w.blockAt(x, y, z);
                if (id == WATER || id == WATER_STILL) w.setBlockAndMetadataWithNotify(x, y, z, WATER, w.metaAt(x, y, z));
            }
}

// ---------------------------------------------------------------- gears, motors, light bulbs (NSSS)
// NSSS's gear network never powers anything: World.applyGearPower only looks at gear groups with
// `i < gearGroups.size() - 1`, and the groups never form. So a motor keeps its tile entity and
// nothing else happens, exactly like the jar. A gear needs a solid cube beside it, checked when a
// neighbour changes (BlockGear.func_271_h), not when it's placed.

bool gearSupported(World& w, int i, int j, int k) { // BlockGear.canPlace
    return w.isBlockOpaqueCube(i - 1, j, k) || w.isBlockOpaqueCube(i + 1, j, k) || w.isBlockOpaqueCube(i, j, k - 1) ||
           w.isBlockOpaqueCube(i, j, k + 1);
}

void gearNeighbourChange(World& w, int i, int j, int k, int id) {
    if (!gearSupported(w, i, j, k)) {
        dropBlockAsItem(w, i, j, k, id, w.metaAt(i, j, k));
        w.setBlockWithNotify(i, j, k, 0);
    }
}

void lightBulbNeighbourChange(World& w, int i, int j, int k, int neighbour) { // BlockLightBulb (setBlock, no notify)
    w.setBlock(i, j, k, redPowerAround(w, i, j, k, neighbour) ? 98 : 97);
}

// ---------------------------------------------------------------- attached blocks
// Torches turn to face a solid neighbour whenever they're added, and can also hang off NSSS's
// sideways slabs (stepSingle with metadata 1..5).

constexpr int STEP_SINGLE = 44, FENCE_ID = 85, GLASS_ID = 20;
bool slabFacing(World& w, int x, int y, int z, int m) { return w.blockAt(x, y, z) == STEP_SINGLE && w.metaAt(x, y, z) == m; }

bool torchCanPlace(World& w, int x, int y, int z) { // BlockTorch.canPlace
    int below = w.blockAt(x, y - 1, z);
    if (below == FENCE_ID || below == GLASS_ID) return true;
    if (normalCube(w, x - 1, y, z) || slabFacing(w, x - 1, y, z, 4)) return true;
    if (normalCube(w, x + 1, y, z) || slabFacing(w, x + 1, y, z, 5)) return true;
    if (normalCube(w, x, y, z - 1) || slabFacing(w, x, y, z - 1, 2)) return true;
    if (normalCube(w, x, y, z + 1) || slabFacing(w, x, y, z + 1, 3)) return true;
    return normalCube(w, x, y - 1, z) || slabFacing(w, x, y - 1, z, 1);
}

void torchAddedNsss(World& w, int i, int j, int k, int id) { // BlockTorch.onBlockAdded
    if (normalCube(w, i - 1, j, k)) w.setBlockMetadataWithNotify(i, j, k, 1);
    else if (normalCube(w, i + 1, j, k)) w.setBlockMetadataWithNotify(i, j, k, 2);
    else if (normalCube(w, i, j, k - 1)) w.setBlockMetadataWithNotify(i, j, k, 3);
    else if (normalCube(w, i, j, k + 1)) w.setBlockMetadataWithNotify(i, j, k, 4);
    else if (normalCube(w, i, j - 1, k)) w.setBlockMetadataWithNotify(i, j, k, 5);
    if (!torchCanPlace(w, i, j, k)) { // isInLegalPosition
        dropBlockAsItem(w, i, j, k, id, w.metaAt(i, j, k));
        w.setBlockWithNotify(i, j, k, 0);
    }
}

void torchNeighbourChange(World& w, int x, int y, int z, int id) { // BlockTorch.onNeighborBlockChange
    if (!torchCanPlace(w, x, y, z)) {
        dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
        w.setBlockWithNotify(x, y, z, 0);
        return;
    }
    int m = w.metaAt(x, y, z);
    bool drop = false;
    if (!normalCube(w, x - 1, y, z) && !slabFacing(w, x - 1, y, z, 4) && m == 1) drop = true;
    if (!normalCube(w, x + 1, y, z) && !slabFacing(w, x + 1, y, z, 5) && m == 2) drop = true;
    if (!normalCube(w, x, y, z - 1) && !slabFacing(w, x, y, z - 1, 2) && m == 3) drop = true;
    if (!normalCube(w, x, y, z + 1) && !slabFacing(w, x, y, z + 1, 3) && m == 4) drop = true;
    if (!normalCube(w, x, y - 1, z) && !slabFacing(w, x, y - 1, z, 1) && m == 5 && w.blockAt(x, y - 1, z) != FENCE_ID) drop = true;
    if (drop) {
        dropBlockAsItem(w, x, y, z, id, m);
        w.setBlockWithNotify(x, y, z, 0);
    }
}

void ladderNeighbourChange(World& w, int i, int j, int k, int id) { // BlockLadder.onNeighborBlockChange
    int m = w.metaAt(i, j, k);
    bool ok = (m == 2 && normalCube(w, i, j, k + 1)) || (m == 3 && normalCube(w, i, j, k - 1)) ||
              (m == 4 && normalCube(w, i + 1, j, k)) || (m == 5 && normalCube(w, i - 1, j, k));
    if (!ok) {
        dropBlockAsItem(w, i, j, k, id, m);
        w.setBlockWithNotify(i, j, k, 0);
    }
}

void signNeighbourChange(World& w, int i, int j, int k, int id) { // BlockSign.onNeighborBlockChange
    bool drop;
    if (id == SIGN) { // the sign post: on something solid
        drop = !matSolid(w.materialAt(i, j - 1, k));
    } else {
        int m = w.metaAt(i, j, k);
        drop = true;
        if (m == 2 && matSolid(w.materialAt(i, j, k + 1))) drop = false;
        if (m == 3 && matSolid(w.materialAt(i, j, k - 1))) drop = false;
        if (m == 4 && matSolid(w.materialAt(i + 1, j, k))) drop = false;
        if (m == 5 && matSolid(w.materialAt(i - 1, j, k))) drop = false;
    }
    if (drop) {
        dropBlockAsItem(w, i, j, k, id, w.metaAt(i, j, k));
        w.setBlockWithNotify(i, j, k, 0);
    }
}

// ---------------------------------------------------------------- sand / gravel

bool canFallBelow(World& w, int x, int y, int z) {
    int id = w.blockAt(x, y, z);
    if (id == 0 || id == FIRE) return true;
    Mat m = BP[id].mat;
    return m == Mat::water || m == Mat::lava;
}

void tryToFall(World& w, int x, int y, int z, int id) {
    if (!(canFallBelow(w, x, y - 1, z) && y >= 0)) return;
    const int R = 32;
    if (!sandFallInstantly && w.checkChunksExist(x - R, y - R, z - R, x + R, y + R, z + R) && w.onFallingBlock) {
        w.onFallingBlock(x, y, z, id); // EntityFallingSand: it removes the block on its first tick
        return;
    }
    w.setBlockWithNotify(x, y, z, 0);
    while (canFallBelow(w, x, y - 1, z) && y > 0) --y;
    if (y > 0) w.setBlockWithNotify(x, y, z, id);
}

// ---------------------------------------------------------------- plants (BlockFlower and its kin)

bool isPlant(int id) { // BlockFlower: flowers, mushrooms, saplings, crops
    switch (BP[id].cls) {
    case BC::Flower: case BC::Mushroom: case BC::Sapling: case BC::Crops: return true;
    default: return false;
    }
}

bool growsOn(int plant, int below) { // canThisPlantGrowOnThisBlockID
    switch (BP[plant].cls) {
    case BC::Mushroom: return BP[below].opaqueLookup; // Block.opaqueCubeLookup
    case BC::Crops: return below == TILLED;
    default: return below == GRASS || below == DIRT || below == TILLED;
    }
}

bool plantStay(World& w, int id, int x, int y, int z) { // canBlockStay
    if (BP[id].cls == BC::Mushroom) return w.getBlockLightValue(x, y, z) <= 13 && growsOn(id, w.blockAt(x, y - 1, z));
    return (w.getBlockLightValue(x, y, z) >= 8 || w.canBlockSeeTheSky(x, y, z)) && growsOn(id, w.blockAt(x, y - 1, z));
}

void checkPlant(World& w, int x, int y, int z, int id) { // func_268_h
    if (!plantStay(w, id, x, y, z)) {
        dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
        w.setBlockWithNotify(x, y, z, 0);
    }
}

// ---------------------------------------------------------------- others

bool cactusStay(World& w, int x, int y, int z) { // BlockCactus.canBlockStay
    if (matSolid(w.materialAt(x - 1, y, z)) || matSolid(w.materialAt(x + 1, y, z)) || matSolid(w.materialAt(x, y, z - 1)) ||
        matSolid(w.materialAt(x, y, z + 1)))
        return false;
    int b = w.blockAt(x, y - 1, z);
    return b == CACTUS || b == SAND;
}

bool reedPlace(World& w, int x, int y, int z) { // BlockReed.canPlace (and canBlockStay)
    int b = w.blockAt(x, y - 1, z);
    if (b == REED) return true;
    if (b != GRASS && b != DIRT) return false;
    auto wet = [&](int a, int c) { Mat m = w.materialAt(a, y - 1, c); return m == Mat::water || m == Mat::ice; };
    return wet(x - 1, z) || wet(x + 1, z) || wet(x, z - 1) || wet(x, z + 1);
}

// BlockBurntCloth.getSurroundHotBlock: hot burnt cloth within two blocks
bool hotNearby(World& w, int x, int y, int z) {
    for (int x1 = x - 2; x1 <= x + 2; ++x1)
        for (int y1 = y - 2; y1 <= y + 2; ++y1)
            for (int z1 = z - 2; z1 <= z + 2; ++z1)
                if (w.blockAt(x1, y1, z1) == 106) return true;
    return false;
}

bool snowPlace(World& w, int x, int y, int z) { // BlockSnow.canPlace: on an opaque block or leaves, not by hot cloth
    int l = w.blockAt(x, y - 1, z);
    if ((l == 0 || !BP[l].opaqueCube || hotNearby(w, x, y, z)) && l != LEAVES) return false;
    return matSolid(w.materialAt(x, y - 1, z)); // blocksMovement
}

// BlockChest.canPlace: not next to two chests, nor next to a double chest
bool chestAt(World& w, int x, int y, int z) { return w.blockAt(x, y, z) == 54; }
bool neighbourChest(World& w, int x, int y, int z) { // isThereANeighborChest
    if (!chestAt(w, x, y, z)) return false;
    return chestAt(w, x - 1, y, z) || chestAt(w, x + 1, y, z) || chestAt(w, x, y, z - 1) || chestAt(w, x, y, z + 1);
}
bool chestPlace(World& w, int x, int y, int z) {
    int l = int(chestAt(w, x - 1, y, z)) + int(chestAt(w, x + 1, y, z)) + int(chestAt(w, x, y, z - 1)) + int(chestAt(w, x, y, z + 1));
    if (l > 1) return false;
    return !neighbourChest(w, x - 1, y, z) && !neighbourChest(w, x + 1, y, z) && !neighbourChest(w, x, y, z - 1) &&
           !neighbourChest(w, x, y, z + 1);
}

const char* tileEntityId(int id) {
    switch (BP[id].cls) {
    case BC::Chest: return "Chest";
    case BC::Furnace: return "Furnace";
    case BC::Sign: return "Sign";
    case BC::MobSpawner: return "MobSpawner";
    case BC::Gear: return "Gear";   // TileEntityGear
    case BC::Motor: return "Motor"; // TileEntityMotor (motor and generator)
    default: return nullptr;
    }
}
} // namespace

void dropBlockAsItem(World& w, int x, int y, int z, int id, int meta) {
    // Block.dropBlockAsItemWithChance(chance 1): quantityDropped(rand), then per item a roll,
    // idDropped(meta, rand) and three floats for the spot. The table is the jar's own answers; the
    // random ones (-2) are written out here.
    JavaRandom& r = w.rand;
    const survival::Drop& d = survival::DROPS[id & 255][meta & 15];
    int qty = d.quantity;
    if (qty == -2) qty = 4 + r.nextInt(2); // BlockRedOre
    for (int i = 0; i < qty; ++i) {
        if (!(r.nextFloat() <= 1.0f)) continue;
        int drop = d.id;
        if (drop == -2) drop = r.nextInt(10) == 0 ? 318 : GRAVEL; // BlockGravel: flint, 1 in 10
        if (drop <= 0) continue;
        float sp = 0.7f;
        double ox = double(r.nextFloat() * sp) + double(1.0f - sp) * 0.5;
        double oy = double(r.nextFloat() * sp) + double(1.0f - sp) * 0.5;
        double oz = double(r.nextFloat() * sp) + double(1.0f - sp) * 0.5;
        w.drops.push_back(World::Drop{double(x) + ox, double(y) + oy, double(z) + oz, drop, 1, d.damage});
    }
}

void dropBlockGoldTouch(World& w, int x, int y, int z, int id, int meta) {
    if (w.rand.nextFloat() > 1.0f) return;
    // the block itself - but crops, sign posts, the wooden pressure plate's id (68, a sign), doors and
    // a lit bulb drop as usual, and the pyramidion and TNT not at all
    if (id == 59 || id == 63 || id == 68 || id == 64 || id == 71 || id == 98) {
        dropBlockAsItem(w, x, y, z, id, meta);
        return;
    }
    if (id <= 0 || id == 99 || id == 46) return;
    float sp = 0.7f;
    double ox = double(w.rand.nextFloat() * sp) + double(1.0f - sp) * 0.5;
    double oy = double(w.rand.nextFloat() * sp) + double(1.0f - sp) * 0.5;
    double oz = double(w.rand.nextFloat() * sp) + double(1.0f - sp) * 0.5;
    w.drops.push_back(World::Drop{double(x) + ox, double(y) + oy, double(z) + oz, id, 1, survival::DROPS[id & 255][meta & 15].damage});
}

// ---- BlockFire
inline bool nsssCloth(int id) { return id == 35 || (id >= 21 && id <= 34) || id == 36; } // NSSS's Classic cloth colours
int fireEncourage(int id) { // NSSS BlockFire.chanceToEncourageFire
    if (nsssCloth(id)) return 30;
    switch (id) { case 5: return 5; case 17: return 5; case 18: return 30; case 47: return 30; case 46: return 15; default: return 0; }
}
int fireCatch(int id) { // abilityToCatchFire
    if (nsssCloth(id)) return 60;
    switch (id) { case 5: return 20; case 17: return 5; case 18: return 60; case 47: return 20; case 46: return 100; default: return 0; }
}
bool fireNextToFuel(World& w, int x, int y, int z) { // func_263_h
    return fireEncourage(w.blockAt(x + 1, y, z)) > 0 || fireEncourage(w.blockAt(x - 1, y, z)) > 0 ||
           fireEncourage(w.blockAt(x, y - 1, z)) > 0 || fireEncourage(w.blockAt(x, y + 1, z)) > 0 ||
           fireEncourage(w.blockAt(x, y, z - 1)) > 0 || fireEncourage(w.blockAt(x, y, z + 1)) > 0;
}
void fireTryCatch(World& w, int x, int y, int z, int chance, JavaRandom& r) { // tryToCatchBlockOnFire
    int id = w.blockAt(x, y, z);
    if (r.nextInt(chance) < fireCatch(id)) {
        bool tnt = id == 46;
        if (r.nextInt(2) == 0) w.setBlockWithNotify(x, y, z, FIRE);
        else w.setBlockWithNotify(x, y, z, 0);
        if (tnt && onTntIgnited) onTntIgnited(x, y, z); // BlockTNT.onBlockDestroyedByPlayer: primed
    }
}
int fireNeighbourChance(World& w, int x, int y, int z) { // getChanceOfNeighborsEncouragingFire
    if (w.blockAt(x, y, z) != 0) return 0;
    int c = 0;
    c = std::max(c, fireEncourage(w.blockAt(x + 1, y, z)));
    c = std::max(c, fireEncourage(w.blockAt(x - 1, y, z)));
    c = std::max(c, fireEncourage(w.blockAt(x, y - 1, z)));
    c = std::max(c, fireEncourage(w.blockAt(x, y + 1, z)));
    c = std::max(c, fireEncourage(w.blockAt(x, y, z - 1)));
    c = std::max(c, fireEncourage(w.blockAt(x, y, z + 1)));
    return c;
}
void fireTick(World& w, int x, int y, int z, JavaRandom& r) { // BlockFire.updateTick
    int meta = w.metaAt(x, y, z);
    if (meta < 15) {
        w.setBlockMetadataWithNotify(x, y, z, meta + 1);
        w.scheduleBlockUpdate(x, y, z, FIRE);
    }
    if (!fireNextToFuel(w, x, y, z)) {
        if (!w.isBlockOpaqueCube(x, y - 1, z) || meta > 3) w.setBlockWithNotify(x, y, z, 0);
    } else if (fireEncourage(w.blockAt(x, y - 1, z)) == 0 && meta == 15 && r.nextInt(4) == 0) {
        w.setBlockWithNotify(x, y, z, 0);
    } else if (meta % 2 == 0 && meta > 2) {
        fireTryCatch(w, x + 1, y, z, 300, r);
        fireTryCatch(w, x - 1, y, z, 300, r);
        fireTryCatch(w, x, y - 1, z, 200, r); // NSSS: 200 below
        fireTryCatch(w, x, y + 1, z, 250, r);
        fireTryCatch(w, x, y, z - 1, 300, r);
        fireTryCatch(w, x, y, z + 1, 300, r);
        for (int a = x - 1; a <= x + 1; ++a)
            for (int c = z - 1; c <= z + 1; ++c)
                for (int b = y - 1; b <= y + 4; ++b) {
                    if (a == x && b == y && c == z) continue;
                    int odds = 100;
                    if (b > y + 1) odds += (b - (y + 1)) * 100;
                    int ch = fireNeighbourChance(w, a, b, c);
                    if (ch > 0 && r.nextInt(odds) <= ch) w.setBlockWithNotify(a, b, c, FIRE);
                }
    }
}

void onBlockAdded(World& w, int x, int y, int z, int id) {
    if (BP[id].container) {
        if (const char* te = tileEntityId(id)) {
            nbt::Tag* t = w.addTileEntity(x, y, z, te);
            if (t && BP[id].cls == BC::Chest) t->set("Items", nbt::makeList(nbt::Compound));
            if (t && BP[id].cls == BC::MobSpawner) {
                t->setString("EntityId", "Pig");
                t->setShort("Delay", 20);
            }
        }
        return;
    }
    if (Redstone::handles(id)) {
        Redstone::onBlockAdded(w, x, y, z, id);
        return;
    }
    switch (BP[id].cls) {
    case BC::Flowing:
        checkForHarden(w, x, y, z, id);
        if (w.blockAt(x, y, z) == id) w.scheduleBlockUpdate(x, y, z, id);
        break;
    case BC::Stationary: checkForHarden(w, x, y, z, id); break;
    case BC::Sponge: spongeAdded(w, x, y, z); break;
    case BC::BurntCloth: // BlockBurntCloth.onBlockAdded: melts the snow and ice two blocks around
        for (int i = x - 2; i <= x + 2; i++)
            for (int j = y - 2; j <= y + 2; j++)
                for (int k = z - 2; k <= z + 2; k++) {
                    int n = w.blockAt(i, j, k);
                    if (n == SNOW || n == ICE) w.setBlockWithNotify(i, j, k, 0);
                }
        break;
    case BC::SpongeInactive: wetSpongeAdded(w, x, y, z); break;
    case BC::Torch: torchAddedNsss(w, x, y, z, id); break;
    case BC::Sand: case BC::Gravel: w.scheduleBlockUpdate(x, y, z, id); break;
    case BC::MinecartTrack: rails::onBlockAdded(w, x, y, z); break;
    case BC::Fire: // BlockFire.onBlockAdded
        if (!w.isBlockOpaqueCube(x, y - 1, z) && !fireNextToFuel(w, x, y, z)) w.setBlockWithNotify(x, y, z, 0);
        else w.scheduleBlockUpdate(x, y, z, id);
        break;
    default: break;
    }
}

void onBlockRemoval(World& w, int x, int y, int z, int id) {
    if (Redstone::handles(id)) Redstone::onBlockRemoval(w, x, y, z, id, w.metaAt(x, y, z));
    if (BP[id].container) {
        // BlockChest / BlockFurnace: what was inside spills out
        static JavaRandom spill(0xC4E57ULL); // the block's own Random in Java, not the world's
        if (nbt::Tag* te = w.tileEntityAt(x, y, z))
            if (nbt::Tag* items = te->getList("Items"))
                for (auto& t : items->items) {
                    int itemId = t->getShort("id"), count = t->getByte("Count"), dmg = t->getShort("Damage");
                    float ox = spill.nextFloat() * 0.8f + 0.1f;
                    float oy = spill.nextFloat() * 0.8f + 0.1f;
                    float oz = spill.nextFloat() * 0.8f + 0.1f;
                    while (count > 0) {
                        int n = spill.nextInt(21) + 10;
                        if (n > count) n = count;
                        count -= n;
                        w.drops.push_back(World::Drop{x + double(ox), y + double(oy), z + double(oz), itemId, n, dmg});
                    }
                }
        w.removeTileEntity(x, y, z);
        return;
    }
    if (id == ICE) { // BlockIce.onBlockRemoval: water, if something holds it up
        Mat m = w.materialAt(x, y - 1, z);
        if (matSolid(m) || matLiquid(m)) w.setBlockWithNotify(x, y, z, WATER);
    }
}

void onNeighborBlockChange(World& w, int x, int y, int z, int id, int neighbour) {
    if (Redstone::handles(id)) {
        Redstone::onNeighborBlockChange(w, x, y, z, id, neighbour);
        return;
    }
    if (isPlant(id)) {
        checkPlant(w, x, y, z, id);
        return;
    }
    auto dropIf = [&](bool fall) {
        if (!fall) return;
        dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
        w.setBlockWithNotify(x, y, z, 0);
    };
    switch (BP[id].cls) {
    case BC::Flowing: case BC::Stationary:
        checkForHarden(w, x, y, z, id);
        if (BP[id].cls == BC::Stationary && w.blockAt(x, y, z) == id) stationaryToFlowing(w, x, y, z, id);
        break;
    case BC::Sand: case BC::Gravel: w.scheduleBlockUpdate(x, y, z, id); break;
    case BC::Gear: gearNeighbourChange(w, x, y, z, id); break;
    case BC::Torch: torchNeighbourChange(w, x, y, z, id); break;
    case BC::Ladder: ladderNeighbourChange(w, x, y, z, id); break;
    case BC::Sign: signNeighbourChange(w, x, y, z, id); break;
    case BC::Soil: // BlockSoil.onNeighborBlockChange: something solid on top makes it dirt
        if (matSolid(w.materialAt(x, y + 1, z))) w.setBlockWithNotify(x, y, z, DIRT);
        break;
    case BC::LightBulb: lightBulbNeighbourChange(w, x, y, z, neighbour); break;
    case BC::BurntCloth: w.setBlock(x, y, z, redPowerAround(w, x, y, z, neighbour) ? 106 : 105); break; // (setBlock, no notify)
    case BC::Sponge: // powered: wet sponge (the water around runs back)
        if (redPowerAround(w, x, y, z, neighbour)) {
            spongeDestroyed(w, x, y, z);
            w.setBlockWithNotify(x, y, z, WET_SPONGE);
        }
        break;
    case BC::SpongeInactive: // power gone: a dry sponge again
        if (!redPowerAround(w, x, y, z, neighbour)) w.setBlockWithNotify(x, y, z, SPONGE_ID);
        break;
    case BC::MinecartTrack: rails::onNeighborChange(w, x, y, z, neighbour); break;
    case BC::Fire: // BlockFire.onNeighborBlockChange
        if (!w.isBlockOpaqueCube(x, y - 1, z) && !fireNextToFuel(w, x, y, z)) w.setBlockWithNotify(x, y, z, 0);
        break;
    case BC::Cactus: dropIf(!cactusStay(w, x, y, z)); break;
    case BC::Reed: dropIf(!reedPlace(w, x, y, z)); break;
    case BC::Snow: dropIf(!snowPlace(w, x, y, z)); break;
    default: break;
    }
}

namespace {
// BlockLeaves.dropChance: 1 in this many leaves drop a sapling (0), an apple (1), a golden apple (2)
int leafDropChance(World& w, int item) {
    static const int BASE[3] = {40, 128, 10000};
    static const int SEASON[4][3] = {{40, 256, 20000}, {60, 128, 10000}, {100, 512, 10000}, {30, 512, 10000}};
    int s = cycleSeason(w);
    return s < 0 ? BASE[item] : SEASON[s][item];
}
// BlockLeaves.updateTick: no log (or mossy brick) within 4 blocks and the leaf goes, maybe dropping
// a sapling, an apple or a golden apple
void nsssLeavesTick(World& w, int x, int y, int z, JavaRandom& r) {
    for (int x1 = x - 4; x1 <= x + 4; ++x1)
        for (int z1 = z - 4; z1 <= z + 4; ++z1)
            for (int y1 = y - 4; y1 <= y + 4; ++y1) {
                int id = w.blockAt(x1, y1, z1);
                if (id == LOG || id == 93) return; // getConnectLog: wood or brickMossy
            }
    w.setBlockWithNotify(x, y, z, 0);
    if (r.nextInt(leafDropChance(w, 0)) == 0) w.drops.push_back(World::Drop{double(x), double(y), double(z), 6, 1, 0});
    if (r.nextInt(leafDropChance(w, 1)) == 0) w.drops.push_back(World::Drop{double(x), double(y), double(z), 260, 1, 0});
    if (r.nextInt(leafDropChance(w, 2)) == 0) w.drops.push_back(World::Drop{double(x), double(y), double(z), 322, 1, 0});
}

// BlockCrops.getGrowthRate
float cropGrowth(World& w, int x, int y, int z, int id) {
    float rate = 1.0f;
    int n = w.blockAt(x, y, z - 1), s = w.blockAt(x, y, z + 1), we = w.blockAt(x - 1, y, z), e = w.blockAt(x + 1, y, z);
    int nw = w.blockAt(x - 1, y, z - 1), ne = w.blockAt(x + 1, y, z - 1), se = w.blockAt(x + 1, y, z + 1),
        sw = w.blockAt(x - 1, y, z + 1);
    bool rowX = we == id || e == id, rowZ = n == id || s == id;
    bool diag = nw == id || ne == id || se == id || sw == id;
    for (int a = x - 1; a <= x + 1; ++a)
        for (int b = z - 1; b <= z + 1; ++b) {
            float f = 0.0f;
            if (w.blockAt(a, y - 1, b) == TILLED) {
                f = 1.0f;
                if (w.metaAt(a, y - 1, b) > 0) f = 3.0f;
            }
            if (a != x || b != z) f /= 4.0f;
            rate += f;
        }
    if (diag || (rowX && rowZ)) rate /= 2.0f;
    switch (cycleSeason(w)) {
    case 1: rate /= 3.0f; break;  // fall
    case 2: rate /= 10.0f; break; // winter
    case 3: rate *= 3.0f; break;  // spring
    default: break;
    }
    return rate;
}

// BlockReed / BlockCactus: grow a block every 16 ticks, up to 3 tall
void growColumn(World& w, int x, int y, int z, int id) {
    if (!w.isAirBlock(x, y + 1, z)) return;
    int h = 1;
    while (w.blockAt(x, y - h, z) == id) ++h;
    if (h < 3) {
        int m = w.metaAt(x, y, z);
        if (m == 15) {
            w.setBlockWithNotify(x, y + 1, z, id);
            w.setBlockMetadataWithNotify(x, y, z, 0);
        } else {
            w.setBlockMetadataWithNotify(x, y, z, m + 1);
        }
    }
}

} // namespace

void updateTick(World& w, int x, int y, int z, int id, JavaRandom& r) {
    if (Redstone::handles(id)) {
        Redstone::updateTick(w, x, y, z, id, r);
        return;
    }
    if (BP[id].cls == BC::Mushroom) { // NSSS BlockMushroom.updateTick (no stay check): spread
        if (r.nextInt(25) == 0) {
            int left = 15;
            for (int a = x - 4; a <= x + 4; ++a)
                for (int c = z - 4; c <= z + 4; ++c)
                    for (int b = y - 1; b <= y + 1; ++b)
                        if (w.blockAt(a, b, c) == id && --left <= 0) return;
            int k = x + r.nextInt(3) - 1;
            int t = r.nextInt(2); int i1 = y + t - r.nextInt(2);
            int k1 = z + r.nextInt(3) - 1;
            int px = x, py = y, pz = z;
            for (int l1 = 0; l1 < 4; ++l1) {
                if (w.isAirBlock(k, i1, k1) && plantStay(w, id, k, i1, k1)) { px = k; py = i1; pz = k1; }
                k = px + r.nextInt(3) - 1;
                t = r.nextInt(2); i1 = py + t - r.nextInt(2);
                k1 = pz + r.nextInt(3) - 1;
            }
            if (w.isAirBlock(k, i1, k1) && plantStay(w, id, k, i1, k1)) w.setBlockWithNotify(k, i1, k1, id);
        }
        return;
    }
    if (isPlant(id)) {
        checkPlant(w, x, y, z, id); // BlockFlower.updateTick
        if (w.blockAt(x, y, z) != id) return;
        switch (BP[id].cls) {
        case BC::Sapling: { // BlockSapling: 16 stages, 1 in growthChance
            int s = cycleSeason(w);
            int chance = s == 1 ? 15 : s == 2 ? 50 : s == 3 ? 3 : 5;
            if (w.getBlockLightValue(x, y + 1, z) >= 9 && r.nextInt(chance) == 0) {
                int meta = w.metaAt(x, y, z);
                if (meta < 15) {
                    w.setBlockMetadataWithNotify(x, y, z, meta + 1);
                } else {
                    w.setBlock(x, y, z, 0);
                    if (!nsssSaplingTree(w, r, x, y, z)) w.setBlock(x, y, z, id);
                }
            }
            break;
        }
        case BC::Crops:
            if (w.getBlockLightValue(x, y + 1, z) >= 9) {
                int m = w.metaAt(x, y, z);
                if (m < 7) {
                    float g = cropGrowth(w, x, y, z, id);
                    if (r.nextInt(int(100.0f / g)) == 0) w.setBlockMetadataWithNotify(x, y, z, m + 1);
                }
            }
            break;
        default: break;
        }
        return;
    }
    switch (BP[id].cls) {
    case BC::Grass: {
        // summer evenings in a world with seasons: a lightning bug over unlit grass
        int64_t t = w.info.time % 24000;
        if (cycleSeason(w) == 0 && t > 12000 && t < 16000 && w.getSavedLightValue(Sky::Block, x, y + 1, z) <= 7) {
            double px = x + 0.5 + (r.nextFloat() - 0.5), py = y + double(1.0625f), pz = z + 0.5 + (r.nextFloat() - 0.5);
            if (w.onParticle) w.onParticle(2, px, py, pz, 0, 0, 0); // "lightningbug"
        }
        if (w.getBlockLightValue(x, y + 1, z) < 4 && matBlocksGrass(w.materialAt(x, y + 1, z))) {
            if (r.nextInt(4) != 0) return;
            w.setBlockWithNotify(x, y, z, DIRT);
        } else if (w.getBlockLightValue(x, y + 1, z) >= 9) {
            int a = x + r.nextInt(3) - 1;
            int b = y + r.nextInt(5) - 3;
            int c = z + r.nextInt(3) - 1;
            if (w.blockAt(a, b, c) == DIRT && w.getBlockLightValue(a, b + 1, c) >= 4 && !matBlocksGrass(w.materialAt(a, b + 1, c)))
                w.setBlockWithNotify(a, b, c, GRASS);
        }
        break;
    }
    case BC::Soil:
        if (r.nextInt(5) == 0) {
            bool water = false;
            for (int a = x - 4; a <= x + 4 && !water; ++a)
                for (int b = y; b <= y + 1 && !water; ++b)
                    for (int c = z - 4; c <= z + 4; ++c)
                        if (w.materialAt(a, b, c) == Mat::water) { water = true; break; }
            if (water) {
                w.setBlockMetadataWithNotify(x, y, z, 7);
            } else {
                int m = w.metaAt(x, y, z);
                if (m > 0) w.setBlockMetadataWithNotify(x, y, z, m - 1);
                else if (w.blockAt(x, y + 1, z) != 59) w.setBlockWithNotify(x, y, z, DIRT);
            }
        }
        break;
    case BC::Reed: growColumn(w, x, y, z, id); break;
    case BC::Cactus: growColumn(w, x, y, z, id); break;
    case BC::Leaves: nsssLeavesTick(w, x, y, z, r); break;
    case BC::Snow: case BC::SnowBlock: // melt by torchlight
        if (w.getSavedLightValue(Sky::Block, x, y, z) > 11) {
            dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::Ice:
        if (w.getSavedLightValue(Sky::Block, x, y, z) > 11 - BP[id].opacity) {
            dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
            w.setBlockWithNotify(x, y, z, WATER_STILL);
        }
        break;
    case BC::RedstoneOre:
        if (id == 74) w.setBlockWithNotify(x, y, z, 73);
        break;
    case BC::Flowing: flowingTick(w, x, y, z, id, r); break;
    case BC::Stationary: stationaryTick(w, x, y, z, id, r); break;
    case BC::Sand: case BC::Gravel: tryToFall(w, x, y, z, id); break;
    case BC::Fire: fireTick(w, x, y, z, r); break;
    default: break;
    }
}

// Block.canPlace and its overrides (World.func_695_a asks it)
bool canPlace(World& w, int id, int x, int y, int z) {
    auto free = [&](int a, int b, int c) { // Block.canPlace: air, or a liquid
        int l = w.blockAt(a, b, c);
        return l == 0 || matLiquid(BP[l].mat);
    };
    auto sides = [&] { // a solid block beside it
        return normalCube(w, x - 1, y, z) || normalCube(w, x + 1, y, z) || normalCube(w, x, y, z - 1) || normalCube(w, x, y, z + 1);
    };
    if (isPlant(id)) return growsOn(id, w.blockAt(x, y - 1, z)); // BlockFlower.canPlace
    switch (BP[id].cls) {
    case BC::Cactus: return free(x, y, z) && cactusStay(w, x, y, z);
    case BC::Door: return y < 127 && normalCube(w, x, y - 1, z) && free(x, y, z) && free(x, y + 1, z);
    case BC::Fire: return normalCube(w, x, y - 1, z) || fireNextToFuel(w, x, y, z);
    case BC::PressurePlate: return w.blockAt(x, y - 1, z) == 85 || normalCube(w, x, y - 1, z);
    case BC::RedstoneWire: case BC::MinecartTrack: return normalCube(w, x, y - 1, z);
    case BC::Stairs: return free(x, y, z); // its modelBlock's
    case BC::Button: case BC::Gear: case BC::Ladder: return sides();
    case BC::Chest: return chestPlace(w, x, y, z);
    case BC::Ice: return !hotNearby(w, x, y, z) && free(x, y, z);
    case BC::Reed: return reedPlace(w, x, y, z);
    case BC::Snow: return snowPlace(w, x, y, z);
    case BC::Torch: case BC::RedstoneTorch: return torchCanPlace(w, x, y, z);
    default: return free(x, y, z);
    }
}

bool canBlockStay(World& w, int id, int x, int y, int z) {
    if (isPlant(id)) return plantStay(w, id, x, y, z);
    switch (BP[id].cls) {
    case BC::Cactus: return cactusStay(w, x, y, z);
    case BC::Reed: return reedPlace(w, x, y, z);
    default: return true;
    }
}

void onBlockDestroyedByPlayer(World& w, int x, int y, int z, int id, int meta) {
    switch (id) {
    case 46: // BlockTNT: primed, with its fuse
        if (onTntIgnited) onTntIgnited(x, y, z);
        break;
    case SPONGE_ID: spongeDestroyed(w, x, y, z); break; // the water held back runs again
    case 59: // BlockCrops: up to three seeds, likelier the riper it was
        for (int i = 0; i < 3; ++i)
            if (w.rand.nextInt(15) <= meta) {
                const float f = 0.7f;
                float f1 = w.rand.nextFloat() * f + (1.0f - f) * 0.5f;
                float f2 = w.rand.nextFloat() * f + (1.0f - f) * 0.5f;
                float f3 = w.rand.nextFloat() * f + (1.0f - f) * 0.5f;
                w.drops.push_back(World::Drop{x + double(f1), y + double(f2), z + double(f3), 295, 1, 0});
            }
        break;
    case LEAVES: { // BlockLeaves: a sapling, an apple, a golden apple (its own new Random)
        static JavaRandom leafRand(0x1EAF5EEDLL);
        if (leafRand.nextInt(leafDropChance(w, 0)) == 0) w.drops.push_back(World::Drop{double(x), double(y), double(z), 6, 1, 0});
        if (leafRand.nextInt(leafDropChance(w, 1)) == 0) w.drops.push_back(World::Drop{double(x), double(y), double(z), 260, 1, 0});
        if (leafRand.nextInt(leafDropChance(w, 2)) == 0) w.drops.push_back(World::Drop{double(x), double(y), double(z), 322, 1, 0});
        break;
    }
    case 99: // BlockPyramidion: a giant (and on peaceful the block comes back)
        if (onPyramidionBroken) onPyramidionBroken(x, y, z);
        break;
    default: break;
    }
}

void torchAdded(World& w, int i, int j, int k, int id) { torchAddedNsss(w, i, j, k, id); }
bool hotClothNearby(World& w, int x, int y, int z) { return hotNearby(w, x, y, z); }
void torchNeighbourChanged(World& w, int x, int y, int z, int id) { torchNeighbourChange(w, x, y, z, id); }
} // namespace Blocks
