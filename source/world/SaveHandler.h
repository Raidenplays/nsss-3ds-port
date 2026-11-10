#pragma once
// One world folder, laid out exactly like the Java game's saves/<World>/:
//   level.dat (gzip NBT, "Data" compound), level.dat_old / _new backups
//   region/r.X.Z.mcr            overworld chunks (McRegionChunkLoader)
//   DIM-1/region/r.X.Z.mcr      Nether chunks
//   <b36(x&63)>/<b36(z&63)>/c.<b36 x>.<b36 z>.dat   pre-McRegion chunks (read only;
//                               like Axeolon, they move into the region file on save)
//   axeolon_* files             Axeolon's extras - left untouched for now
#include <string>
#include "Chunk.h"
#include "RegionFile.h"

struct LevelInfo {
    int64_t seed = 0;
    int spawnX = 0, spawnY = 64, spawnZ = 0;
    int64_t time = 0;
    bool hasPlayer = false;
    double playerX = 0, playerY = 0, playerZ = 0;
    float playerYaw = 0, playerPitch = 0;
    int dimension = 0;
    int gameType = 0; // Axeolon: 0 survival, 1 creative
};

class SaveHandler {
public:
    // worldDir: e.g. "sdmc:/3ds/axeolon3ds/saves/World1". dimension: 0 or -1.
    SaveHandler(const std::string& worldDir, int dimension);

    bool readLevel(LevelInfo& out);  // level.dat -> level.dat_new -> level.dat_old (Axeolon's recovery order)
    bool writeLevel(const LevelInfo& in);
    // A new world's level.dat, with the same entries in the same order as the mod writes them
    bool createLevel(const LevelInfo& in, int gameType); // updates the loaded compound, keeps everything else

    Chunk* loadChunk(int cx, int cz);   // null if never saved
    bool chunkSaved(int cx, int cz);    // in a region file already (cheap: the region header)
    bool saveChunk(Chunk& c, int64_t worldTime);
    void flush() { regions.closeAll(); }

    const std::string& worldPath() const { return worldDir; }
    // The saved player (Data.Player) - everything the Java game stores there (inventory, battle
    // slots, mana, Axeolon progress...) stays in this compound; the game reads/updates its fields.
    // Null before readLevel or for a world without a player.
    nbt::Tag* player() const;
    nbt::Tag* levelData() const { return levelRoot ? levelRoot->getCompound("Data") : nullptr; }

private:
    std::string worldDir, dimDir;
    int dimension;
    RegionCache regions;
    nbt::TagPtr levelRoot; // whole level.dat, kept for round-tripping

    Chunk* loadLegacyChunk(int cx, int cz);
};

// Java's Integer.toString(n, 36)
std::string toBase36(int n);
