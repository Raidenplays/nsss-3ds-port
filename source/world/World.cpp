#include "World.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "BlockTable.h"
#include "Blocks.h"
#include "gen/ChunkGenerator.h"

namespace {
inline bool inRange(int x, int z) { return x >= -32000000 && z >= -32000000 && x < 32000000 && z <= 32000000; }
inline int opacity(int id) { return BP[id & 255].opacity; }
} // namespace

World::World(SaveHandler* s, const LevelInfo& li, bool test) : info(li), save(s), testTerrain(test) {
    rand.setSeed(int64_t(li.seed) ^ 0x2B992DDFA23249D6LL);
    if (!test) gen.reset(new ChunkGenerator(li.seed));
}

World::~World() = default;

// ================================================================ peek access

Chunk* World::getChunkSlow(int cx, int cz) const {
    auto it = chunks.find(key(cx, cz));
    if (it == chunks.end()) return nullptr;
    Chunk* c = it->second.get();
    slots[slotOf(cx, cz)] = c;
    return c;
}

uint8_t World::getBlockId(int x, int y, int z) const {
    if (y < 0 || y >= Chunk::HEIGHT) return 0;
    Chunk* c = getChunk(x >> 4, z >> 4);
    return c ? c->getBlockID(x & 15, y, z & 15) : 0;
}

int World::getBlockMetadata(int x, int y, int z) const {
    if (y < 0 || y >= Chunk::HEIGHT) return 0;
    Chunk* c = getChunk(x >> 4, z >> 4);
    return c ? c->getMetadata(x & 15, y, z & 15) : 0;
}

int World::getLightValue(int x, int y, int z) const {
    if (y < 0) return 0;
    if (y >= Chunk::HEIGHT) return std::max(0, 15 - skylightSubtracted);
    Chunk* c = getChunk(x >> 4, z >> 4);
    if (!c) return 15;
    int sky = c->getSkyLight(x & 15, y, z & 15) - skylightSubtracted;
    int blk = c->getBlockLight(x & 15, y, z & 15);
    return std::max(sky, blk); // Chunk.getBlockLightValue
}

bool World::setBlockAndMetadata(int x, int y, int z, uint8_t id, int meta) {
    if (!getChunk(x >> 4, z >> 4)) return false;
    return setBlockAndMetadataWithNotify(x, y, z, id, meta);
}

int World::getTopY(int x, int z) const {
    Chunk* c = getChunk(x >> 4, z >> 4);
    return c ? c->topY(x & 15, z & 15) : 0;
}

// ================================================================ chunk provider

bool World::chunkExists(int cx, int cz) const {
    if (!inWindow(cx, cz)) return false;
    return getChunk(cx, cz) != nullptr;
}

Chunk* World::loadOrGenerate(int cx, int cz) {
    Chunk* c = save ? save->loadChunk(cx, cz) : nullptr;
    if (c) {
        c->world = this;
        if (c->regenSkylight) {
            c->regenSkylight = false;
            std::memset(c->skylight.data, 0, sizeof c->skylight.data);
            chunkGenerateSkylight(*c);
            c->modified = true;
        }
    } else {
        c = new Chunk(cx, cz);
        c->world = this;
        if (testTerrain) {
            generateTestChunk(*c);
            c->terrainPopulated = true;
            c->modified = false;
        } else {
            // ChunkProviderGenerate.provideChunk: blocks, then func_1024_c (heightmap + skylight),
            // before the provider has stored the chunk
            if (pregenerated && pregenerated(cx, cz, c->blocks)) (void)0;
            else gen->provideChunk(cx, cz, c->blocks);
            chunkGenerateSkylight(*c);
        }
    }
    chunks[key(cx, cz)].reset(c);
    slots[slotOf(cx, cz)] = c;
    return c;
}

Chunk* World::provideChunk(int cx, int cz) {
    if (!ignoreWindow && !inWindow(cx, cz)) return nullptr; // blankChunk
    if (Chunk* c = getChunk(cx, cz)) return c;
    yieldPoint();
    Chunk* c = loadOrGenerate(cx, cz);
    justLoaded.emplace_back(cx, cz);
    if (onChunkLoaded) onChunkLoaded(*c);
    yieldPoint();
    chunkGenerateBlockLight(*c); // func_4143_d, on every chunk that comes in
    yieldPoint();
    if (testTerrain) return c;
    auto populated = [&](int x, int z) { Chunk* n = provideChunk(x, z); return !n || n->terrainPopulated; };
    if (!c->terrainPopulated && chunkExists(cx + 1, cz + 1) && chunkExists(cx, cz + 1) && chunkExists(cx + 1, cz))
        populate(cx, cz);
    if (chunkExists(cx - 1, cz) && !populated(cx - 1, cz) && chunkExists(cx - 1, cz + 1) && chunkExists(cx, cz + 1) &&
        chunkExists(cx - 1, cz))
        populate(cx - 1, cz);
    if (chunkExists(cx, cz - 1) && !populated(cx, cz - 1) && chunkExists(cx + 1, cz - 1) && chunkExists(cx, cz - 1) &&
        chunkExists(cx + 1, cz))
        populate(cx, cz - 1);
    if (chunkExists(cx - 1, cz - 1) && !populated(cx - 1, cz - 1) && chunkExists(cx - 1, cz - 1) &&
        chunkExists(cx, cz - 1) && chunkExists(cx - 1, cz))
        populate(cx - 1, cz - 1);
    return getChunk(cx, cz);
}

