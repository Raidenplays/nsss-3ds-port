#include "Chunk.h"
#include <vector>

namespace ChunkIO {

namespace {
bool loadNibbles(const std::vector<uint8_t>* src, NibbleArray& dst) { // NibbleArray.isValid
    std::memset(dst.data, 0, sizeof dst.data);
    if (!src || src->size() != sizeof dst.data) return false;
    std::memcpy(dst.data, src->data(), sizeof dst.data);
    return true;
}
} // namespace

Chunk* fromLevel(nbt::TagPtr level) {
    if (!level) return nullptr;
    const std::vector<uint8_t>* blocks = level->getByteArray("Blocks");
    if (!blocks || blocks->size() != sizeof(Chunk::blocks)) return nullptr; // "missing block data, skipping"

    Chunk* c = new Chunk(level->getInt("xPos"), level->getInt("zPos"));
    std::memcpy(c->blocks, blocks->data(), sizeof c->blocks);
    loadNibbles(level->getByteArray("Data"), c->data); // (invalid: a new, empty one)
    bool skyOk = loadNibbles(level->getByteArray("SkyLight"), c->skylight);
    loadNibbles(level->getByteArray("BlockLight"), c->blocklight); // (invalid: func_1014_a does nothing)
    const std::vector<uint8_t>* hm = level->getByteArray("HeightMap");
    bool hmOk = hm && hm->size() == 256;
    if (hmOk) std::memcpy(c->heightMap, hm->data(), 256);
    c->terrainPopulated = level->getBool("TerrainPopulated");
    c->frozen = level->getBool("Frozen");
    c->snowy = level->getBool("Snowy");
    // no height map or sky light (an edited world): the world makes them again (func_1024_c)
    c->regenSkylight = !hmOk || !skyOk;

    // The big arrays now live in the Chunk; free their copies in the kept compound but keep the
    // entries, so saving writes them back in their original order.
    for (const char* n : {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"})
        if (nbt::Tag* t = level->get(n)) std::vector<uint8_t>().swap(t->bytes);
    c->level = std::move(level);
    return c;
}

// ChunkLoader.storeChunkInCompound
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
    level->setBool("Frozen", c.frozen);
    level->setBool("Snowy", c.snowy);
    // Entities / TileEntities: the world keeps them in this compound
    if (!level->has("Entities")) level->set("Entities", nbt::makeList(nbt::Compound));
    if (!level->has("TileEntities")) level->set("TileEntities", nbt::makeList(nbt::Compound));

    // The root owns "Level" while it's being written; SaveHandler::saveChunk
    // hands it back to the chunk afterwards (no copy of the big arrays).
    root->set("Level", std::move(level));
    return root;
}

} // namespace ChunkIO
