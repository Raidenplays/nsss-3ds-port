#pragma once
// One world folder, laid out like the NSSS jar's NSSSsaves/<World>/:
//   level.dat (gzip NBT, "Data" compound), level.dat_old / _new backups, name.data (the world's name)
//   region/r.X.Z.mcr        chunks (ChunkLoader through RegionFileCache: each one a gzipped NBT,
//                           deflated again by the region file - exactly what the jar writes)
//   <b36(x&63)>/<b36(z&63)>/c.<b36 x>.<b36 z>.dat   Alpha's chunk files (read; saved into the region)
#include <string>
#include "Chunk.h"
#include "RegionFile.h"

struct LevelInfo {
    int64_t seed = 0;
    std::string name = "New World"; // name.data (LevelName)
    int spawnX = 0, spawnY = 64, spawnZ = 0;
    int64_t time = 0;
    int64_t sizeOnDisk = 0;
    bool hasPlayer = false;
    double playerX = 0, playerY = 0, playerZ = 0;
    float playerYaw = 0, playerPitch = 0;
    // GuiNameWorld's choices (World.season / weather: EnumSeason / EnumWeather order)
    int season = 4, weather = 3; // cycle, seasonal
    bool snowCovered = false, cheatsDisabled = false, experiencedFirstSnow = false;
    int worldType = 0;
};

class SaveHandler {
public:
    // worldDir: e.g. "sdmc:/3ds/nsss3ds/NSSSsaves/World1"
    explicit SaveHandler(const std::string& worldDir);

    bool readLevel(LevelInfo& out);  // the World constructor's level.dat read
    bool writeLevel(const LevelInfo& in); // World.loadWorldSaveInfo (the save)
    // A new world: its name.data and level.dat
    bool createLevel(const LevelInfo& in);

    Chunk* loadChunk(int cx, int cz);   // null if never saved
    bool chunkSaved(int cx, int cz);    // in a region file already (cheap: the region header)
    bool saveChunk(Chunk& c, int64_t worldTime);
    void flush() { regions.closeAll(); }

    const std::string& worldPath() const { return worldDir; }
    // The saved player (Data.Player): everything the jar stores there stays in this compound; the
    // game reads/updates its fields. Null before readLevel or for a world without a player.
    nbt::Tag* player() const;
    nbt::Tag* levelData() const { return levelRoot ? levelRoot->getCompound("Data") : nullptr; }

private:
    std::string worldDir;
    RegionCache regions;
    nbt::TagPtr levelRoot; // whole level.dat, kept for round-tripping

    Chunk* loadLegacyChunk(int cx, int cz);
};

// Java's Integer.toString(n, 36)
std::string toBase36(int n);
// EnumSeason / EnumWeather names, as level.dat stores them
const char* seasonName(int s);
const char* weatherName(int w);
