#pragma once
// Port of Beta's World + ChunkProviderLoadOrGenerate for one dimension.
//
// Two ways in:
//   - "peek" calls (getBlockId, getChunk, getLightValue...) never load anything; the renderer,
//     physics and UI use these and see unloaded chunks as empty.
//   - the Java-named calls below (blockAt, setBlock, setBlockWithNotify, getHeightValue, the
//     lighting queue...) behave like Beta's World: touching a chunk inside the window loads it,
//     or generates and populates it, exactly like ChunkProviderLoadOrGenerate.provideChunk.
//     World generation and block behaviour (Blocks.cpp) are written against these.
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "BlockProps.h"
#include "Chunk.h"
#include "JavaRandom.h"
#include "Physics.h"
#include "SaveHandler.h"

class ChunkGenerator;

struct RayHit {
    bool hit = false;
    int x = 0, y = 0, z = 0;    // block that was hit
    int nx = 0, ny = 0, nz = 0; // face normal (where a placed block goes)
};

enum class Sky : uint8_t { Sky, Block }; // EnumSkyBlock

class World {
public:
    // save may be null for the built-in test world (nothing is ever saved). seed/worldDir drive the
    // generator for chunks that were never saved.
    World(SaveHandler* save, const LevelInfo& info, bool testTerrain);
    ~World();

    // ---- peek access (never loads) ----
    uint8_t getBlockId(int x, int y, int z) const;
    int getBlockMetadata(int x, int y, int z) const;
    int getLightValue(int x, int y, int z) const; // getBlockLightValue without the slab/farmland rule
    int getBlockLightValue(int x, int y, int z) const; // ... with it (a half slab or farmland: the brightest neighbour)
    float getBrightness(int x, int y, int z) const;    // World.getBrightness (lightBrightnessTable)
    int currentSeason() const;                         // getCurrentSeason (a CYCLE world turns every daysInSeason days)
    float mobRatio() const;                            // EnumSeason.getMobRatio
    bool setBlockAndMetadata(int x, int y, int z, uint8_t id, int meta); // player edits: ...WithNotify
    int getTopY(int x, int z) const;
    Chunk* getChunk(int cx, int cz) const {
        Chunk* c = slots[slotOf(cx, cz)];
        if (c && c->xPosition == cx && c->zPosition == cz) return c;
        return getChunkSlow(cx, cz);
    }
    bool isChunkLoaded(int cx, int cz) const { return getChunk(cx, cz) != nullptr; }

    // Loads (or generates) up to `budget` missing chunks nearest the centre and unloads (saving
    // first) chunks well outside the radius. Also moves Beta's 31x31 window. Returns the chunks
    // newly loaded (including any the generator pulled in on the way).
    std::vector<std::pair<int, int>> updateLoadedArea(int centreCx, int centreCz, int radius, int budget);
    void saveAll(); // modified chunks + level.dat
    bool canSave() const { return save != nullptr; }
    template <class F> void forEachChunk(F f) { for (auto& kv : chunks) f(*kv.second); }
    bool chunkOnDisk(int cx, int cz) { return save && save->chunkSaved(cx, cz); }
    // A saved chunk read straight from the region file, not added to the world (null if none)
    std::unique_ptr<Chunk> readSavedChunk(int cx, int cz);
    bool generates() const { return !testTerrain; }
    RayHit rayTrace(float ox, float oy, float oz, float dx, float dy, float dz, float maxDist, bool liquids = false) const;

    LevelInfo info;
    // NSSS world settings (GuiNameWorld / the /set* commands); seasons and weather are ported with
    // World.tick. Values match EnumSeason / EnumWeather order.
    enum Season { SUMMER, FALL, WINTER, SPRING, CYCLE };
    enum Weather { CLEAR, PRECIPITATION, WEATHER_CYCLE, SEASONAL };
    int season = SUMMER, weather = CLEAR;
    bool snowCovered = false, cheatsDisabled = false;
    int daysInSeason = 14;
    int worldType = 0; // 0 normal, 1 floating, 2 corruption, 3 desert, 4 old-timey
    int skylightSubtracted = 0;
    // RenderGlobal.markBlockRangeNeedsUpdate (IWorldAccess); set by the game
    std::function<void(int, int, int, int, int, int)> onBlocksDirty;
    // Terrain made ahead of time on the other CPU core (TerrainWorker): fills 65536 block ids
    // and returns true if it has (cx, cz) ready. Same result as generating it here.
    std::function<bool(int, int, uint8_t*)> pregenerated;
    // Called now and then from the middle of generation, so a long populate can be paused
    // (GenFiber) and the frame drawn; nothing else may unload chunks until it finishes.
    std::function<void()> onYieldPoint;
    std::function<void(Chunk&)> onChunkLoaded;  // every chunk that comes in (loaded or generated)
    std::function<void(Chunk&)> beforeSave;     // just before a chunk is written (entities go into its NBT)
    std::function<void(int, int)> onUnload;      // a chunk has gone
    int yieldCounter = 0;
    void yieldPoint() {
        yieldCounter = 0;
        if (onYieldPoint) onYieldPoint();
    }
    // updateLoadedArea in two halves, for when chunk loading runs on GenFiber
    void unloadFar(int centreCx, int centreCz, int keepRadius);
    std::vector<std::pair<int, int>> missingNear(int centreCx, int centreCz, int radius, size_t max) const;
    std::vector<std::pair<int, int>> justLoaded; // chunks provideChunk brought in since last updateLoadedArea

