// Block behaviour parity: runs a block script on the 3DS World, like tools/nsss/BlockSimDump.java.
// usage: blocksim <seed> <emptyDir> <script> <out>   (script format: see BlockSimDump.java)
#include "world/World.h"
#include "world/Blocks.h"
#include "world/Redstone.h"
#include "world/BlockProps.h"
#include "render/ChunkMesh.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: blocksim seed dir script out\n"); return 1; }
    mkdir(argv[2], 0777);
    SaveHandler s(argv[2]);
    LevelInfo li;
    li.seed = strtoll(argv[1], nullptr, 10);
    World w(&s, li, false);
    w.rand.setSeed(777);
    // like the Java side, falling sand becomes an entity that this test never ticks
    w.onFallingBlock = [](int, int, int, int) {};
    Blocks::onTntIgnited = [](int, int, int) {}; // EntityTNTPrimed: spawned, never ticked here
    w.info.time = 0;
    FILE* in = fopen(argv[3], "r");
    FILE* out = fopen(argv[4], "wb");
    char line[256];
    bool windowSet = false;
    while (fgets(line, sizeof line, in)) {
        char cmd[16] = {0};
        int v[7] = {0};
        int n = sscanf(line, "%15s %d %d %d %d %d %d %d", cmd, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]);
        if (n < 1 || cmd[0] == '#') continue;
        if (!strcmp(cmd, "load")) {
            if (!windowSet) { w.setWindow(v[0], v[1]); windowSet = true; }
            for (int ring = 0; ring <= v[2]; ring++)
                for (int dx = -ring; dx <= ring; dx++)
                    for (int dz = -ring; dz <= ring; dz++)
                        if (std::max(std::abs(dx), std::abs(dz)) == ring) w.provideChunk(v[0] + dx, v[1] + dz);
        } else if (!strcmp(cmd, "set")) {
            w.setBlockAndMetadataWithNotify(v[0], v[1], v[2], v[3], v[4]);
        } else if (!strcmp(cmd, "tick")) {
            for (int i = 0; i < v[0]; i++) { w.info.time++; w.tickUpdates(false); }
        } else if (!strcmp(cmd, "rtick")) {
            for (int r = 0; r < v[6]; r++)
                for (int x = v[0]; x <= v[3]; x++)
                    for (int z = v[2]; z <= v[5]; z++)
                        for (int y = v[1]; y <= v[4]; y++) {
                            int id = w.blockAt(x, y, z);
                            if (id > 0 && BP[id].tickOnLoad) Blocks::updateTick(w, x, y, z, id, w.rand);
                        }
        } else if (!strcmp(cmd, "use")) {
            Redstone::blockActivated(w, v[0], v[1], v[2]);
        } else if (!strcmp(cmd, "sub")) {
            w.skylightSubtracted = v[0];
        } else if (!strcmp(cmd, "render")) {
            // the quads, as BlockSimDump's shadow Tessellator writes them (big-endian ints)
            auto be = [&](uint32_t u) { uint8_t b[4] = {uint8_t(u >> 24), uint8_t(u >> 16), uint8_t(u >> 8), uint8_t(u)}; fwrite(b, 1, 4, out); };
            int sub = w.skylightSubtracted;
            ChunkMesher::recordQuad = [&](int x, int y, int z, const float q[4][5], float factor, int light) {
                be(uint32_t(x)); be(uint32_t(y)); be(uint32_t(z));
                for (int k = 0; k < 4; k++)
                    for (int a = 0; a < 5; a++) { uint32_t u; memcpy(&u, &q[k][a], 4); be(u); }
                int lv = std::max((light >> 4) - sub, 0);
                lv = std::max(lv, light & 15);
                float c = factor * ChunkMesher::LIGHT_BRIGHTNESS[lv];
                int ci = std::min(255, std::max(0, int(c * 255.0f)));
                be(uint32_t(ci << 16 | ci << 8 | ci));
            };
            MeshData md;
            ChunkMesher::buildSection(w, v[0], v[1], v[2], md);
            ChunkMesher::recordQuad = nullptr;
        } else if (!strcmp(cmd, "dump")) {
            for (int x = v[0]; x <= v[3]; x++)
                for (int z = v[2]; z <= v[5]; z++)
                    for (int y = v[1]; y <= v[4]; y++) {
                        uint8_t b[2] = {w.blockAt(x, y, z), uint8_t(w.metaAt(x, y, z))};
                        fwrite(b, 1, 2, out);
                    }
        }
    }
    fclose(out);
    puts("ok");
}