void World::populate(int cx, int cz) {
    Chunk* c = provideChunk(cx, cz);
    if (!c || c->terrainPopulated) return;
    c->terrainPopulated = true;
#ifdef GEN_DEBUG
    fprintf(stderr, "POP %d %d\n", cx, cz);
#endif
    gen->populate(*this, cx, cz);
    c = getChunk(cx, cz);
    if (c) c->modified = true;
}

int World::getFirstUncoveredBlock(int x, int z) {
    int y = 63;
    while (!isAirBlock(x, y + 1, z)) ++y;
    return blockAt(x, y, z);
}

void World::findSpawn() {
    ignoreWindow = true;
    info.spawnX = 0;
    info.spawnY = 64;
    int tries = 0;
    auto ok = [&](int x, int z) {
        (void)tries; // NSSS World.isThisBlockSpawnable: sand (grass in the floating/corruption/desert types)
        return getFirstUncoveredBlock(x, z) == 12;
    };
    for (info.spawnZ = 0; !ok(info.spawnX, info.spawnZ);) {
        int a = rand.nextInt(64);
        info.spawnX += a - rand.nextInt(64);
        a = rand.nextInt(64);
        info.spawnZ += a - rand.nextInt(64);
        // the 3DS can't keep every chunk the walk touches; far ones go to disk
        if (chunks.size() > 64) {
            std::vector<int64_t> far;
            for (auto& kv : chunks)
                if (std::abs(kv.second->xPosition - (info.spawnX >> 4)) > 2 || std::abs(kv.second->zPosition - (info.spawnZ >> 4)) > 2)
                    far.push_back(kv.first);
            for (int64_t k : far) unload(k);
        }
    }
    ignoreWindow = false;
}

void World::unload(int64_t k) {
    auto it = chunks.find(k);
    if (it == chunks.end()) return;
    Chunk* c = it->second.get();
    if (beforeSave) beforeSave(*c);
    if (save && c->modified) save->saveChunk(*c, info.time);
    if (onUnload) onUnload(c->xPosition, c->zPosition);
    if (slots[slotOf(c->xPosition, c->zPosition)] == c) slots[slotOf(c->xPosition, c->zPosition)] = nullptr;
    chunks.erase(it);
}

void World::unloadFar(int ccx, int ccz, int keep) {
    std::vector<int64_t> far;
    for (auto& kv : chunks) {
        Chunk* c = kv.second.get();
        if (std::abs(c->xPosition - ccx) > keep || std::abs(c->zPosition - ccz) > keep) far.push_back(kv.first);
    }
    for (int64_t k : far) unload(k);
}

std::vector<std::pair<int, int>> World::missingNear(int ccx, int ccz, int radius, size_t max) const {
    std::vector<std::pair<int, int>> out;
    for (int ring = 0; ring <= radius && out.size() < max; ring++)
        for (int dx = -ring; dx <= ring && out.size() < max; dx++)
            for (int dz = -ring; dz <= ring && out.size() < max; dz++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                if (!getChunk(ccx + dx, ccz + dz)) out.emplace_back(ccx + dx, ccz + dz);
            }
    return out;
}

std::vector<std::pair<int, int>> World::updateLoadedArea(int ccx, int ccz, int radius, int budget) {
    setWindow(ccx, ccz);
    // Unload far chunks first (saving them) to keep memory flat. Chunks just outside the radius
    // stay a while longer so populate still finds its neighbours.
    std::vector<int64_t> far;
    for (auto& kv : chunks) {
        Chunk* c = kv.second.get();
        if (std::abs(c->xPosition - ccx) > radius + 2 || std::abs(c->zPosition - ccz) > radius + 2) far.push_back(kv.first);
    }
    for (int64_t k : far) unload(k);
    int done = 0;
    for (int ring = 0; ring <= radius && done < budget; ring++)
        for (int dx = -ring; dx <= ring && done < budget; dx++)
            for (int dz = -ring; dz <= ring && done < budget; dz++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                if (getChunk(ccx + dx, ccz + dz)) continue;
                provideChunk(ccx + dx, ccz + dz);
                done++;
            }
    std::vector<std::pair<int, int>> out;
    out.swap(justLoaded);
    // only report chunks that are still here (a cascade may have unloaded nothing, but be safe)
    out.erase(std::remove_if(out.begin(), out.end(), [&](const std::pair<int, int>& p) { return !getChunk(p.first, p.second); }),
              out.end());
    return out;
}

void World::saveAll() {
    if (!save) return;
    for (auto& kv : chunks) {
        if (beforeSave) beforeSave(*kv.second);
        if (kv.second->modified) save->saveChunk(*kv.second, info.time);
    }
    save->writeLevel(info);
    save->flush();
}

// ================================================================ block access (Java World)

std::unique_ptr<Chunk> World::readSavedChunk(int cx, int cz) {
    if (!save || !save->chunkSaved(cx, cz)) return nullptr;
    return std::unique_ptr<Chunk>(save->loadChunk(cx, cz));
}

uint8_t World::blockAtSlow(int x, int y, int z) {
    if (!inRange(x, z) || y < 0 || y >= 256) return 0;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    return c ? c->getBlockID(x & 15, y, z & 15) : 0;
}

int World::metaAt(int x, int y, int z) {
    if (!inRange(x, z) || y < 0 || y >= 256) return 0;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    return c ? c->getMetadata(x & 15, y, z & 15) : 0;
}

