#include "world/gen/ChunkGenerator.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
static uint64_t fnv(const uint8_t* b, size_t n) { uint64_t h = 0xcbf29ce484222325ULL; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ULL; } return h; }
int main(int argc, char** argv) {
  long long seed = atoll(argv[1]); int cx0 = atoi(argv[2]), cz0 = atoi(argv[3]), r = atoi(argv[4]);
  ChunkGenerator g(seed, "");
  std::vector<uint8_t> out(65536);
  for (int cx = cx0 - r; cx <= cx0 + r; cx++) for (int cz = cz0 - r; cz <= cz0 + r; cz++) {
    g.provideChunk(cx, cz, out.data());
    printf("%d %d %016llx\n", cx, cz, (unsigned long long)fnv(out.data(), 65536));
    if (argc > 7 && cx == atoi(argv[5]) && cz == atoi(argv[6])) { FILE* f = fopen(argv[7], "wb"); fwrite(out.data(), 1, 65536, f); fclose(f); }
  }
}
