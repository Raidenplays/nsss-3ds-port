#pragma once
// Not So Seecret Saturday (Alpha 1.1.2_01 fork) terrain: ChunkProviderGenerate + MapGenCaves,
// ported line by line from the a1.1.12 jar so a seed gives the same blocks as the PC game.
// Checked against the jar's own classes with tools/host/alphagen.cpp + tools/nsss/AlphaGenDump.java.
//
// Block array layout is Alpha's: 128 tall, index x << 11 | z << 7 | y (32768 bytes).
#include <cstdint>
#include <vector>
#include "world/JavaRandom.h"

namespace alpha {

// Alpha's MathHelper: the same 65536-entry table as Beta, but indexed with 10430.38F
// (Beta uses 10430.378F, a different float).
float sin(float v);
float cos(float v);
inline int floor_double(double v) {
    int i = int(v);
    return v < double(i) ? i - 1 : i;
}

class NoisePerlin { // NoiseGeneratorPerlin
public:
    explicit NoisePerlin(JavaRandom& r);
    double generateNoise(double x, double y, double z) const;
    double getVal(double x, double z) const { return generateNoise(x, z, 0.0); }
    // func_805_a: always the full 3D loop (Alpha has no 2D shortcut here)
    void add3D(double* out, double x, double y, double z, int sx, int sy, int sz, double fx, double fy,
               double fz, double amp) const;
    // func_805_a_: 2D shortcuts for sy == 1 or sz == 1 (used by the beach and soil noise)
    void add2D(double* out, double x, double y, double z, int sx, int sy, int sz, double fx, double fy,
               double fz, double amp) const;
    double xCoord, yCoord, zCoord;

private:
    int p[512];
    static double grad(int i, double x, double y, double z);
    static double grad2(int i, double x, double z); // func_4110_a
};

class NoiseOctaves { // NoiseGeneratorOctaves
public:
    NoiseOctaves(JavaRandom& r, int octaves);
    void generate(std::vector<double>& out, double x, double y, double z, int sx, int sy, int sz, double fx,
                  double fy, double fz) const;  // generateNoiseOctaves
    void generate2(std::vector<double>& out, double x, double y, double z, int sx, int sy, int sz, double fx,
                   double fy, double fz) const; // generateNoiseOctaves_2
    double getVal(double x, double z) const;

private:
    std::vector<NoisePerlin> gens;
};

// World settings the generator reads
struct GenWorld {
    int64_t seed = 0;
    bool winter = false; // snowCovered || season == winter: the sea freezes at y 63
};

class Terrain {
public:
    explicit Terrain(const GenWorld& w);
    // provideChunk without the Chunk object: fills 32768 block ids (Alpha layout)
    void provideChunk(int cx, int cz, uint8_t* blocks);

    // declared in the order the Java constructor creates them (members initialise in this order)
    GenWorld world;
    JavaRandom genRandom; // shared with the populate step
    NoiseOctaves noiseGen1, noiseGen2, noiseGen3, beachNoise, soilNoise, noiseGen6, noiseGen7;
    NoiseOctaves treeNoise; // populate's tree count

private:
    std::vector<double> densities, sandBeach, gravelBeach, soilThickness, nd1, nd2, nd3, hb1, hb2;
    JavaRandom caveRand;

    void generateTerrain(int cx, int cz, uint8_t* b);
    void replaceBlocksForBiome(int cx, int cz, uint8_t* b);
    void initializeNoiseField(int x, int y, int z, int sx, int sy, int sz);
    void caves(int cx, int cz, uint8_t* b); // MapGenBase.func_867_a with MapGenCaves
    void caveStart(int i, int j, int k, int l, uint8_t* b);
    void tunnel(int cx, int cz, uint8_t* b, double x, double y, double z, float width, float yaw, float pitch,
                int step, int steps, double hScale);
};

} // namespace alpha