bool World::checkChunksExist(int x1, int y1, int z1, int x2, int y2, int z2) const {
    if (y2 < 0 || y1 >= 256) return false;
    x1 >>= 4; z1 >>= 4; x2 >>= 4; z2 >>= 4;
    for (int x = x1; x <= x2; ++x)
        for (int z = z1; z <= z2; ++z)
            if (!chunkExists(x, z)) return false;
    return true;
}

bool World::setBlock(int x, int y, int z, int id) {
    if (!inRange(x, z) || y < 0 || y >= 256) return false;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    if (!c) return true; // EmptyChunk.setBlockID
    return chunkSetBlockID(*c, x & 15, y, z & 15, id);
}

bool World::setBlockAndMetadataJ(int x, int y, int z, int id, int meta) {
    if (!inRange(x, z) || y < 0 || y >= 256) return false;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    if (!c) return true;
    return chunkSetBlockIDWithMetadata(*c, x & 15, y, z & 15, id, meta);
}

bool World::setBlockMetadata(int x, int y, int z, int meta) {
    if (!inRange(x, z) || y < 0 || y >= 256) return false;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    if (c) {
        c->modified = true;
        c->data.set(x & 15, y, z & 15, meta);
    }
    return true;
}

void World::setBlockMetadataWithNotify(int x, int y, int z, int meta) {
    if (setBlockMetadata(x, y, z, meta)) notifyBlockChange(x, y, z, blockAt(x, y, z));
}

bool World::setBlockWithNotify(int x, int y, int z, int id) {
    if (!setBlock(x, y, z, id)) return false;
    notifyBlockChange(x, y, z, id);
    return true;
}

bool World::setBlockAndMetadataWithNotify(int x, int y, int z, int id, int meta) {
    if (!setBlockAndMetadataJ(x, y, z, id, meta)) return false;
    notifyBlockChange(x, y, z, id);
    return true;
}

void World::notifyBlockChange(int x, int y, int z, int id) {
    markBlockNeedsUpdate(x, y, z);
    notifyBlocksOfNeighborChange(x, y, z, id);
}

