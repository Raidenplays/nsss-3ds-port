// Host check for the NSSS (Alpha) terrain: writes the same raw dump as tools/nsss/AlphaGenDump.java.
//   g++ -std=c++17 -O2 -ffp-contract=off -Isource tools/host/alphagen.cpp source/world/gen/AlphaGen.cpp -o alphagen
//   ./alphagen <seed> <x0> <z0> <x1> <z1> <out>     then: cmp out java.bin
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "world/gen/AlphaGen.h"

int main(int argc, char** argv) {
    if (argc < 7) {
        std::fprintf(stderr, "usage: alphagen seed x0 z0 x1 z1 out\n");
        return 1;
    }
    alpha::GenWorld w;
    w.seed = std::strtoll(argv[1], nullptr, 10);
    int x0 = std::atoi(argv[2]), z0 = std::atoi(argv[3]), x1 = std::atoi(argv[4]), z1 = std::atoi(argv[5]);
    alpha::Terrain t(w);
    std::vector<uint8_t> b(32768);
    FILE* f = std::fopen(argv[6], "wb");
    for (int cx = x0; cx <= x1; ++cx)
        for (int cz = z0; cz <= z1; ++cz) {
            t.provideChunk(cx, cz, b.data());
            std::fwrite(b.data(), 1, b.size(), f);
        }
    std::fclose(f);
    std::puts("ok");
}
