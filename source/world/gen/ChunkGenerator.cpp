#include "ChunkGenerator.h"
#include <cstring>

ChunkGenerator::ChunkGenerator(int64_t s) : seed(s) {
    alpha::GenWorld gw;
    gw.seed = seed;
    alphaTerrain.reset(new alpha::Terrain(gw));
}

void ChunkGenerator::provideChunk(int cx, int cz, uint8_t* out) {
    alphaTerrain->provideChunk(cx, cz, scratch);
    std::memset(out, 0, 65536);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) std::memcpy(out + (x << 12 | z << 8), scratch + (x << 11 | z << 7), 128);
}