    // ---- Beta World (Java semantics) ----
    // World constructor for a brand-new world: walk from (0, 0) until the first uncovered block
    // is sand outside the snowy biomes (WorldProvider.canCoordinateBeSpawn)
    void findSpawn();
    int getFirstUncoveredBlock(int x, int z);
    bool ignoreWindow = false; // field_9430_x: provideChunk loads anywhere (spawn search)
    void setWindow(int cx, int cz) { windowX = cx; windowZ = cz; } // ChunkProviderLoadOrGenerate.func_21110_c
    bool inWindow(int cx, int cz) const { return cx >= windowX - 15 && cz >= windowZ - 15 && cx <= windowX + 15 && cz <= windowZ + 15; }
    bool chunkExists(int cx, int cz) const;
    Chunk* provideChunk(int cx, int cz); // null = Beta's blank chunk
    uint8_t blockAt(int x, int y, int z) {
        if (++yieldCounter >= 2048) yieldPoint();
        if (unsigned(y) >= 256u) return 0;
        Chunk* c = getChunk(x >> 4, z >> 4);
        if (!c || (!ignoreWindow && !inWindow(x >> 4, z >> 4))) return blockAtSlow(x, y, z);
        return c->blocks[(x & 15) << 12 | (z & 15) << 8 | y];
    }
    uint8_t blockAtSlow(int x, int y, int z);
    int metaAt(int x, int y, int z);
    Mat materialAt(int x, int y, int z) { return BP[blockAt(x, y, z)].mat; }
    bool isAirBlock(int x, int y, int z) { return blockAt(x, y, z) == 0; }
    bool isBlockOpaqueCube(int x, int y, int z) { return BP[blockAt(x, y, z)].opaqueCube; }
    bool blockExists(int x, int y, int z) const { return y >= 0 && y < 256 && chunkExists(x >> 4, z >> 4); }
    bool checkChunksExist(int x1, int y1, int z1, int x2, int y2, int z2) const;
    bool doChunksNearChunkExist(int x, int y, int z, int r) const { return checkChunksExist(x - r, y - r, z - r, x + r, y + r, z + r); }

    bool setBlock(int x, int y, int z, int id);
    bool setBlockAndMetadataJ(int x, int y, int z, int id, int meta); // World.setBlockAndMetadata
    bool setBlockMetadata(int x, int y, int z, int meta);
    void setBlockMetadataWithNotify(int x, int y, int z, int meta);
    bool setBlockWithNotify(int x, int y, int z, int id);
    bool setBlockAndMetadataWithNotify(int x, int y, int z, int id, int meta);
    void notifyBlockChange(int x, int y, int z, int id);
    void notifyBlocksOfNeighborChange(int x, int y, int z, int id);
    void markBlockNeedsUpdate(int x, int y, int z) { markBlocksDirty(x, y, z, x, y, z); }
    void markBlocksDirty(int x1, int y1, int z1, int x2, int y2, int z2) {
        if (onBlocksDirty) onBlocksDirty(x1, y1, z1, x2, y2, z2);
    }
    void markBlocksDirtyVertical(int x, int z, int y1, int y2) {
        if (y1 > y2) std::swap(y1, y2);
        markBlocksDirty(x, y1, z, x, y2, z);
    }

    bool canBlockSeeTheSky(int x, int y, int z);
    bool canExistingBlockSeeTheSky(int x, int y, int z);
    int getBlockLightValue(int x, int y, int z) { return getBlockLightValue_do(x, y, z, true); }
    int getBlockLightValue_do(int x, int y, int z, bool slabs);
    int getHeightValue(int x, int z);
    int findTopSolidBlock(int x, int z);
    int getSavedLightValue(Sky t, int x, int y, int z);
    void setLightValue(Sky t, int x, int y, int z, int v);
    void neighborLightPropagationChanged(Sky t, int x, int y, int z, int v);
    void scheduleLightingUpdate(Sky t, int x1, int y1, int z1, int x2, int y2, int z2, bool merge = true); // func_616_a / func_627_a
    bool updatingLighting(); // func_6465_g: runs up to 500 queued updates; true if more are left
    size_t lightQueueSize() const { return lightQueue.size(); }

