#include "SaveHandler.h"
#include <cstdio>
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

SaveHandler::SaveHandler(const std::string& dir, int dim)
    : worldDir(dir), dimDir(dim == -1 ? dir + "/DIM-1" : dir), dimension(dim), regions(dimDir) {}

bool SaveHandler::readLevel(LevelInfo& out) {
    static const char* names[] = {"/level.dat", "/level.dat_new", "/level.dat_old"};
    for (const char* n : names) {
        nbt::TagPtr root = nbt::readGzipFile(worldDir + n);
        if (!root) continue;
        nbt::Tag* d = root->getCompound("Data");
        if (!d) continue;
        out.seed = d->getLong("RandomSeed");
        out.spawnX = d->getInt("SpawnX");
        out.spawnY = d->getInt("SpawnY");
        out.spawnZ = d->getInt("SpawnZ");
        out.time = d->getLong("Time");
        out.gameType = d->getInt("GameType");
        if (nbt::Tag* p = d->getCompound("Player")) {
            out.hasPlayer = true;
            out.dimension = p->getInt("Dimension");
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
    return false;
}

nbt::Tag* SaveHandler::player() const {
    if (!levelRoot) return nullptr;
    nbt::Tag* d = levelRoot->getCompound("Data");
    return d ? d->getCompound("Player") : nullptr;
}

bool SaveHandler::writeLevel(const LevelInfo& in) {
    if (!levelRoot) levelRoot = nbt::makeCompound();
    nbt::Tag& d = levelRoot->setCompound("Data");
    d.setLong("RandomSeed", in.seed);
    d.setInt("SpawnX", in.spawnX);
    d.setInt("SpawnY", in.spawnY);
    d.setInt("SpawnZ", in.spawnZ);
    d.setLong("Time", in.time);
    if (in.hasPlayer) {
        nbt::Tag& p = d.setCompound("Player");
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(in.playerX));
        pos->items.push_back(nbt::makeDouble(in.playerY));
        pos->items.push_back(nbt::makeDouble(in.playerZ));
        p.set("Pos", std::move(pos));
        nbt::TagPtr rot = nbt::makeList(nbt::Float);
        rot->items.push_back(nbt::makeFloat(in.playerYaw));
        rot->items.push_back(nbt::makeFloat(in.playerPitch));
        p.set("Rotation", std::move(rot));
        p.setInt("Dimension", in.dimension);
    }
    // Same dance as World.saveLevel: write _new, keep the old one as _old, then swap in.
    std::string cur = worldDir + "/level.dat", nw = worldDir + "/level.dat_new", old = worldDir + "/level.dat_old";
    if (!nbt::writeGzipFile(nw, *levelRoot)) return false;
    remove(old.c_str());
    rename(cur.c_str(), old.c_str());
    if (rename(nw.c_str(), cur.c_str()) != 0) return false;
    return true;
}

bool SaveHandler::createLevel(const LevelInfo& in, int gameType) {
    mkdir(worldDir.c_str(), 0777);
    levelRoot = nbt::makeCompound();
    nbt::Tag& d = levelRoot->setCompound("Data");
    d.setLong("LastPlayed", int64_t(time(nullptr)) * 1000);
    d.setLong("RandomSeed", in.seed);
    d.setInt("SpawnY", in.spawnY);
    d.setInt("SpawnZ", in.spawnZ);
    d.setInt("SpawnX", in.spawnX);
    d.setLong("Time", in.time);
    d.set("AxeolonBiomes", nbt::makeList(nbt::Compound));
    d.setInt("GameType", gameType);
    d.setLong("SizeOnDisk", 0);
    return writeLevel(in);
}

bool SaveHandler::chunkSaved(int cx, int cz) {
    RegionFile* rf = regions.get(cx, cz, false);
    return rf && rf->isChunkSaved(cx & 31, cz & 31);
}

Chunk* SaveHandler::loadChunk(int cx, int cz) {
    std::vector<uint8_t> raw;
    RegionFile* rf = regions.get(cx, cz, false);
    if (rf && rf->readChunk(cx & 31, cz & 31, raw)) {
        nbt::TagPtr root = nbt::readRoot(raw.data(), raw.size());
        if (!root) return nullptr;
        nbt::Tag* level = root->getCompound("Level");
        if (!level) return nullptr;
        // Detach "Level" from the root so the chunk owns it
        nbt::TagPtr lv;
        for (auto& e : root->entries) if (e.first == "Level") lv = std::move(e.second);
        Chunk* c = ChunkIO::fromLevel(std::move(lv), dimension == -1 ? 0 : 15);
        if (c && (c->xPosition != cx || c->zPosition != cz)) { // "in the wrong location; relocating"
            c->xPosition = cx;
            c->zPosition = cz;
            c->modified = true;
        }
        return c;
    }
    Chunk* legacy = loadLegacyChunk(cx, cz);
    if (legacy) legacy->modified = true; // goes into the region file next save
    return legacy;
}

Chunk* SaveHandler::loadLegacyChunk(int cx, int cz) {
    std::string p = dimDir + "/" + toBase36(cx & 63) + "/" + toBase36(cz & 63) + "/c." + toBase36(cx) + "." +
                    toBase36(cz) + ".dat";
    nbt::TagPtr root = nbt::readGzipFile(p);
    if (!root) return nullptr;
    nbt::TagPtr lv;
    for (auto& e : root->entries) if (e.first == "Level") lv = std::move(e.second);
    Chunk* c = ChunkIO::fromLevel(std::move(lv), dimension == -1 ? 0 : 15);
    if (c) { c->xPosition = cx; c->zPosition = cz; }
    return c;
}

bool SaveHandler::saveChunk(Chunk& c, int64_t worldTime) {
    RegionFile* rf = regions.get(c.xPosition, c.zPosition, true);
    if (!rf) return false;
    nbt::TagPtr root = ChunkIO::toRoot(c, worldTime);
    std::vector<uint8_t> raw;
    nbt::writeRoot(*root, raw);
    bool ok = rf->writeChunk(c.xPosition & 31, c.zPosition & 31, raw);
    for (auto& e : root->entries) if (e.first == "Level") c.level = std::move(e.second); // hand it back
    // ... without the big arrays: the Chunk has them, and ChunkIO::fromLevel frees these copies too. Kept,
    // they doubled every saved chunk (~160 KB each) - an Old 3DS ran out of memory saving and then loading.
    if (c.level)
        for (const char* n : {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"})
            if (nbt::Tag* t = c.level->get(n)) std::vector<uint8_t>().swap(t->bytes);
    if (ok) c.modified = false;
    return ok;
}
