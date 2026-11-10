#pragma once
// ChunkProviderHell + MapGenCavesHell: the Nether's terrain, the same blocks as the mod's for the
// same seed. Populating (WorldGenHellLava, fire, glowstone, mushrooms, ember buds) is in
// Populate.cpp (populateHell) next to the overworld's decorations it shares.
#include <cstdint>
#include <vector>
#include "Noise.h"
#include "world/JavaRandom.h"

class NetherTerrain {
public:
    explicit NetherTerrain(int64_t seed);
    // provideChunk: out = 65536 block ids, index x << 12 | z << 8 | y
    void provideChunk(int cx, int cz, uint8_t* out);
    // populate continues with hellRNG where the last provideChunk left it (Java never reseeds it);
    // a chunk generated elsewhere (on the other core) puts it where it would have been
    void syncAfterProvide(int cx, int cz);
    JavaRandom hellRNG;
    int64_t seed;

private:
    NoiseOctaves n4169i, n4168j, n4167k, n4166l, n4165m, n4177a, n4176b;
    std::vector<double> density, n4162p, n4161q, n4160r, n4175c, n4174d, n4173e, n4172f, n4171g;
    JavaRandom caveRand;
    uint8_t scratch[32768];
    void terrain(int cx, int cz, uint8_t* b);  // func_4059_a
    void surface(int cx, int cz, uint8_t* b);  // func_4058_b
    void heightNoise(int x, int y, int z, int sx, int sy, int sz); // func_4057_a
    void caves(int cx, int cz, uint8_t* b);
    void caveChunk(int chunkX, int chunkZ, int cx, int cz, uint8_t* b);
    void tunnel(int cx, int cz, uint8_t* b, double x, double y, double z, float width, float yaw, float pitch,
                int step, int steps, double hScale);
};