    void scheduleBlockUpdate(int x, int y, int z, int id);
    void scheduleBlockUpdateIn(int x, int y, int z, int id, int delay);
    bool tickUpdates(bool all); // World.TickUpdates
    // World.func_4080_j: random block ticks (160 a chunk: Axeolon's chunks are twice as tall) and
    // cave ambience, for loaded chunks within 9 of the player
    void tickBlocks(double px, double py, double pz);
    std::function<void(const char*, double, double, double, float, float)> onSound; // playSoundEffect
    // World.entityJoinedWorld for a mob made during generation (its saved form): the game adopts
    // it, or else it goes into its chunk's saved list
    std::function<void(nbt::TagPtr)> onSpawnEntity;
    // AxeolonBlockAnims.play / .door and World.spawnParticle, for block code that has no renderer
    std::function<void(int kind, int x, int y, int z, const char* name)> onBlockAnim;
    std::function<void(int x, int y, int z, int metaBefore)> onDoorSwing;
    std::function<void(int type, double x, double y, double z, double a, double b, double c)> onParticle;
    // BlockPistonBase.launch: throw whatever is in the box
    std::function<void(const AABB& box, double mx, double my, double mz)> onLaunch;
    // BlockDispenser.dispenseItem (the game owns the projectiles and dropped items)
    std::function<void(int x, int y, int z)> onDispense;
    // BlockSand.tryToFall: an EntityFallingSand takes the block from here
    std::function<void(int x, int y, int z, int id)> onFallingBlock;
    void spawnEntity(nbt::TagPtr t);
    int updateLCG = 0;          // field_9437_g
    int caveSoundCooldown = 0;  // field_9426_L
    bool scheduledUpdatesAreImmediate = false;
    bool editingBlocks = false; // field_1043_h: neighbour notifications are suppressed

    // TileEntity data as saved NBT, in the chunk's "TileEntities" list
    nbt::Tag* tileEntityAt(int x, int y, int z);
    nbt::Tag* addTileEntity(int x, int y, int z, const char* id); // replaces any existing one
    void removeTileEntity(int x, int y, int z);

    JavaRandom rand; // World.rand (Java seeds it from the clock)
    // Block.dropBlockAsItem results waiting for the item-entity port: position, item id, damage
    struct Drop { double x, y, z; int id, count, damage; };
    std::vector<Drop> drops;
    ChunkGenerator* generator() { return gen.get(); }

private:
    SaveHandler* save;
    bool testTerrain;
    std::unique_ptr<ChunkGenerator> gen;
    std::unordered_map<int64_t, std::unique_ptr<Chunk>> chunks;
    // ChunkProviderLoadOrGenerate's 32x32 slot array, as a lookup cache in front of the map
    mutable Chunk* slots[32 * 32] = {};
    static int slotOf(int cx, int cz) { return (cx & 31) + (cz & 31) * 32; }
    int windowX = 0, windowZ = 0;

    struct LightUpdate {
        Sky type;
        int x1, y1, z1, x2, y2, z2;
        void run(World& w);                                         // MetadataChunkBlock.func_4127_a
        bool tryMerge(int ax, int ay, int az, int bx, int by, int bz); // func_866_a
    };
    std::vector<LightUpdate> lightQueue;
    int lightDepth = 0;  // field_4204_J
    int lightAdding = 0; // field_9429_y (static in Java)

    struct Tick {
        int x, y, z, id;
        int64_t time, serial;
        bool operator<(const Tick& o) const { return time != o.time ? time < o.time : serial < o.serial; }
    };
    std::set<Tick> tickTree;
    std::unordered_set<uint64_t> tickSet;
    int64_t tickSerial = 0;
    static uint64_t tickKey(int x, int y, int z, int id) {
        return (uint64_t(uint32_t(x)) << 38) ^ (uint64_t(uint32_t(z)) << 14) ^ (uint64_t(y & 255) << 8) ^ uint64_t(id & 255);
    }

    static int64_t key(int cx, int cz) { return int64_t(cx) << 32 ^ uint32_t(cz); }
    Chunk* getChunkSlow(int cx, int cz) const;
    Chunk* loadOrGenerate(int cx, int cz);
    void populate(int cx, int cz);
    void generateTestChunk(Chunk& c);
    void unload(int64_t k);

public:
    // ---- Chunk internals that need the world (Chunk.java) ----
    bool chunkSetBlockID(Chunk& c, int x, int y, int z, int id);
    bool chunkSetBlockIDWithMetadata(Chunk& c, int x, int y, int z, int id, int meta);
    void chunkGenerateSkylight(Chunk& c);    // func_1024_c
    void chunkGenerateBlockLight(Chunk& c);  // func_4143_d
    void chunkGenerateHeightMap(Chunk& c);   // generateHeightMap
private:
    void chunkRelight(Chunk& c, int x, int y, int z); // func_1003_g
    void chunkCheckNeighbours(Chunk& c, int x, int z); // func_996_c
    void chunkCheckColumn(Chunk& c, int x, int z, int h); // func_1020_f
};
