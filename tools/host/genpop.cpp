// Generates and populates chunks in a fixed order on the 3DS World; compare with gentest/GenPop.java.
// usage: [DIM=-1] genpop <worldDir> cx0 cz0 loadRadius reportRadius
#include "world/World.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
static uint64_t fnv(const uint8_t* b, size_t n, uint64_t h = 0xcbf29ce484222325ULL) { for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ULL; } return h; }
int main(int argc, char** argv) {
    int dim = getenv("DIM") ? atoi(getenv("DIM")) : 0; // DIM=-1: the Nether (gentest/GenPopNether.java)
    SaveHandler s(argv[1], dim); LevelInfo li; s.readLevel(li); li.dimension = dim; World w(&s, li, false);
    int cx0 = atoi(argv[2]), cz0 = atoi(argv[3]), R = atoi(argv[4]), R2 = atoi(argv[5]);
    w.setWindow(cx0, cz0);
    w.rand.setSeed(777);
    for (int ring = 0; ring <= R; ring++)
        for (int dx = -ring; dx <= ring; dx++)
            for (int dz = -ring; dz <= ring; dz++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                w.provideChunk(cx0 + dx, cz0 + dz);
            }
    { int n = 0; while (w.updatingLighting()) { if (++n > 100000) break; } }
    for (int cx = cx0 - R2; cx <= cx0 + R2; cx++)
        for (int cz = cz0 - R2; cz <= cz0 + R2; cz++) {
            Chunk* c = w.getChunk(cx, cz);
            if (!c) { printf("%d %d missing\n", cx, cz); continue; }
            printf("%d %d %016llx %016llx %d %016llx %016llx\n", cx, cz, (unsigned long long)fnv(c->blocks, 65536), (unsigned long long)fnv(c->data.data, 32768), c->terrainPopulated, (unsigned long long)fnv(c->skylight.data, 32768), (unsigned long long)fnv(c->blocklight.data, 32768));
        }
    if (argc > 8) { // dump one chunk's blocks + meta
        Chunk* c = w.getChunk(atoi(argv[6]), atoi(argv[7]));
        FILE* f = fopen(argv[8], "wb"); fwrite(c->blocks, 1, 65536, f); fwrite(c->data.data, 1, 32768, f); fwrite(c->skylight.data, 1, 32768, f); fwrite(c->blocklight.data, 1, 32768, f); fclose(f);
    }
}