void World::notifyBlocksOfNeighborChange(int x, int y, int z, int id) {
    if (editingBlocks) return;
    static const int D[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    for (auto& d : D) {
        int nx = x + d[0], ny = y + d[1], nz = z + d[2];
        int nid = blockAt(nx, ny, nz);
        if (nid != 0 && BP[nid].cls != BC::None) Blocks::onNeighborBlockChange(*this, nx, ny, nz, nid, id);
    }
}

bool World::canBlockSeeTheSky(int x, int y, int z) {
    Chunk* c = provideChunk(x >> 4, z >> 4);
    return c ? y >= c->heightMap[(z & 15) << 4 | (x & 15)] : false;
}

bool World::canExistingBlockSeeTheSky(int x, int y, int z) {
    if (!inRange(x, z)) return false;
    if (y < 0) return false;
    if (y >= 256) return true;
    if (!chunkExists(x >> 4, z >> 4)) return false;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    return y >= c->heightMap[(z & 15) << 4 | (x & 15)];
}

int World::getBlockLightValue_do(int x, int y, int z, bool slabs) {
    if (!inRange(x, z)) return 15;
    if (slabs) {
        int id = blockAt(x, y, z);
        if (id == 44 || id == 60) { // stairSingle, tilledField
            int v = getBlockLightValue_do(x, y + 1, z, false);
            v = std::max(v, getBlockLightValue_do(x + 1, y, z, false));
            v = std::max(v, getBlockLightValue_do(x - 1, y, z, false));
            v = std::max(v, getBlockLightValue_do(x, y, z + 1, false));
            v = std::max(v, getBlockLightValue_do(x, y, z - 1, false));
            return v;
        }
    }
    if (y < 0) return 0;
    if (y >= 256) return std::max(0, 15 - skylightSubtracted);
    Chunk* c = provideChunk(x >> 4, z >> 4);
    if (!c) return 0;
    int s = c->skylight.get(x & 15, y, z & 15) - skylightSubtracted;
    int b = c->blocklight.get(x & 15, y, z & 15);
    return b > s ? b : s;
}

int World::getHeightValue(int x, int z) {
    if (!inRange(x, z)) return 0;
    if (!chunkExists(x >> 4, z >> 4)) return 0;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    return c->heightMap[(z & 15) << 4 | (x & 15)];
}

int World::findTopSolidBlock(int x, int z) {
    Chunk* c = provideChunk(x >> 4, z >> 4);
    int y = 255;
    while (matSolid(materialAt(x, y, z)) && y > 0) --y;
    if (!c) return -1;
    for (; y > 0; --y) {
        int id = c->getBlockID(x & 15, y, z & 15);
        if (id != 0 && (matSolid(BP[id].mat) || matLiquid(BP[id].mat))) return y + 1;
    }
    return -1;
}

int World::getSavedLightValue(Sky t, int x, int y, int z) {
    if (y >= 0 && y < 256 && inRange(x, z)) {
        if (!chunkExists(x >> 4, z >> 4)) return 0;
        Chunk* c = provideChunk(x >> 4, z >> 4);
        return t == Sky::Sky ? c->skylight.get(x & 15, y, z & 15) : c->blocklight.get(x & 15, y, z & 15);
    }
    return t == Sky::Sky ? 15 : 0; // EnumSkyBlock.field_1722_c
}

void World::setLightValue(Sky t, int x, int y, int z, int v) {
    if (!inRange(x, z) || y < 0 || y >= 256 || !chunkExists(x >> 4, z >> 4)) return;
    Chunk* c = provideChunk(x >> 4, z >> 4);
    c->modified = true;
    if (t == Sky::Sky) c->skylight.set(x & 15, y, z & 15, v);
    else c->blocklight.set(x & 15, y, z & 15, v);
    markBlockNeedsUpdate(x, y, z);
}

void World::neighborLightPropagationChanged(Sky t, int x, int y, int z, int v) {
    if (!blockExists(x, y, z)) return;
    if (t == Sky::Sky) {
        if (canExistingBlockSeeTheSky(x, y, z)) v = 15;
    } else {
        int l = BP[blockAt(x, y, z)].light;
        if (l > v) v = l;
    }
    if (getSavedLightValue(t, x, y, z) != v) scheduleLightingUpdate(t, x, y, z, x, y, z);
}

// ================================================================ lighting queue

void World::scheduleLightingUpdate(Sky t, int x1, int y1, int z1, int x2, int y2, int z2, bool merge) {
    ++lightAdding;
    if (lightAdding == 50) {
        --lightAdding;
        return;
    }
    int mx = (x2 + x1) / 2, mz = (z2 + z1) / 2;
    if (!blockExists(mx, 64, mz)) {
        --lightAdding;
        return;
    }
    if (merge) {
        int n = std::min<int>(5, int(lightQueue.size()));
        for (int i = 0; i < n; ++i) {
            LightUpdate& u = lightQueue[lightQueue.size() - i - 1];
            if (u.type == t && u.tryMerge(x1, y1, z1, x2, y2, z2)) {
                --lightAdding;
                return;
            }
        }
    }
    lightQueue.push_back(LightUpdate{t, x1, y1, z1, x2, y2, z2});
    if (lightQueue.size() > 1000000) { // "More than 1000000 updates, aborting lighting updates"
#ifdef GEN_DEBUG
        fprintf(stderr, "More than 1000000 updates, aborting lighting updates\n");
#endif
        lightQueue.clear();
    }
    --lightAdding;
}

bool World::updatingLighting() {
    if (lightDepth >= 50) return false;
    ++lightDepth;
    int budget = 500;
    bool more = false;
    while (!lightQueue.empty()) {
        if (--budget <= 0) {
            more = true;
            break;
        }
        LightUpdate u = lightQueue.back();
        lightQueue.pop_back();
        u.run(*this);
    }
    --lightDepth;
    return more;
}

bool World::LightUpdate::tryMerge(int ax, int ay, int az, int bx, int by, int bz) {
    if (ax >= x1 && ay >= y1 && az >= z1 && bx <= x2 && by <= y2 && bz <= z2) return true;
    const int g = 1;
    if (ax >= x1 - g && ay >= y1 - g && az >= z1 - g && bx <= x2 + g && by <= y2 + g && bz <= z2 + g) {
        int ox = x2 - x1, oy = y2 - y1, oz = z2 - z1;
        if (ax > x1) ax = x1;
        if (ay > y1) ay = y1;
        if (az > z1) az = z1;
        if (bx < x2) bx = x2;
        if (by < y2) by = y2;
        if (bz < z2) bz = z2;
        int nx = bx - ax, ny = by - ay, nz = bz - az;
        if (nx * ny * nz - ox * oy * oz <= 2) {
            x1 = ax; y1 = ay; z1 = az; x2 = bx; y2 = by; z2 = bz;
            return true;
        }
    }
    return false;
}

void World::LightUpdate::run(World& w) {
    int sx = x2 - x1 + 1, sy = y2 - y1 + 1, sz = z2 - z1 + 1;
    if (sx * sy * sz > 65536) return; // "Light too large, skipping!"
    int lastCx = 0, lastCz = 0;
    bool haveLast = false, lastOk = false;
    for (int x = x1; x <= x2; ++x)
        for (int z = z1; z <= z2; ++z) {
            int cx = x >> 4, cz = z >> 4;
            bool ok;
            if (haveLast && cx == lastCx && cz == lastCz) ok = lastOk;
            else {
                ok = w.doChunksNearChunkExist(x, 0, z, 1);
                lastOk = ok;
                lastCx = cx;
                lastCz = cz;
                haveLast = true;
            }
            if (!ok) continue;
            if (y1 < 0) y1 = 0;
            if (y2 >= 256) y2 = 255;
            // Same reads as getSavedLightValue / blockAt / canExistingBlockSeeTheSky, with the
            // chunks of this column and its four neighbours looked up once (nothing in here can
            // load a chunk, so the pointers stay valid)
            auto existing = [&](int bx, int bz) -> Chunk* { return w.chunkExists(bx >> 4, bz >> 4) ? w.getChunk(bx >> 4, bz >> 4) : nullptr; };
            Chunk* c0 = existing(x, z);
            Chunk* cw = existing(x - 1, z);
            Chunk* ce = existing(x + 1, z);
            Chunk* cn = existing(x, z - 1);
            Chunk* cs = existing(x, z + 1);
            const bool sky = type == Sky::Sky;
            const int outside = sky ? 15 : 0;
            auto saved = [&](Chunk* c, int bx, int by, int bz) -> int {
                if (by < 0 || by >= 256) return outside;
                if (!c) return 0;
                return sky ? c->skylight.get(bx & 15, by, bz & 15) : c->blocklight.get(bx & 15, by, bz & 15);
            };
            const int lx = x & 15, lz = z & 15;
            const int height = c0 ? c0->heightMap[lz << 4 | lx] : 256;
            for (int y = y1; y <= y2; ++y) {
                int cur = saved(c0, x, y, z);
                int id = c0 ? c0->blocks[lx << 12 | lz << 8 | y] : 0;
                int op = opacity(id);
                if (op == 0) op = 1;
                int src = 0;
                if (sky) {
                    if (c0 && y >= height) src = 15; // canExistingBlockSeeTheSky
                } else {
                    src = BP[id].light;
                }
                int v;
                if (op >= 15 && src == 0) {
                    v = 0;
                } else {
                    int a = saved(cw, x - 1, y, z);
                    int b = saved(ce, x + 1, y, z);
                    int c = saved(c0, x, y - 1, z);
                    int d = saved(c0, x, y + 1, z);
                    int e = saved(cn, x, y, z - 1);
                    int f = saved(cs, x, y, z + 1);
                    v = a;
                    if (b > v) v = b;
                    if (c > v) v = c;
                    if (d > v) v = d;
                    if (e > v) v = e;
                    if (f > v) v = f;
                    v -= op;
                    if (v < 0) v = 0;
                    if (src > v) v = src;
                }
                if (cur != v) {
                    w.setLightValue(type, x, y, z, v);
                    int n = v - 1;
                    if (n < 0) n = 0;
                    w.neighborLightPropagationChanged(type, x - 1, y, z, n);
                    w.neighborLightPropagationChanged(type, x, y - 1, z, n);
                    w.neighborLightPropagationChanged(type, x, y, z - 1, n);
                    if (x + 1 >= x2) w.neighborLightPropagationChanged(type, x + 1, y, z, n);
                    if (y + 1 >= y2) w.neighborLightPropagationChanged(type, x, y + 1, z, n);
                    if (z + 1 >= z2) w.neighborLightPropagationChanged(type, x, y, z + 1, n);
                }
            }
        }
}

// ================================================================ scheduled ticks

void World::scheduleBlockUpdate(int x, int y, int z, int id) {
    const int R = 8;
    if (scheduledUpdatesAreImmediate) {
        if (checkChunksExist(x - R, y - R, z - R, x + R, y + R, z + R)) {
            int cur = blockAt(x, y, z);
            if (cur == id && cur > 0) Blocks::updateTick(*this, x, y, z, cur, rand);
        }
        return;
    }
    if (!checkChunksExist(x - R, y - R, z - R, x + R, y + R, z + R)) return;
    uint64_t k = tickKey(x, y, z, id);
    if (tickSet.count(k)) return;
    Tick t{x, y, z, id, id > 0 ? int64_t(BP[id].tickRate) + info.time : 0, tickSerial++};
    tickSet.insert(k);
    tickTree.insert(t);
}

void World::scheduleBlockUpdateIn(int x, int y, int z, int id, int delay) {
    if (scheduledUpdatesAreImmediate) {
        scheduleBlockUpdate(x, y, z, id);
        return;
    }
    if (!checkChunksExist(x - 8, y - 8, z - 8, x + 8, y + 8, z + 8)) return;
    uint64_t k = tickKey(x, y, z, id);
    if (tickSet.count(k)) return;
    tickSet.insert(k);
    tickTree.insert(Tick{x, y, z, id, int64_t(delay) + info.time, tickSerial++});
}

void World::spawnEntity(nbt::TagPtr t) {
    if (onSpawnEntity) {
        onSpawnEntity(std::move(t));
        return;
    }
    nbt::Tag* pos = t->getList("Pos");
    if (!pos || pos->items.size() < 3) return;
    int cx = int(std::floor(pos->items[0]->d)) >> 4, cz = int(std::floor(pos->items[2]->d)) >> 4;
    Chunk* c = provideChunk(cx, cz);
    if (!c) return;
    if (!c->level) c->level = nbt::makeCompound();
    nbt::Tag* list = c->level->getList("Entities");
    if (!list) {
        c->level->set("Entities", nbt::makeList(nbt::Compound));
        list = c->level->getList("Entities");
    }
    list->items.push_back(std::move(t));
    c->modified = true;
}

void World::tickBlocks(double px, double py, double pz) {
    int pcx = int(std::floor(px / 16.0)), pcz = int(std::floor(pz / 16.0));
    if (caveSoundCooldown > 0) --caveSoundCooldown;
    for (int dx = -9; dx <= 9; ++dx)
        for (int dz = -9; dz <= 9; ++dz) {
            Chunk* c = getChunk(pcx + dx, pcz + dz);
            if (!c) continue; // Java would load it; here only what's loaded ticks
            // ...and only with all its neighbours loaded too: a grass block spreading or a sapling
            // growing over the edge must never make a chunk load (or generate) in the middle of a tick
            bool ring = true;
            for (int ax = -1; ax <= 1 && ring; ++ax)
                for (int az = -1; az <= 1; ++az)
                    if (!getChunk(pcx + dx + ax, pcz + dz + az)) { ring = false; break; }
            if (!ring) continue;
            int bx = (pcx + dx) * 16, bz = (pcz + dz) * 16;
            if (caveSoundCooldown == 0) {
                updateLCG = int(uint32_t(updateLCG) * 3u + 1013904223u);
                int v = updateLCG >> 2;
                int x = v & 15, z = v >> 8 & 15, y = v >> 16 & 255;
                int id = c->getBlockID(x, y, z);
                x += bx;
                z += bz;
                if (id == 0 && getBlockLightValue(x, y, z) <= rand.nextInt(8) && getSavedLightValue(Sky::Sky, x, y, z) <= 0) {
                    double ex = x + 0.5 - px, ey = y + 0.5 - py, ez = z + 0.5 - pz;
                    double d2 = ex * ex + ey * ey + ez * ez;
                    if (d2 <= 64.0 && d2 > 4.0) {
                        if (onSound) onSound("ambient.cave.cave", x + 0.5, y + 0.5, z + 0.5, 0.7f, 0.8f + rand.nextFloat() * 0.2f);
                        caveSoundCooldown = rand.nextInt(12000) + 6000;
                    }
                }
            }
            for (int i = 0; i < 160; ++i) {
                updateLCG = int(uint32_t(updateLCG) * 3u + 1013904223u);
                int v = updateLCG >> 2;
                int x = v & 15, z = v >> 8 & 15, y = v >> 16 & 255;
                int id = c->blocks[x << 12 | z << 8 | y];
                if (BP[id].tickOnLoad) Blocks::updateTick(*this, x + bx, y, z + bz, id, rand);
            }
        }
}

bool World::tickUpdates(bool all) {
    int n = std::min<int>(1000, int(tickTree.size()));
    for (int i = 0; i < n; ++i) {
        auto it = tickTree.begin();
        if (!all && it->time > info.time) break;
        Tick t = *it;
        tickTree.erase(it);
        tickSet.erase(tickKey(t.x, t.y, t.z, t.id));
        const int R = 8;
        if (checkChunksExist(t.x - R, t.y - R, t.z - R, t.x + R, t.y + R, t.z + R)) {
            int cur = blockAt(t.x, t.y, t.z);
            if (cur == t.id && cur > 0) Blocks::updateTick(*this, t.x, t.y, t.z, cur, rand);
        }
    }
    return !tickTree.empty();
}

// ================================================================ tile entities

namespace {
nbt::Tag* teList(Chunk& c) {
    if (!c.level) c.level = nbt::makeCompound();
    nbt::Tag* l = c.level->getList("TileEntities");
    if (!l) {
        c.level->set("TileEntities", nbt::makeList(nbt::Compound));
        l = c.level->getList("TileEntities");
    }
    return l;
}
} // namespace

nbt::Tag* World::tileEntityAt(int x, int y, int z) {
    Chunk* c = provideChunk(x >> 4, z >> 4);
    if (!c) return nullptr;
    nbt::Tag* l = teList(*c);
    for (auto& t : l->items)
        if (t->getInt("x") == x && t->getInt("y") == y && t->getInt("z") == z) return t.get();
    return nullptr;
}

nbt::Tag* World::addTileEntity(int x, int y, int z, const char* id) {
    Chunk* c = provideChunk(x >> 4, z >> 4);
    if (!c) return nullptr;
    removeTileEntity(x, y, z);
    nbt::Tag* l = teList(*c);
    nbt::TagPtr t = nbt::makeCompound();
    t->setString("id", id);
    t->setInt("x", x);
    t->setInt("y", y);
    t->setInt("z", z);
    l->listType = nbt::Compound;
    l->items.push_back(std::move(t));
    c->modified = true;
    return l->items.back().get();
}

void World::removeTileEntity(int x, int y, int z) {
    Chunk* c = getChunk(x >> 4, z >> 4);
    if (!c || !c->level) return;
    nbt::Tag* l = c->level->getList("TileEntities");
    if (!l) return;
    auto& v = l->items;
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const nbt::TagPtr& t) { return t->getInt("x") == x && t->getInt("y") == y && t->getInt("z") == z; }),
            v.end());
}

