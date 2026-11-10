#include "Blocks.h"
#include "gen/ChunkGenerator.h"
#include "Rails.h"
#include <cstdio>
#include <cstdlib>
#include "BlockProps.h"
#include "World.h"
#include "Redstone.h"
#include "game/Survival.h"

namespace Blocks {
bool sandFallInstantly = false;
bool flowerGenerating = false;
bool iceCollecting = false;
void (*onTntIgnited)(int x, int y, int z) = nullptr;
bool quietRemoval = false;

namespace {
// block ids (MCP names in Beta: waterStill = 8 flowing, waterMoving = 9 still, same for lava)
enum : int {
    STONE = 1, GRASS = 2, DIRT = 3, COBBLE = 4, SAPLING = 6, WATER = 8, WATER_STILL = 9, LAVA = 10,
    LAVA_STILL = 11, SAND = 12, GRAVEL = 13, LOG = 17, LEAVES = 18, TALLGRASS = 31, DEADBUSH = 32,
    FLOWER_Y = 37, FLOWER_R = 38, MUSH_B = 39, MUSH_R = 40, OBSIDIAN = 49, FIRE = 51, SIGN = 63,
    LADDER = 65, TILLED = 60, SNOW = 78, ICE = 79, CACTUS = 81, REED = 83, SLOWSAND = 88, BLOODSTONE = 87,
    CHARRED_COBBLE = 101, CHARRED_STONE = 102, CHARRED_SMOOTH = 103, CHARRED_MOSSY = 109,
    DOUBLE_PLANT = 113, MANABLOOM = 114, WISP_LILY = 115, AX_FLOWERS = 116, CHARRED_SAPLING = 119,
    DUSKPETAL_OPEN = 134, DOUBLE_GLOW = 135, DOUBLE_PLANT2 = 136, CATTAIL = 141, ICICLE = 144,
    COMPACTED_ICE = 156, BONE_PILE = 170, STONE_TYPES = 171, PORTAL = 90,
};

inline bool isDoor(int id) { return BP[id].cls == BC::Door; }
inline bool opaque(int id) { return BP[id].opaqueLookup; } // Block.opaqueCubeLookup

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
    if (m == 0) w.setBlockWithNotify(x, y, z, OBSIDIAN);
    else if (m <= 4) w.setBlockWithNotify(x, y, z, COBBLE);
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

bool blocksFlow(World& w, int x, int y, int z) { // func_295_l
    int id = w.blockAt(x, y, z);
    if (isDoor(id) || id == SIGN || id == LADDER || id == REED) return true;
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
    int step = m == Mat::lava && !w.hell ? 2 : 1; // lava runs further in the Nether
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
        if (s.sources >= 2 && m == Mat::water) {
            if (w.isBlockOpaqueCube(x, y - 1, z)) next = 0;
            else if (w.materialAt(x, y - 1, z) == m && w.metaAt(x, y, z) == 0) next = 0;
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

// ---------------------------------------------------------------- plants (BlockFlower family)

bool isPlant(int id) {
    switch (BP[id].cls) {
    case BC::Flower: case BC::Mushroom: case BC::TallGrass: case BC::DeadBush: case BC::Sapling: case BC::Crops:
    case BC::AxeolonFlower: case BC::DoublePlant: case BC::Cattail:
        return true;
    default:
        return false;
    }
}

bool normalSoil(int id) { return id == GRASS || id == DIRT || id == TILLED; }
bool isCharred(int id) { return id == CHARRED_COBBLE || id == CHARRED_STONE || id == CHARRED_SMOOTH || id == CHARRED_MOSSY; }
bool isCaveFloor(int id) { return id == STONE || id == COBBLE || id == GRAVEL || id == DIRT || id == GRASS; }

bool growsOn(int plant, int below) { // canThisPlantGrowOnThisBlockID
    switch (BP[plant].cls) {
    case BC::Mushroom: return opaque(below);
    case BC::DeadBush: return below == SAND || below == GRASS || below == DIRT;
    case BC::Crops: return below == TILLED;
    case BC::AxeolonFlower: case BC::DoublePlant:
        return normalSoil(below) || isCharred(below) || isCaveFloor(below) || below == BLOODSTONE || below == SLOWSAND;
    default: return normalSoil(below);
    }
}

bool flowerStay(World& w, int id, int x, int y, int z) { // BlockFlower.canBlockStay
    return !needsLight(w, x, y, z) && growsOn(id, w.blockAt(x, y - 1, z));
}

bool cattailSpot(World& w, int x, int y, int z) {
    return w.materialAt(x, y - 1, z) == Mat::water && matSolid(w.materialAt(x, y - 2, z));
}

bool plantStay(World& w, int id, int x, int y, int z) {
    switch (BP[id].cls) {
    case BC::Mushroom:
        return w.getBlockLightValue(x, y, z) <= 13 && growsOn(id, w.blockAt(x, y - 1, z));
    case BC::AxeolonFlower: {
        int below = w.blockAt(x, y - 1, z);
        bool lit = !needsLight(w, x, y, z);
        if (id == WISP_LILY) return isCaveFloor(below);
        if (id == MANABLOOM || id == DUSKPETAL_OPEN) return lit && normalSoil(below);
        int meta = w.metaAt(x, y, z);
        if (meta == 0) return isCharred(below);                          // ashcap
        if (meta == 1) return below == BLOODSTONE || below == SLOWSAND; // ember bud
        return lit && normalSoil(below);
    }
    case BC::DoublePlant: {
        int meta = w.metaAt(x, y, z);
        if (meta & 8) return w.blockAt(x, y - 1, z) == id && (w.metaAt(x, y - 1, z) & 8) == 0;
        if (w.blockAt(x, y + 1, z) != id || (w.metaAt(x, y + 1, z) & 8) == 0) return false;
        int type = meta & 7;
        int set = id == DOUBLE_PLANT ? 0 : (id == DOUBLE_GLOW ? 1 : 2);
        int below = w.blockAt(x, y - 1, z);
        bool lit = !needsLight(w, x, y, z);
        if (set == 0 && type == 5) return isCharred(below);
        if (set == 0 && type == 6) return below == BLOODSTONE || below == SLOWSAND;
        if (set == 1 && type == 1) return isCaveFloor(below);
        if (set == 0 && (type == 0 || type == 1)) return flowerStay(w, id, x, y, z);
        return lit && normalSoil(below);
    }
    case BC::Cattail:
        if (w.metaAt(x, y, z) & 8) return w.blockAt(x, y - 1, z) == id && (w.metaAt(x, y - 1, z) & 8) == 0;
        return cattailSpot(w, x, y, z) && w.blockAt(x, y + 1, z) == id && (w.metaAt(x, y + 1, z) & 8) != 0;
    default:
        return flowerStay(w, id, x, y, z);
    }
}

void checkPlant(World& w, int x, int y, int z, int id) { // func_268_h
    if (!plantStay(w, id, x, y, z)) {
        dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
        w.setBlockWithNotify(x, y, z, 0);
    }
}

// ---------------------------------------------------------------- others

bool cactusStay(World& w, int x, int y, int z) {
    if (matSolid(w.materialAt(x - 1, y, z)) || matSolid(w.materialAt(x + 1, y, z)) || matSolid(w.materialAt(x, y, z - 1)) ||
        matSolid(w.materialAt(x, y, z + 1)))
        return false;
    int b = w.blockAt(x, y - 1, z);
    return b == CACTUS || b == SAND;
}

bool reedPlace(World& w, int x, int y, int z) {
    int b = w.blockAt(x, y - 1, z);
    if (b == REED) return true;
    if (b != GRASS && b != DIRT && b != SAND && b != GRAVEL) return false;
    return w.materialAt(x - 1, y - 1, z) == Mat::water || w.materialAt(x + 1, y - 1, z) == Mat::water ||
           w.materialAt(x, y - 1, z - 1) == Mat::water || w.materialAt(x, y - 1, z + 1) == Mat::water;
}

bool snowPlace(World& w, int x, int y, int z) {
    int b = w.blockAt(x, y - 1, z);
    return b != 0 && BP[b].opaqueCube ? matSolid(w.materialAt(x, y - 1, z)) : false; // isOpaqueCube()
}

bool icicleHolds(World& w, int x, int y, int z, bool rising) {
    int id = w.blockAt(x, y, z);
    if (id == ICICLE) return ((w.metaAt(x, y, z) & 1) == 1) == rising;
    return w.isBlockOpaqueCube(x, y, z);
}

void leafDecayMark(World& w, int x, int y, int z, int r, bool onlyClear) {
    int g = r + 1;
    if (!w.checkChunksExist(x - g, y - g, z - g, x + g, y + g, z + g)) return;
    for (int i = -r; i <= r; ++i)
        for (int j = -r; j <= r; ++j)
            for (int k = -r; k <= r; ++k) {
                if (w.blockAt(x + i, y + j, z + k) != LEAVES) continue;
                int m = w.metaAt(x + i, y + j, z + k);
                if (onlyClear && (m & 4) != 0) continue;
                w.setBlockMetadata(x + i, y + j, z + k, m | 4);
            }
}

const char* tileEntityId(int id) {
    switch (BP[id].cls) {
    case BC::Chest: return "Chest";
    case BC::Furnace: return "Furnace";
    case BC::Dispenser: return "Trap";
    case BC::Sign: return "Sign";
    case BC::MobSpawner: return "MobSpawner";
    case BC::Note: return "Music";
    case BC::MixedSlab: return "AxeolonMixedSlab";
    case BC::FlowerPot: return "AxeolonFlowerPot";
    case BC::Crate: return "AxeolonCrate";
    case BC::SpawnCrystal: return "AxeolonSpawnCrystal";
    case BC::Hopper: return "AxeolonHopper";
    default: return nullptr;
    }
}

void freezeNeighbours(World& w, int x, int y, int z) {
    static const int D[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (auto& d : D)
        if (w.materialAt(x + d[0], y + d[1], z + d[2]) == Mat::water) w.setBlockWithNotify(x + d[0], y + d[1], z + d[2], ICE);
}
} // namespace

void dropBlockAsItem(World& w, int x, int y, int z, int id, int meta) {
    // Block.dropBlockAsItemWithChance(chance 1): quantityDropped(rand), then per item a roll,
    // idDropped(meta, rand) and three floats for the spot. The table is the mod's own answers;
    // the random ones (-2) are written out here.
    JavaRandom& r = w.rand;
    const survival::Drop& d = survival::DROPS[id & 255][meta & 15];
    int qty = d.quantity;
    if (BP[id].cls == BC::Ice) qty = iceCollecting ? 1 : 0;
    else if (qty == -2) {
        switch (id) {
        case LEAVES: qty = r.nextInt(16) == 0 ? 1 : 0; break;
        case 21: case 222: qty = 4 + r.nextInt(5); break;            // lapis
        case 73: case 74: case 220: case 223: qty = 4 + r.nextInt(2); break; // redstone
        case BONE_PILE: qty = 1 + r.nextInt(3); break;
        default: qty = 1; break;
        }
    }
    for (int i = 0; i < qty; ++i) {
        if (!(r.nextFloat() <= 1.0f)) continue;
        int drop = d.id;
        if (drop == -2) {
            if (id == GRAVEL) drop = r.nextInt(10) == 0 ? 318 : GRAVEL;
            else drop = r.nextInt(8) == 0 ? 295 : -1; // tall grass, double grass and ferns: seeds
        }
        if (drop <= 0) continue;
        float sp = 0.7f;
        double ox = double(r.nextFloat() * sp) + double(1.0f - sp) * 0.5;
        double oy = double(r.nextFloat() * sp) + double(1.0f - sp) * 0.5;
        double oz = double(r.nextFloat() * sp) + double(1.0f - sp) * 0.5;
        w.drops.push_back(World::Drop{double(x) + ox, double(y) + oy, double(z) + oz, drop, 1, d.damage});
    }
}

bool needsLight(World& w, int x, int y, int z) {
    return flowerGenerating && w.getBlockLightValue(x, y, z) < 8 && !w.canBlockSeeTheSky(x, y, z);
}

bool isNaturalStone(int id) { return id >= STONE_TYPES && id <= STONE_TYPES + 6; }

int oreKind(int id) {
    switch (id) {
    case 16: return 0;
    case 15: return 1;
    case 14: return 2;
    case 73: case 74: return 3;
    case 56: return 4;
    case 21: return 5;
    default: return -1;
    }
}

bool placeCattail(World& w, int x, int y, int z, bool notify) {
    if (!(w.blockAt(x, y, z) == 0 && w.blockAt(x, y + 1, z) == 0 && cattailSpot(w, x, y, z))) return false;
    w.setBlockAndMetadataJ(x, y, z, CATTAIL, 0);
    if (notify) {
        w.setBlockAndMetadataWithNotify(x, y + 1, z, CATTAIL, 8);
        w.markBlockNeedsUpdate(x, y, z);
    } else {
        w.setBlockAndMetadataJ(x, y + 1, z, CATTAIL, 8);
    }
    return true;
}

// ---- BlockFire
int fireEncourage(int id) { // chanceToEncourageFire
    switch (id) { case 5: return 5; case 17: return 5; case 18: return 30; case 47: return 30; case 46: return 15; case 35: return 30; default: return 0; }
}
int fireCatch(int id) { // abilityToCatchFire
    switch (id) { case 5: return 20; case 17: return 5; case 18: return 60; case 47: return 20; case 46: return 100; case 35: return 60; default: return 0; }
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
    bool eternal = w.blockAt(x, y - 1, z) == BLOODSTONE;
    int meta = w.metaAt(x, y, z);
    if (meta < 15) {
        w.setBlockMetadataWithNotify(x, y, z, meta + 1);
        w.scheduleBlockUpdate(x, y, z, FIRE);
    }
    if (!eternal && !fireNextToFuel(w, x, y, z)) {
        if (!w.isBlockOpaqueCube(x, y - 1, z) || meta > 3) w.setBlockWithNotify(x, y, z, 0);
    } else if (!eternal && fireEncourage(w.blockAt(x, y - 1, z)) == 0 && meta == 15 && r.nextInt(4) == 0) {
        w.setBlockWithNotify(x, y, z, 0);
    } else if (meta % 2 == 0 && meta > 2) {
        fireTryCatch(w, x + 1, y, z, 300, r);
        fireTryCatch(w, x - 1, y, z, 300, r);
        fireTryCatch(w, x, y - 1, z, 250, r);
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

// ---------------------------------------------------------------- Nether portal (BlockPortal)

namespace {
void portalNeighborChange(World& w, int x, int y, int z) { // BlockPortal.onNeighborBlockChange
    int dx = 0, dz = 1;
    if (w.blockAt(x - 1, y, z) == PORTAL || w.blockAt(x + 1, y, z) == PORTAL) { dx = 1; dz = 0; }
    int by = y;
    while (w.blockAt(x, by - 1, z) == PORTAL) --by;
    if (w.blockAt(x, by - 1, z) != OBSIDIAN) { w.setBlockWithNotify(x, y, z, 0); return; }
    int h = 1;
    while (h < 4 && w.blockAt(x, by + h, z) == PORTAL) ++h;
    if (h == 3 && w.blockAt(x, by + h, z) == OBSIDIAN) {
        bool alongX = w.blockAt(x - 1, y, z) == PORTAL || w.blockAt(x + 1, y, z) == PORTAL;
        bool alongZ = w.blockAt(x, y, z - 1) == PORTAL || w.blockAt(x, y, z + 1) == PORTAL;
        if (alongX && alongZ) w.setBlockWithNotify(x, y, z, 0);
        else if ((w.blockAt(x + dx, y, z + dz) != OBSIDIAN || w.blockAt(x - dx, y, z - dz) != PORTAL) &&
                 (w.blockAt(x - dx, y, z - dz) != OBSIDIAN || w.blockAt(x + dx, y, z + dz) != PORTAL))
            w.setBlockWithNotify(x, y, z, 0);
    } else {
        w.setBlockWithNotify(x, y, z, 0);
    }
}
} // namespace

bool tryToCreatePortal(World& w, int x, int y, int z) {
    int dx = 0, dz = 0;
    if (w.blockAt(x - 1, y, z) == OBSIDIAN || w.blockAt(x + 1, y, z) == OBSIDIAN) dx = 1;
    if (w.blockAt(x, y, z - 1) == OBSIDIAN || w.blockAt(x, y, z + 1) == OBSIDIAN) dz = 1;
    if (dx == dz) return false;
    if (w.blockAt(x - dx, y, z - dz) == 0) { x -= dx; z -= dz; }
    for (int i = -1; i <= 2; ++i)
        for (int j = -1; j <= 3; ++j) {
            bool frame = i == -1 || i == 2 || j == -1 || j == 3;
            if ((i == -1 || i == 2) && (j == -1 || j == 3)) continue; // the corners don't matter
            int id = w.blockAt(x + dx * i, y + j, z + dz * i);
            if (frame) {
                if (id != OBSIDIAN) return false;
            } else if (id != 0 && id != FIRE) {
                return false;
            }
        }
    w.editingBlocks = true;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j) w.setBlockWithNotify(x + dx * i, y + j, z + dz * i, PORTAL);
    w.editingBlocks = false;
    return true;
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
    case BC::Sand: case BC::Gravel: w.scheduleBlockUpdate(x, y, z, id); break;
    case BC::Frozen:
        if (id == COMPACTED_ICE) w.scheduleBlockUpdate(x, y, z, id);
        break;
    case BC::MagicLight: w.scheduleBlockUpdate(x, y, z, id); break;
    case BC::MinecartTrack: rails::onBlockAdded(w, x, y, z); break;
    case BC::Fire: // BlockFire.onBlockAdded: on obsidian it may light a portal
        if (w.blockAt(x, y - 1, z) == OBSIDIAN && tryToCreatePortal(w, x, y, z)) break;
        if (!w.isBlockOpaqueCube(x, y - 1, z) && !fireNextToFuel(w, x, y, z)) w.setBlockWithNotify(x, y, z, 0);
        else w.scheduleBlockUpdate(x, y, z, id);
        break;
    default: break;
    }
}

void onBlockRemoval(World& w, int x, int y, int z, int id) {
    if (Redstone::handles(id)) Redstone::onBlockRemoval(w, x, y, z, id, w.metaAt(x, y, z));
    if (quietRemoval) {
        if (BP[id].container) w.removeTileEntity(x, y, z);
        return;
    }
    if (BP[id].cls == BC::Crate) {
        // BlockCrate: it drops itself with what's inside (like a shulker box)
        std::shared_ptr<nbt::Tag> tag;
        if (nbt::Tag* te = w.tileEntityAt(x, y, z))
            if (nbt::Tag* items = te->getList("Items"); items && !items->items.empty()) {
                tag = std::shared_ptr<nbt::Tag>(nbt::makeCompound().release());
                tag->set("Items", nbt::clone(*items));
            }
        w.drops.push_back(World::Drop{x + 0.5, y + 0.5, z + 0.5, id, 1, 0, tag});
        w.removeTileEntity(x, y, z);
        return;
    }
    if (BP[id].container) {
        // BlockChest / BlockFurnace / BlockCrate: what was inside spills out
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
    switch (BP[id].cls) {
    case BC::Leaves:
        leafDecayMark(w, x, y, z, 1, false);
        break;
    case BC::Log: leafDecayMark(w, x, y, z, 4, true); break;
    case BC::Ice: {
        if (iceCollecting) break;
        Mat m = w.materialAt(x, y - 1, z);
        if (matSolid(m) || matLiquid(m)) w.setBlockWithNotify(x, y, z, WATER);
        break;
    }
    default: break;
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
    switch (BP[id].cls) {
    case BC::Flowing: case BC::Stationary:
        checkForHarden(w, x, y, z, id);
        if (w.blockAt(x, y + 1, z) == CATTAIL) onNeighborBlockChange(w, x, y + 1, z, CATTAIL, neighbour);
        if (BP[id].cls == BC::Stationary && w.blockAt(x, y, z) == id) stationaryToFlowing(w, x, y, z, id);
        break;
    case BC::Sand: case BC::Gravel: w.scheduleBlockUpdate(x, y, z, id); break;
    case BC::Portal: portalNeighborChange(w, x, y, z); break;
    case BC::MinecartTrack: rails::onNeighborChange(w, x, y, z, neighbour); break;
    case BC::Cake: // nothing under it: gone (it drops nothing)
        if (!matSolid(w.materialAt(x, y - 1, z))) w.setBlockWithNotify(x, y, z, 0);
        break;
    case BC::Fire: // BlockFire.onNeighborBlockChange
        if (!w.isBlockOpaqueCube(x, y - 1, z) && !fireNextToFuel(w, x, y, z)) w.setBlockWithNotify(x, y, z, 0);
        break;
    case BC::Cactus:
        if (!cactusStay(w, x, y, z)) {
            dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::Reed:
        if (!reedPlace(w, x, y, z)) {
            dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::Snow:
        if (!snowPlace(w, x, y, z)) {
            dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::Frozen:
        if (id == COMPACTED_ICE) w.scheduleBlockUpdate(x, y, z, id);
        break;
    case BC::BonePile:
        if (!w.isBlockOpaqueCube(x, y - 1, z)) {
            dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::Icicle: {
        bool rising = (w.metaAt(x, y, z) & 1) == 1;
        if (rising) {
            if (!icicleHolds(w, x, y - 1, z, true)) {
                dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
                w.setBlockWithNotify(x, y, z, 0);
            }
        } else if (!icicleHolds(w, x, y + 1, z, false)) {
            w.setBlockWithNotify(x, y, z, 0); // BlockIcicle.fall: the falling icicle entity isn't ported yet
        }
        break;
    }
    default: break;
    }
}

namespace {
// BlockLeaves.updateTick: a marked leaf with no log within 4 (through leaves) decays
int leafScratch[32 * 32 * 32];
void leavesTick(World& w, int x, int y, int z, int id) {
    int meta = w.metaAt(x, y, z);
    if ((meta & 4) == 0) return;
    const int R = 4, S = 32, S2 = S * S, H = S / 2;
    auto at = [&](int a, int b, int c) -> int& { return leafScratch[(a + H) * S2 + (b + H) * S + c + H]; };
    if (w.checkChunksExist(x - R - 1, y - R - 1, z - R - 1, x + R + 1, y + R + 1, z + R + 1)) {
        for (int a = -R; a <= R; ++a)
            for (int b = -R; b <= R; ++b)
                for (int c = -R; c <= R; ++c) {
                    int n = w.blockAt(x + a, y + b, z + c);
                    at(a, b, c) = n == LOG ? 0 : n == LEAVES ? -2 : -1;
                }
        for (int d = 1; d <= 4; ++d)
            for (int a = -R; a <= R; ++a)
                for (int b = -R; b <= R; ++b)
                    for (int c = -R; c <= R; ++c)
                        if (at(a, b, c) == d - 1) {
                            if (at(a - 1, b, c) == -2) at(a - 1, b, c) = d;
                            if (at(a + 1, b, c) == -2) at(a + 1, b, c) = d;
                            if (at(a, b - 1, c) == -2) at(a, b - 1, c) = d;
                            if (at(a, b + 1, c) == -2) at(a, b + 1, c) = d;
                            if (at(a, b, c - 1) == -2) at(a, b, c - 1) = d;
                            if (at(a, b, c + 1) == -2) at(a, b, c + 1) = d;
                        }
    }
    if (at(0, 0, 0) >= 0) {
        w.setBlockMetadataWithNotify(x, y, z, meta & ~4);
    } else { // removeLeaves
        dropBlockAsItem(w, x, y, z, id, w.metaAt(x, y, z));
        w.setBlockWithNotify(x, y, z, 0);
    }
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
    if (isPlant(id)) {
        checkPlant(w, x, y, z, id); // BlockFlower.updateTick
        if (w.blockAt(x, y, z) != id) return;
        switch (BP[id].cls) {
        case BC::Sapling:
            if (w.getBlockLightValue(x, y + 1, z) >= 9 && r.nextInt(7) == 0) {
                int meta = w.metaAt(x, y, z);
                if ((meta & 8) == 0) w.setBlockMetadataWithNotify(x, y, z, meta | 8);
                else growSapling(w, r, x, y, z, id, meta);
            }
            break;
        case BC::Crops:
            if (w.getBlockLightValue(x, y + 1, z) >= 9) {
                int m = w.metaAt(x, y, z);
                if (m < 7) {
                    float g = cropGrowth(w, x, y, z, id);
                    if (r.nextInt(int(100.0f / g)) == 0) w.setBlockMetadataWithNotify(x, y, z, m + 1);
                }
            }
            break;
        case BC::AxeolonFlower: { // Duskpetals open at night and close by day
            bool day = w.skylightSubtracted < 4;
            int meta = w.metaAt(x, y, z);
            if (id == AX_FLOWERS) {
                if ((meta == 2 || meta == 3) && !day) w.setBlockAndMetadataWithNotify(x, y, z, DUSKPETAL_OPEN, 0);
                else if (meta == 3 && day) w.setBlockMetadataWithNotify(x, y, z, 2);
            } else if (id == DUSKPETAL_OPEN && day) {
                w.setBlockAndMetadataWithNotify(x, y, z, AX_FLOWERS, 2);
            }
            break;
        }
        default: break;
        }
        return;
    }
    switch (BP[id].cls) {
    case BC::Grass:
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
    case BC::Leaves: leavesTick(w, x, y, z, id); break;
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
    case BC::Frozen:
        if (id == COMPACTED_ICE) freezeNeighbours(w, x, y, z);
        break;
    case BC::Fire: fireTick(w, x, y, z, r); break;
    case BC::MagicLight: { // BlockMagicLight: ages every 30 s (and on random ticks), gone at 15
        int age = w.metaAt(x, y, z) + 1;
        if (age >= 15) w.setBlockWithNotify(x, y, z, 0);
        else {
            w.setBlockMetadataWithNotify(x, y, z, age);
            w.scheduleBlockUpdate(x, y, z, id);
        }
        break;
    }
    default: break;
    }
}

bool canPlaceBlockAt(World& w, int id, int x, int y, int z) {
    switch (BP[id].cls) {
    case BC::Cactus: {
        int cur = w.blockAt(x, y, z);
        if (!(cur == 0 || matLiquid(BP[cur].mat))) return false;
        return cactusStay(w, x, y, z);
    }
    case BC::Reed: return reedPlace(w, x, y, z);
    case BC::Snow: return snowPlace(w, x, y, z);
    case BC::Pumpkin: {
        int cur = w.blockAt(x, y, z);
        return (cur == 0 || matLiquid(BP[cur].mat)) && w.isBlockOpaqueCube(x, y - 1, z);
    }
    case BC::Cattail: return w.blockAt(x, y, z) == 0 && w.blockAt(x, y + 1, z) == 0 && cattailSpot(w, x, y, z);
    case BC::Torch: case BC::RedstoneTorch: case BC::LapisTorch: case BC::Lever: // a solid side or floor
        return w.isBlockOpaqueCube(x - 1, y, z) || w.isBlockOpaqueCube(x + 1, y, z) || w.isBlockOpaqueCube(x, y, z - 1) ||
               w.isBlockOpaqueCube(x, y, z + 1) || w.isBlockOpaqueCube(x, y - 1, z);
    case BC::Ladder: case BC::Button: // a solid side
        return w.isBlockOpaqueCube(x - 1, y, z) || w.isBlockOpaqueCube(x + 1, y, z) || w.isBlockOpaqueCube(x, y, z - 1) ||
               w.isBlockOpaqueCube(x, y, z + 1);
    case BC::PressurePlate: case BC::RedstoneWire: case BC::LapisWire:
        return w.isBlockOpaqueCube(x, y - 1, z);
    case BC::MinecartTrack: return rails::canPlace(w, x, y, z);
    case BC::Cake: { // BlockCake: on something solid
        int cur = w.blockAt(x, y, z);
        return (cur == 0 || matLiquid(BP[cur].mat)) && matSolid(w.materialAt(x, y - 1, z));
    }
    default:
        if (isPlant(id)) return growsOn(id, w.blockAt(x, y - 1, z)); // BlockFlower.canPlaceBlockAt
        {
            int cur = w.blockAt(x, y, z);
            return cur == 0 || matLiquid(BP[cur].mat);
        }
    }
}

bool canBlockStay(World& w, int id, int x, int y, int z) {
    if (isPlant(id)) return plantStay(w, id, x, y, z);
    switch (BP[id].cls) {
    case BC::Cactus: return cactusStay(w, x, y, z);
    case BC::Reed: return reedPlace(w, x, y, z);
    case BC::Icicle: {
        bool rising = (w.metaAt(x, y, z) & 1) == 1;
        return rising ? icicleHolds(w, x, y - 1, z, true) : icicleHolds(w, x, y + 1, z, false);
    }
    default: return true;
    }
}
} // namespace Blocks
