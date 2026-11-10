#include "ChunkGenerator.h"
#include <cstring>

ChunkGenerator::ChunkGenerator(int64_t s) : seed(s) {
    alpha::GenWorld gw;
    gw.seed = seed;
    alphaTerrain.reset(new alpha::Terrain(gw));
}

void ChunkGenerator::provideChunk(int cx, int cz, uint8_t* out) {
    alphaTerrain->provideChunk(cx, cz, out);
}