// ================================================================ Chunk.java internals

void World::chunkGenerateHeightMap(Chunk& c) {
    int low = 255;
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            int h = 255;
            int col = x << 12 | z << 8;
            while (h > 0 && opacity(c.blocks[col + h - 1]) == 0) --h;
            c.heightMap[z << 4 | x] = uint8_t(std::min(h, 255));
            if (h < low) low = h;
        }
    c.lowestHeight = low;
    c.modified = true;
}

void World::chunkGenerateSkylight(Chunk& c) {
    int low = 255;
    for (int x = 0; x < 16; ++x, yieldPoint())
        for (int z = 0; z < 16; ++z) {
            int h = 255;
            int col = x << 12 | z << 8;
            while (h > 0 && opacity(c.blocks[col + h - 1]) == 0) --h;
            c.heightMap[z << 4 | x] = uint8_t(std::min(h, 255));
            if (h < low) low = h;
            int light = 15, y = 255;
            do {
                light -= opacity(c.blocks[col + y]);
                if (light > 0) c.skylight.set(x, y, z, light);
                --y;
            } while (y > 0 && light > 0);
        }
    c.lowestHeight = low;
    for (int x = 0; x < 16; ++x, yieldPoint())
        for (int z = 0; z < 16; ++z) chunkCheckNeighbours(c, x, z);
    c.modified = true;
}

