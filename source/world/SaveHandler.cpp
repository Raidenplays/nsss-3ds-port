#include "SaveHandler.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

std::string toBase36(int n) {
    if (n == 0) return "0";
    bool neg = n < 0;
    int64_t v = neg ? -int64_t(n) : n;
    std::string s;
    while (v > 0) { int d = int(v % 36); s.insert(s.begin(), char(d < 10 ? '0' + d : 'a' + d - 10)); v /= 36; }
    return neg ? "-" + s : s;
}

const char* seasonName(int s) {
    static const char* N[5] = {"summer", "fall", "winter", "spring", "cycle"};
    return s >= 0 && s < 5 ? N[s] : "cycle";
}
const char* weatherName(int w) {
    static const char* N[4] = {"clear", "precipitation", "cycle", "seasonal"};
    return w >= 0 && w < 4 ? N[w] : "seasonal";
}

SaveHandler::SaveHandler(const std::string& dir) : worldDir(dir), regions(dir) {}

bool SaveHandler::readLevel(LevelInfo& out) {
    // name.data: the world's name (the first line, NULs dropped)
    if (FILE* f = fopen((worldDir + "/name.data").c_str(), "rb")) {
        char line[256] = {0};
        if (fgets(line, sizeof line, f)) {
            std::string n;
            for (char ch : std::string(line)) if (ch && ch != '\n' && ch != '\r') n += ch;
            out.name = n;
        }
        fclose(f);
    }
    nbt::TagPtr root = nbt::readGzipFile(worldDir + "/level.dat");
    nbt::Tag* d = root ? root->getCompound("Data") : nullptr;
    if (!d) return false;
    out.seed = d->getLong("RandomSeed");
    out.spawnX = d->getInt("SpawnX");
    out.spawnY = d->getInt("SpawnY");
    out.spawnZ = d->getInt("SpawnZ");
    out.time = d->getLong("Time");
    out.sizeOnDisk = d->getLong("SizeOnDisk");
    out.snowCovered = d->getBool("SnowCovered");
    out.cheatsDisabled = d->getBool("cheatsDisabled");
    bool since1112 = d->getBool("loadedSinceOneOneTwelve");
    out.experiencedFirstSnow = d->getBool("experiencedFirstSnow");
    // EnumWeather.valueOf / EnumSeason.valueOf; a world from before 1.1.12 has neither
    int w = -1, s = -1;
    for (int i = 0; i < 4; ++i) if (d->getString("weather") == weatherName(i)) w = i;
    for (int i = 0; i < 5; ++i) if (d->getString("season") == seasonName(i)) s = i;
    if (w >= 0 && s >= 0) {
        out.weather = w;
        out.season = s;
    } else if (w >= 0) {
        out.weather = w; // (valueOf fails on the season after setting the weather)
        if (out.snowCovered && !since1112) { out.season = 2; out.weather = 1; }
    } else if (out.snowCovered && !since1112) {
        out.season = 2; // winter, precipitation
        out.weather = 1;
    }
    out.worldType = d->getBool("isFloating") ? 1 : d->getInt("WorldType");
    if (nbt::Tag* p = d->getCompound("Player")) {
        out.hasPlayer = true;
        if (nbt::Tag* pos = p->getList("Pos"); pos && pos->items.size() == 3) {
            out.playerX = pos->items[0]->d;
            out.playerY = pos->items[1]->d;
            out.playerZ = pos->items[2]->d;
        }
        if (nbt::Tag* rot = p->getList("Rotation"); rot && rot->items.size() == 2) {
            out.playerYaw = float(rot->items[0]->d);
            out.playerPitch = float(rot->items[1]->d);
        }
    }
    levelRoot = std::move(root);
    return true;
}

nbt::Tag* SaveHandler::player() const {
    if (!levelRoot) return nullptr;
    nbt::Tag* d = levelRoot->getCompound("Data");
    return d ? d->getCompound("Player") : nullptr;
}

