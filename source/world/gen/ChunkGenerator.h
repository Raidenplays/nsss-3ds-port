#pragma once
// Axeolon's overworld generator: ChunkProviderGenerate + WorldChunkManager + MapGenCaves +
// MapGenRavine + AxeolonStone + AxeolonPeaks + AxeolonBlend, ported to give the same blocks as
// Java for the same seed (checked against the mod with hosttest/gen.cpp and gentest/GenRaw.java).
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include "Biome.h"
#include "Noise.h"
#include "Nether.h"
#include "AlphaGen.h"
#include "world/JavaRandom.h"

class World;

struct BiomeOverride {
    int x, z, radius;
    Biome biome;
    double temperature, humidity;
    bool contains(int bx, int bz) const {
        int64_t dx = int64_t(bx) - x, dz = int64_t(bz) - z;
        return dx * dx + dz * dz <= int64_t(radius) * radius;
    }
};

// AxeolonBlend: chunks that existed before the world was opened with this version
class Blend {
public:
    // reads <worldDir>/axeolon_blend.dat, or writes an empty one for a world that has none
    void init(const std::string& worldDir);
    bool isOld(int cx, int cz) const { return any && old.count(key(cx, cz)); }
    double factor(int x, int z) const;
    std::unordered_set<int64_t> old;
    bool any = false;
    static int64_t key(int cx, int cz) { return int64_t(uint64_t(int64_t(cx)) << 32 | uint32_t(cz)); }
};

class Peaks {
public:
    Peaks(int64_t seed, const Blend& blend);
    double region(int x, int z) const;
    bool isPeaks(int x, int z) const { return region(x, z) > 0.35 && !blend.isOld(x >> 4, z >> 4); }
    double lift(int x, int z) const;
    void raise(uint8_t* blocks, int cx, int cz) const; // 256-tall layout

private:
    int64_t seed;
    const Blend& blend;
    int perm[512];
    double noise(double x, double y, int salt) const;
    double ridged(double x, double z, int salt) const;
    uint8_t strata(int x, int y, int z) const;
};

class ChunkManager { // WorldChunkManager
public:
    ChunkManager(int64_t seed, const Peaks& peaks);
    // loadBlockGeneratorData: fills biomes (w*h) and temperature/humidity
    void load(std::vector<Biome>& out, int x, int z, int w, int h);
    Biome biomeAt(int x, int z); // func_4073_a
    void getTemperatures(std::vector<double>& out, int x, int z, int w, int h);
    double rawTemperature(int x, int z) const; // func_4072_b (the sky colour's temperature)
    std::vector<double> temperature, humidity, field_4196_c;
    std::vector<BiomeOverride> overrides;

private:
    NoiseOctaves2 tempNoise, humNoise, mixNoise;
    const Peaks& peaks;
    const BiomeOverride* findOverride(int x, int z) const;
};

class ChunkGenerator {
public:
    // worldDir may be empty (no blending); dimension -1 = the Nether (ChunkProviderHell)
    ChunkGenerator(int64_t seed, const std::string& worldDir, int dimension = 0);
    ~ChunkGenerator();
    // provideChunk: out = 65536 block ids, index x << 12 | z << 8 | y
    void provideChunk(int cx, int cz, uint8_t* out);
    // ChunkProviderGenerate.populate (Populate.cpp)
    void populate(World& w, int cx, int cz);
    int64_t seed;
    Blend blend;
    std::unique_ptr<Peaks> peaks;
    std::unique_ptr<ChunkManager> manager;
    std::vector<Biome> biomes; // of the last chunk
    std::unique_ptr<NetherTerrain> hell; // set in the Nether
    std::unique_ptr<alpha::Terrain> alphaTerrain; // NSSS: the overworld is Alpha's (AlphaGen)
    // a chunk was generated on the other core: put the generator's state where it would be
    void providedElsewhere(int cx, int cz) { if (hell) hell->syncAfterProvide(cx, cz); }

private:
    JavaRandom rand;
    NoiseOctaves n912k, n911l, n910m, n909n, n908o, n922a, n921b, mobNoise, extremeNoise;
    std::vector<double> generatedTemperatures;
    std::vector<double> heightField, sandNoise, gravelNoise, stoneNoise, d4185, d4184, d4183, d4182, d4181, extreme;
    JavaRandom caveRand, ravineRand;
    float widthByY[1024];
    uint8_t scratch[32768]; // the 128-tall Beta block array

    void generateTerrain(int cx, int cz, uint8_t* b);
    void replaceBlocksForBiome(int cx, int cz, uint8_t* b);
    void heightNoise(int x, int y, int z, int sx, int sy, int sz);
    // MapGenBase.func_867_a for caves (0) and ravines (1)
    void mapGen(int kind, int cx, int cz, uint8_t* b);
    void caveChunk(int chunkX, int chunkZ, int cx, int cz, uint8_t* b);
    void caveTunnel(int cx, int cz, uint8_t* b, double x, double y, double z, float width,
                    float yaw, float pitch, int step, int steps, double hScale);
    void ravineChunk(int chunkX, int chunkZ, int cx, int cz, uint8_t* b);
    void ravine(int64_t s, int cx, int cz, uint8_t* b, double x, double y, double z, float width, float yaw,
                float pitch, int step, int steps, double hScale);
    void cavern(int64_t s, int chunkX, int chunkZ, int cx, int cz, uint8_t* b);
    void carve(int cx, int cz, uint8_t* b, double x, double y, double z, double hw, double hh, bool isRavine);
    void stoneTypes(uint8_t* b, int cx, int cz);
};

// ChunkProviderHell.populate (Populate.cpp)
void populateHell(World& w, JavaRandom& hellRNG, int cx, int cz);

// BlockSapling.func_21028_c (Populate.cpp, next to the tree generators)
bool growSapling(World& w, JavaRandom& r, int x, int y, int z, int id, int meta);
