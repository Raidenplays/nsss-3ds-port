#pragma once
// NSSS's overworld: Alpha terrain + caves (AlphaGen) and NSSS's decoration step (Populate.cpp),
// both 1:1 with the a1.1.12 jar.
#include <cstdint>
#include <memory>
#include <string>
#include "AlphaGen.h"
#include "world/JavaRandom.h"

class World;

class ChunkGenerator {
public:
    explicit ChunkGenerator(int64_t seed);
    // ChunkProviderGenerate.provideChunk: out = 32768 block ids, index x << 11 | z << 7 | y
    void provideChunk(int cx, int cz, uint8_t* out);
    // ChunkProviderGenerate.generateStructures
    void populate(World& w, int cx, int cz);
    int64_t seed;
    std::unique_ptr<alpha::Terrain> alphaTerrain;

private:
    void alphaPopulate(World& w, int cx, int cz);
};

// NSSS BlockSapling's tree (Populate.cpp)
bool nsssSaplingTree(World& w, JavaRandom& r, int x, int y, int z);
