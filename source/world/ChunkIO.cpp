#include "Chunk.h"
#include <vector>

namespace {

// Chunk.expandBlocks / expandNibbles (Axeolon): 128 -> 256 tall
void expandBlocks(const std::vector<uint8_t>& old, uint8_t* out) {
    std::memset(out, 0, 65536);
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
            std::memcpy(out + (x << 12 | z << 8), old.data() + (x << 11 | z << 7), 128);
}

void expandNibbles(const std::vector<uint8_t>& old, uint8_t* out, int fillTop) {
    uint8_t top = uint8_t(fillTop << 4 | fillTop);
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++) {
            int from = (x << 11 | z << 7) >> 1, to = (x << 12 | z << 8) >> 1;
            std::memcpy(out + to, old.data() + from, 64);
            std::memset(out + to + 64, top, 64);
        }
}

void loadNibbles(const std::vector<uint8_t>* src, NibbleArray& dst, bool legacy, int fillTop, bool& valid) {
    valid = false;
    std::memset(dst.data, 0, sizeof dst.data);
    if (!src) return;
    if (legacy && src->size() == 16384) { expandNibbles(*src, dst.data, fillTop); valid = true; }
    else if (src->size() == sizeof dst.data) { std::memcpy(dst.data, src->data(), sizeof dst.data); valid = true; }
}

} // namespace

namespace ChunkIO {

Chunk* fromLevel(nbt::TagPtr level, int skyFill) {
    if (!level) return nullptr;
    const std::vector<uint8_t>* blocks = level->getByteArray("Blocks");
    if (!blocks) return nullptr; // "missing block data, skipping"

    Chunk* c = new Chunk(level->getInt("xPos"), level->getInt("zPos"));
    bool legacy = blocks->size() == 32768;
    if (legacy) { expandBlocks(*blocks, c->blocks); c->modified = true; }
    else if (blocks->size() == 65536) std::memcpy(c->blocks, blocks->data(), 65536);
    else { delete c; return nullptr; }

    bool dataOk, skyOk, blockOk;
    loadNibbles(level->getByteArray("Data"), c->data, legacy, 0, dataOk);
    loadNibbles(level->getByteArray("SkyLight"), c->skylight, legacy, skyFill, skyOk);
    loadNibbles(level->getByteArray("BlockLight"), c->blocklight, legacy, 0, blockOk);
    const std::vector<uint8_t>* hm = level->getByteArray("HeightMap");
    bool hmOk = hm && hm->size() == 256;
    if (hmOk) std::memcpy(c->heightMap, hm->data(), 256);
    c->terrainPopulated = level->getBool("TerrainPopulated");
    // ChunkLoader: no height map or sky light (an edited world) -> the world makes them again
    // (func_1024_c) as it takes the chunk; missing block light just starts dark (func_1014_a is empty)
    c->regenSkylight = !hmOk || !skyOk;
    (void)dataOk; (void)blockOk;

    // The big arrays now live in the Chunk; free their copies in the kept compound (~160 KB per
    // 256-tall chunk) but keep the entries, so saving writes them back in their original order.
    for (const char* n : {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"})
        if (nbt::Tag* t = level->get(n)) std::vector<uint8_t>().swap(t->bytes);
    c->level = std::move(level);
    return c;
}

nbt::TagPtr toRoot(Chunk& c, int64_t worldTime) {
    nbt::TagPtr root = nbt::makeCompound();
    nbt::TagPtr level = c.level ? std::move(c.level) : nbt::makeCompound();
    level->setInt("xPos", c.xPosition);
    level->setInt("zPos", c.zPosition);
    level->setLong("LastUpdate", worldTime);
    level->setByteArray("Blocks", c.blocks, sizeof c.blocks);
    level->setByteArray("Data", c.data.data, sizeof c.data.data);
    level->setByteArray("SkyLight", c.skylight.data, sizeof c.skylight.data);
    level->setByteArray("BlockLight", c.blocklight.data, sizeof c.blocklight.data);
    level->setByteArray("HeightMap", c.heightMap, sizeof c.heightMap);
    level->setBool("TerrainPopulated", c.terrainPopulated);
    // Entities / TileEntities: kept exactly as loaded until those are ported
    if (!level->has("Entities")) level->set("Entities", nbt::makeList(nbt::Compound));
    if (!level->has("TileEntities")) level->set("TileEntities", nbt::makeList(nbt::Compound));

    // The root owns "Level" while it's being written; SaveHandler::saveChunk
    // hands it back to the chunk afterwards (no copy of the 160 KB arrays).
    root->set("Level", std::move(level));
    return root;
}

} // namespace ChunkIO