bool SaveHandler::writeLevel(const LevelInfo& in) {
    // World.loadWorldSaveInfo: a new Data compound in the jar's order, the player's carried over
    nbt::TagPtr oldPlayer;
    if (levelRoot)
        if (nbt::Tag* d = levelRoot->getCompound("Data"))
            for (auto& e : d->entries) if (e.first == "Player") oldPlayer = std::move(e.second);
    levelRoot = nbt::makeCompound();
    nbt::Tag& d = levelRoot->setCompound("Data");
    d.setLong("RandomSeed", in.seed);
    d.setString("LevelName", in.name);
    d.setInt("SpawnX", in.spawnX);
    d.setInt("SpawnY", in.spawnY);
    d.setInt("SpawnZ", in.spawnZ);
    d.setLong("Time", in.time);
    d.setLong("SizeOnDisk", in.sizeOnDisk);
    d.setBool("SnowCovered", in.snowCovered);
    d.setBool("cheatsDisabled", in.cheatsDisabled);
    d.setInt("WorldType", in.worldType);
    d.setLong("LastPlayed", int64_t(time(nullptr)) * 1000);
    d.setBool("loadedSinceOneOneTwelve", true);
    d.setBool("experiencedFirstSnow", in.experiencedFirstSnow);
    d.setString("weather", weatherName(in.weather));
    d.setString("season", seasonName(in.season));
    if (in.hasPlayer) {
        if (oldPlayer) d.set("Player", std::move(oldPlayer));
        nbt::Tag* pp = d.getCompound("Player");
        nbt::Tag& p = pp ? *pp : d.setCompound("Player");
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(in.playerX));
        pos->items.push_back(nbt::makeDouble(in.playerY));
        pos->items.push_back(nbt::makeDouble(in.playerZ));
        p.set("Pos", std::move(pos));
        nbt::TagPtr rot = nbt::makeList(nbt::Float);
        rot->items.push_back(nbt::makeFloat(in.playerYaw));
        rot->items.push_back(nbt::makeFloat(in.playerPitch));
        p.set("Rotation", std::move(rot));
    }
    // write _new, keep the old one as _old, then swap in
    std::string cur = worldDir + "/level.dat", nw = worldDir + "/level.dat_new", old = worldDir + "/level.dat_old";
    if (!nbt::writeGzipFile(nw, *levelRoot)) return false;
    remove(old.c_str());
    rename(cur.c_str(), old.c_str());
    if (rename(nw.c_str(), cur.c_str()) != 0) return false;
    return true;
}

bool SaveHandler::createLevel(const LevelInfo& in) {
    mkdir(worldDir.c_str(), 0777);
    if (FILE* f = fopen((worldDir + "/name.data").c_str(), "wb")) { // GuiSelectWorld.createWorldWithSettings
        fputs(in.name.c_str(), f);
        fclose(f);
    }
    levelRoot.reset();
    return writeLevel(in);
}

bool SaveHandler::chunkSaved(int cx, int cz) {
    RegionFile* rf = regions.get(cx, cz, false);
    return rf && rf->isChunkSaved(cx & 31, cz & 31);
}

namespace {
nbt::TagPtr takeLevel(nbt::TagPtr root) {
    nbt::TagPtr lv;
    if (root)
        for (auto& e : root->entries) if (e.first == "Level") lv = std::move(e.second);
    return lv;
}
} // namespace

Chunk* SaveHandler::loadChunk(int cx, int cz) {
    std::vector<uint8_t> raw;
    RegionFile* rf = regions.get(cx, cz, false);
    if (rf && rf->readChunk(cx & 31, cz & 31, raw)) {
        // the jar writes a gzipped compound into the (deflating) region stream; plain NBT reads too
        nbt::TagPtr root;
        if (raw.size() >= 2 && raw[0] == 0x1f && raw[1] == 0x8b) {
            std::vector<uint8_t> nbtData;
            if (nbt::inflateAny(raw.data(), raw.size(), nbtData)) root = nbt::readRoot(nbtData.data(), nbtData.size());
        } else {
            root = nbt::readRoot(raw.data(), raw.size());
        }
        Chunk* c = ChunkIO::fromLevel(takeLevel(std::move(root)));
        if (c && (c->xPosition != cx || c->zPosition != cz)) { // "in the wrong location; relocating"
            c->xPosition = cx;
            c->zPosition = cz;
            c->modified = true;
        }
        return c;
    }
    return loadLegacyChunk(cx, cz);
}

Chunk* SaveHandler::loadLegacyChunk(int cx, int cz) {
    std::string p = worldDir + "/" + toBase36(cx & 63) + "/" + toBase36(cz & 63) + "/c." + toBase36(cx) + "." +
                    toBase36(cz) + ".dat";
    Chunk* c = ChunkIO::fromLevel(takeLevel(nbt::readGzipFile(p)));
    if (c) {
        c->xPosition = cx;
        c->zPosition = cz;
    }
    return c;
}

bool SaveHandler::saveChunk(Chunk& c, int64_t worldTime) {
    RegionFile* rf = regions.get(c.xPosition, c.zPosition, true);
    if (!rf) return false;
    nbt::TagPtr root = ChunkIO::toRoot(c, worldTime);
    std::vector<uint8_t> raw, gz;
    nbt::writeRoot(*root, raw);
    bool ok = nbt::deflateGzip(raw.data(), raw.size(), gz) && rf->writeChunk(c.xPosition & 31, c.zPosition & 31, gz);
    c.level = takeLevel(std::move(root)); // hand it back...
    // ... without the big arrays: the Chunk has them (kept, they doubled every saved chunk)
    if (c.level)
        for (const char* n : {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"})
            if (nbt::Tag* t = c.level->get(n)) std::vector<uint8_t>().swap(t->bytes);
    if (ok) c.modified = false;
    return ok;
}
