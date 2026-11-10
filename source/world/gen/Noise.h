#pragma once
// Beta's noise generators, ported line by line (NoiseGeneratorPerlin, NoiseGeneratorOctaves,
// NoiseGenerator2 = 2D simplex, NoiseGeneratorOctaves2). Same seed, same doubles as Java.
#include <vector>
#include "world/JavaRandom.h"

class NoisePerlin {
public:
    explicit NoisePerlin(JavaRandom& r);
    double generateNoise(double x, double y, double z) const;
    double noise2(double x, double z) const { return generateNoise(x, z, 0.0); } // func_801_a
    // func_805_a: adds this octave into out
    void add(double* out, double x, double y, double z, int sx, int sy, int sz, double fx, double fy, double fz,
             double amp) const;
    double xCoord, yCoord, zCoord;

private:
    int p[512];
};

class NoiseOctaves {
public:
    NoiseOctaves(JavaRandom& r, int octaves);
    NoiseOctaves(JavaRandom&& r, int octaves) : NoiseOctaves(r, octaves) {}
    // generateNoiseOctaves: out is resized to sx*sy*sz and overwritten
    void generate(std::vector<double>& out, double x, double y, double z, int sx, int sy, int sz, double fx,
                  double fy, double fz) const;
    // func_4109_a: a 2D sheet at y = 10
    void generate2D(std::vector<double>& out, int x, int z, int sx, int sz, double fx, double fz, double unused) const {
        generate(out, double(x), 10.0, double(z), sx, 1, sz, fx, 1.0, fz);
        (void)unused;
    }
    double value2D(double x, double z) const; // func_806_a

private:
    std::vector<NoisePerlin> octaves;
};

class NoiseSimplex {
public:
    explicit NoiseSimplex(JavaRandom& r);
    void add(double* out, double x, double z, int sx, int sz, double fx, double fz, double amp) const;

private:
    int p[512];
    double xo, yo, zo;
};

class NoiseOctaves2 {
public:
    NoiseOctaves2(JavaRandom& r, int octaves);
    NoiseOctaves2(JavaRandom&& r, int octaves) : NoiseOctaves2(r, octaves) {}
    // func_4111_a / func_4112_a (persistence 0.5): out resized to sx*sz if smaller, all zeroed
    void generate(std::vector<double>& out, double x, double z, int sx, int sz, double fx, double fz, double lac,
                  double pers = 0.5) const;

private:
    std::vector<NoiseSimplex> octaves;
};
