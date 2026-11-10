// NSSS populate parity: loads chunks ring by ring on the 3DS World (Alpha terrain + Alpha populate)
// and writes id+meta of every block in the report square, like tools/nsss/AlphaPopDump.java.
// usage: alphapop <seed> <emptyDir> cx0 cz0 loadRadius reportRadius out
#include "world/World.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>
int main(int argc, char** argv) {
    if (argc < 8) { fprintf(stderr, "usage: alphapop seed dir cx0 cz0 R R2 out\n"); return 1; }
    mkdir(argv[2], 0777);
    SaveHandler s(argv[2]);
    LevelInfo li;
    li.seed = strtoll(argv[1], nullptr, 10);
    World w(&s, li, false);
    int cx0 = atoi(argv[3]), cz0 = atoi(argv[4]), R = atoi(argv[5]), R2 = atoi(argv[6]);
    w.setWindow(cx0, cz0);
    for (int ring = 0; ring <= R; ring++)
        for (int dx = -ring; dx <= ring; dx++)
            for (int dz = -ring; dz <= ring; dz++) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                w.provideChunk(cx0 + dx, cz0 + dz);
            }
    FILE* f = fopen(argv[7], "wb");
    for (int cx = cx0 - R2; cx <= cx0 + R2; cx++)
        for (int cz = cz0 - R2; cz <= cz0 + R2; cz++) {
            Chunk* c = w.getChunk(cx, cz);
            for (int x = 0; x < 16; x++)
                for (int z = 0; z < 16; z++)
                    for (int y = 0; y < 128; y++) {
                        uint8_t b[2] = {uint8_t(c ? c->getBlockID(x, y, z) : 255), uint8_t(c ? c->getMetadata(x, y, z) : 255)};
                        fwrite(b, 1, 2, f);
                    }
        }
    fclose(f);
    puts("ok");
}