void World::chunkGenerateBlockLight(Chunk& c) {
    const int Y0 = 32;
    for (int x = 0; x < 16; ++x, yieldPoint())
        for (int z = 0; z < 16; ++z) {
            int col = x << 12 | z << 8;
            for (int y = 0; y < 256; ++y) {
                int l = BP[c.blocks[col + y]].light;
                if (l > 0) c.blocklight.set(x, y, z, l);
            }
            int light = 15;
            for (int y = Y0 - 2; y < 256 && light > 0; c.blocklight.set(x, y, z, light)) {
                ++y;
                int id = c.blocks[col + y];
                int op = opacity(id), lv = BP[id].light;
                if (op == 0) op = 1;
                light -= op;
                if (lv > light) light = lv;
                if (light < 0) light = 0;
            }
        }
    scheduleLightingUpdate(Sky::Block, c.xPosition * 16, Y0 - 1, c.zPosition * 16, c.xPosition * 16 + 16, Y0 + 1,
                           c.zPosition * 16 + 16);
    c.modified = true;
}

void World::chunkCheckNeighbours(Chunk& c, int x, int z) {
    int h = c.heightMap[z << 4 | x];
    int wx = c.xPosition * 16 + x, wz = c.zPosition * 16 + z;
    chunkCheckColumn(c, wx - 1, wz, h);
    chunkCheckColumn(c, wx + 1, wz, h);
    chunkCheckColumn(c, wx, wz - 1, h);
    chunkCheckColumn(c, wx, wz + 1, h);
}

void World::chunkCheckColumn(Chunk& c, int x, int z, int h) {
    int n = getHeightValue(x, z);
    if (n > h) {
        scheduleLightingUpdate(Sky::Sky, x, h, z, x, n, z);
        c.modified = true;
    } else if (n < h) {
        scheduleLightingUpdate(Sky::Sky, x, n, z, x, h, z);
        c.modified = true;
    }
}

void World::chunkRelight(Chunk& c, int x, int y, int z) {
    int old = c.heightMap[z << 4 | x];
    int h = old;
    if (y > old) h = y;
    int col = x << 12 | z << 8;
    while (h > 0 && opacity(c.blocks[col + h - 1]) == 0) --h;
    if (h == old) return;
    markBlocksDirtyVertical(x, z, h, old);
    c.heightMap[z << 4 | x] = uint8_t(std::min(h, 255));
    if (h < c.lowestHeight) c.lowestHeight = h;
    else {
        int low = 255;
        for (int i = 0; i < 16; ++i)
            for (int k = 0; k < 16; ++k) low = std::min<int>(low, c.heightMap[k << 4 | i]);
        c.lowestHeight = low;
    }
    int wx = c.xPosition * 16 + x, wz = c.zPosition * 16 + z;
    if (h < old) {
        for (int i = h; i < old; ++i) c.skylight.set(x, i, z, 15);
    } else {
        scheduleLightingUpdate(Sky::Sky, wx, old, wz, wx, h, wz);
        for (int i = old; i < h; ++i) c.skylight.set(x, i, z, 0);
    }
    int light = 15;
    int top = h;
    for (; h > 0 && light > 0; c.skylight.set(x, h, z, light)) {
        --h;
        int op = opacity(c.getBlockID(x, h, z));
        if (op == 0) op = 1;
        light -= op;
        if (light < 0) light = 0;
    }
    while (h > 0 && opacity(c.getBlockID(x, h - 1, z)) == 0) --h;
    if (h != top) scheduleLightingUpdate(Sky::Sky, wx - 1, h, wz - 1, wx + 1, top, wz + 1);
    c.modified = true;
}

bool World::chunkSetBlockIDWithMetadata(Chunk& c, int x, int y, int z, int id, int meta) {
    if (++yieldCounter >= 256) yieldPoint();
    int h = c.heightMap[z << 4 | x];
    int old = c.getBlockID(x, y, z);
    if (old == (id & 255) && c.data.get(x, y, z) == meta) return false; // Java compares var8 == var4 (int)
    int wx = c.xPosition * 16 + x, wz = c.zPosition * 16 + z;
    c.blocks[Chunk::index(x, y, z)] = uint8_t(id);
    if (old != 0) Blocks::onBlockRemoval(*this, wx, y, wz, old);
    c.data.set(x, y, z, meta);
    {
        if (opacity(id) != 0) {
            if (y >= h) chunkRelight(c, x, y + 1, z);
        } else if (y == h - 1) {
            chunkRelight(c, x, y, z);
        }
        scheduleLightingUpdate(Sky::Sky, wx, y, wz, wx, y, wz);
    }
    scheduleLightingUpdate(Sky::Block, wx, y, wz, wx, y, wz);
    chunkCheckNeighbours(c, x, z);
    c.data.set(x, y, z, meta);
    if (id != 0) Blocks::onBlockAdded(*this, wx, y, wz, id & 255);
    c.modified = true;
    return true;
}

bool World::chunkSetBlockID(Chunk& c, int x, int y, int z, int id) {
    if (++yieldCounter >= 256) yieldPoint();
    int h = c.heightMap[z << 4 | x];
    int old = c.getBlockID(x, y, z);
    if (old == id) return false;
    int wx = c.xPosition * 16 + x, wz = c.zPosition * 16 + z;
    c.blocks[Chunk::index(x, y, z)] = uint8_t(id);
    if (old != 0) Blocks::onBlockRemoval(*this, wx, y, wz, old);
    c.data.set(x, y, z, 0);
    {
        if (opacity(id) != 0) {
            if (y >= h) chunkRelight(c, x, y + 1, z);
        } else if (y == h - 1) {
            chunkRelight(c, x, y, z);
        }
        scheduleLightingUpdate(Sky::Sky, wx, y, wz, wx, y, wz);
    }
    scheduleLightingUpdate(Sky::Block, wx, y, wz, wx, y, wz);
    chunkCheckNeighbours(c, x, z);
    if (id != 0) Blocks::onBlockAdded(*this, wx, y, wz, id & 255);
    c.modified = true;
    return true;
}

// ================================================================ ray trace

RayHit World::rayTrace(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, bool liquids) const {
    RayHit r;
    int x = int(std::floor(ox)), y = int(std::floor(oy)), z = int(std::floor(oz));
    int stepX = dx > 0 ? 1 : -1, stepY = dy > 0 ? 1 : -1, stepZ = dz > 0 ? 1 : -1;
    const float INF = 1e30f;
    float tDeltaX = dx != 0 ? std::fabs(1.0f / dx) : INF;
    float tDeltaY = dy != 0 ? std::fabs(1.0f / dy) : INF;
    float tDeltaZ = dz != 0 ? std::fabs(1.0f / dz) : INF;
    float tMaxX = dx != 0 ? ((dx > 0 ? (x + 1 - ox) : (ox - x)) * tDeltaX) : INF;
    float tMaxY = dy != 0 ? ((dy > 0 ? (y + 1 - oy) : (oy - y)) * tDeltaY) : INF;
    float tMaxZ = dz != 0 ? ((dz > 0 ? (z + 1 - oz) : (oz - z)) * tDeltaZ) : INF;
    int nx = 0, ny = 0, nz = 0;
    float t = 0;
    while (t <= maxDist) {
        uint8_t id = getBlockId(x, y, z);
        // TODO(port): Block.collisionRayTrace for non-cube shapes and canCollideCheck for liquids
        // BlockFluids.canCollideCheck: only still sources, and only when asked (rayTraceBlocks_do(.., true))
        bool liquidHit = liquids && (BLOCKS[id].flags & BF_LIQUID) && getBlockMetadata(x, y, z) == 0;
        if (id != 0 && id != 111 && (liquidHit || !(BLOCKS[id].flags & BF_LIQUID))) { // magic light: isCollidable false
            r.hit = true; r.x = x; r.y = y; r.z = z; r.nx = nx; r.ny = ny; r.nz = nz;
            return r;
        }
        if (tMaxX < tMaxY && tMaxX < tMaxZ) { x += stepX; t = tMaxX; tMaxX += tDeltaX; nx = -stepX; ny = 0; nz = 0; }
        else if (tMaxY < tMaxZ) { y += stepY; t = tMaxY; tMaxY += tDeltaY; nx = 0; ny = -stepY; nz = 0; }
        else { z += stepZ; t = tMaxZ; tMaxZ += tDeltaZ; nx = 0; ny = 0; nz = -stepZ; }
    }
    return r;
}

// Built-in test terrain (no save)
void World::generateTestChunk(Chunk& c) {
    JavaRandom r(int64_t(c.xPosition) * 341873128712LL + int64_t(c.zPosition) * 132897987541LL + info.seed);
    for (int lx = 0; lx < 16; lx++)
        for (int lz = 0; lz < 16; lz++) {
            float wx = float(c.xPosition * 16 + lx), wz = float(c.zPosition * 16 + lz);
            int h = 64 + int(6.0f * std::sin(wx * 0.09f) * std::cos(wz * 0.07f) + 3.0f * std::sin((wx + wz) * 0.21f));
            for (int y = 0; y <= h; y++) {
                uint8_t id = y == 0 ? 7 : y < h - 3 ? (r.nextInt(100) < 2 ? 16 : 1) : y < h ? 3 : 2;
                c.blocks[Chunk::index(lx, y, lz)] = id;
            }
            for (int y = h + 1; y < Chunk::HEIGHT; y++) c.skylight.set(lx, y, lz, 15);
        }
}

int World::getBlockLightValue(int x, int y, int z) const {
    int id = getBlockId(x, y, z);
    if (id == 44 || id == 60) { // Block.useNeighborBrightness: stepSingle, tilledField
        int l = getLightValue(x, y + 1, z);
        l = std::max(l, getLightValue(x + 1, y, z));
        l = std::max(l, getLightValue(x - 1, y, z));
        l = std::max(l, getLightValue(x, y, z + 1));
        l = std::max(l, getLightValue(x, y, z - 1));
        return l;
    }
    return getLightValue(x, y, z);
}

float World::getBrightness(int x, int y, int z) const {
    static float table[16];
    static bool made = false;
    if (!made) { // World's static lightBrightnessTable (f = 0.05)
        for (int i = 0; i <= 15; ++i) {
            float f1 = 1.0f - float(i) / 15.0f;
            table[i] = (1.0f - f1) / (f1 * 3.0f + 1.0f) * (1.0f - 0.05f) + 0.05f;
        }
        made = true;
    }
    return table[getBlockLightValue(x, y, z) & 15];
}

int World::currentSeason() const {
    if (season != CYCLE) return season;
    int64_t span = int64_t(24000) * daysInSeason;
    return int(info.time % (span * 4) / span);
}

float World::seasonProgress() const {
    int timeOfSeason = int(info.time % (int64_t(24000) * daysInSeason) / 24000);
    return float(timeOfSeason) / float(daysInSeason);
}

float World::mobRatio() const {
    static const float RATIO[4] = {1.0f, 0.6f, 0.2f, 1.5f}; // summer, fall, winter, spring
    int s = currentSeason();
    return s >= 0 && s < 4 ? RATIO[s] : 1.0f;
}
