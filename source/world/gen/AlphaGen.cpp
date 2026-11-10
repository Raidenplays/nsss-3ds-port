#include "AlphaGen.h"
#include <cmath>

// Line-by-line port of the a1.1.12 jar's ChunkProviderGenerate, NoiseGeneratorOctaves,
// NoiseGeneratorPerlin, MapGenBase and MapGenCaves. Java evaluates operands left to right and
// C++ doesn't promise that, so every expression with two random calls is split into statements.

namespace alpha {

namespace {
inline int64_t mul(int64_t a, int64_t b) { return int64_t(uint64_t(a) * uint64_t(b)); }
inline int64_t add(int64_t a, int64_t b) { return int64_t(uint64_t(a) + uint64_t(b)); }
inline double lerp(double t, double a, double b) { return a + t * (b - a); }
inline double fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
constexpr int STONE = 1, GRASS = 2, DIRT = 3, BEDROCK = 7, WATER_MOVING = 8, WATER = 9, LAVA_MOVING = 10,
              SAND = 12, GRAVEL = 13, ICE = 79;

const float* table() {
    static float t[65536];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < 65536; ++i) t[i] = float(std::sin(i * 3.141592653589793 * 2.0 / 65536.0));
        built = true;
    }
    return t;
}
} // namespace

float sin(float v) { return table()[int(v * 10430.38f) & 65535]; }
float cos(float v) { return table()[int(v * 10430.38f + 16384.0f) & 65535]; }

// ---------------------------------------------------------------- NoiseGeneratorPerlin

NoisePerlin::NoisePerlin(JavaRandom& r) {
    xCoord = r.nextDouble() * 256.0;
    yCoord = r.nextDouble() * 256.0;
    zCoord = r.nextDouble() * 256.0;
    for (int i = 0; i < 256; ++i) p[i] = i;
    for (int j = 0; j < 256; ++j) {
        int k = r.nextInt(256 - j) + j;
        int l = p[j];
        p[j] = p[k];
        p[k] = l;
        p[j + 256] = p[j];
    }
}

double NoisePerlin::grad(int i, double x, double y, double z) {
    int j = i & 15;
    double a = j >= 8 ? y : x;
    double b = j >= 4 ? (j != 12 && j != 14 ? z : x) : y;
    return ((j & 1) != 0 ? -a : a) + ((j & 2) != 0 ? -b : b);
}

double NoisePerlin::grad2(int i, double x, double z) {
    int j = i & 15;
    double a = (1 - ((j & 8) >> 3)) * x;
    double b = j >= 4 ? (j != 12 && j != 14 ? z : x) : 0.0;
    return ((j & 1) != 0 ? -a : a) + ((j & 2) != 0 ? -b : b);
}

double NoisePerlin::generateNoise(double x, double y, double z) const {
    double d3 = x + xCoord, d4 = y + yCoord, d5 = z + zCoord;
    int i = int(d3), j = int(d4), k = int(d5);
    if (d3 < i) i--;
    if (d4 < j) j--;
    if (d5 < k) k--;
    int l = i & 255, i1 = j & 255, j1 = k & 255;
    d3 -= i;
    d4 -= j;
    d5 -= k;
    double d6 = fade(d3), d7 = fade(d4), d8 = fade(d5);
    int k1 = p[l] + i1, l1 = p[k1] + j1, i2 = p[k1 + 1] + j1;
    int j2 = p[l + 1] + i1, k2 = p[j2] + j1, l2 = p[j2 + 1] + j1;
    return lerp(d8,
                lerp(d7, lerp(d6, grad(p[l1], d3, d4, d5), grad(p[k2], d3 - 1.0, d4, d5)),
                     lerp(d6, grad(p[i2], d3, d4 - 1.0, d5), grad(p[l2], d3 - 1.0, d4 - 1.0, d5))),
                lerp(d7, lerp(d6, grad(p[l1 + 1], d3, d4, d5 - 1.0), grad(p[k2 + 1], d3 - 1.0, d4, d5 - 1.0)),
                     lerp(d6, grad(p[i2 + 1], d3, d4 - 1.0, d5 - 1.0),
                          grad(p[l2 + 1], d3 - 1.0, d4 - 1.0, d5 - 1.0))));
}

void NoisePerlin::add3D(double* out, double x, double y, double z, int sx, int sy, int sz, double fx, double fy,
                        double fz, double amp) const {
    int n = 0;
    double inv = 1.0 / amp;
    int last = -1;
    double a = 0, b = 0, c = 0, d = 0;
    for (int ix = 0; ix < sx; ++ix) {
        double px = (x + ix) * fx + xCoord;
        int ipx = int(px);
        if (px < ipx) ipx--;
        int X = ipx & 255;
        px -= ipx;
        double u = fade(px);
        for (int iz = 0; iz < sz; ++iz) {
            double pz = (z + iz) * fz + zCoord;
            int ipz = int(pz);
            if (pz < ipz) ipz--;
            int Z = ipz & 255;
            pz -= ipz;
            double w = fade(pz);
            for (int iy = 0; iy < sy; ++iy) {
                double py = (y + iy) * fy + yCoord;
                int ipy = int(py);
                if (py < ipy) ipy--;
                int Y = ipy & 255;
                py -= ipy;
                double v = fade(py);
                if (iy == 0 || Y != last) {
                    last = Y;
                    int j1 = p[X] + Y, k1 = p[j1] + Z, l1 = p[j1 + 1] + Z;
                    int i2 = p[X + 1] + Y, j2 = p[i2] + Z, k2 = p[i2 + 1] + Z;
                    a = lerp(u, grad(p[k1], px, py, pz), grad(p[j2], px - 1.0, py, pz));
                    b = lerp(u, grad(p[l1], px, py - 1.0, pz), grad(p[k2], px - 1.0, py - 1.0, pz));
                    c = lerp(u, grad(p[k1 + 1], px, py, pz - 1.0), grad(p[j2 + 1], px - 1.0, py, pz - 1.0));
                    d = lerp(u, grad(p[l1 + 1], px, py - 1.0, pz - 1.0),
                             grad(p[k2 + 1], px - 1.0, py - 1.0, pz - 1.0));
                }
                double e = lerp(v, a, b), f = lerp(v, c, d);
                out[n++] += lerp(w, e, f) * inv;
            }
        }
    }
}

void NoisePerlin::add2D(double* out, double x, double y, double z, int sx, int sy, int sz, double fx, double fy,
                        double fz, double amp) const {
    if (sy == 1) {
        int n = 0;
        double inv = 1.0 / amp;
        for (int ix = 0; ix < sx; ++ix) {
            double px = (x + ix) * fx + xCoord;
            int ipx = int(px);
            if (px < ipx) ipx--;
            int X = ipx & 255;
            px -= ipx;
            double u = fade(px);
            for (int iz = 0; iz < sz; ++iz) {
                double pz = (z + iz) * fz + zCoord;
                int ipz = int(pz);
                if (pz < ipz) ipz--;
                int Z = ipz & 255;
                pz -= ipz;
                double w = fade(pz);
                int l = p[X] + 0, j1 = p[l] + Z, k1 = p[X + 1] + 0, l1 = p[k1] + Z;
                double a = lerp(u, grad2(p[j1], px, pz), grad(p[l1], px - 1.0, 0.0, pz));
                double b = lerp(u, grad(p[j1 + 1], px, 0.0, pz - 1.0), grad(p[l1 + 1], px - 1.0, 0.0, pz - 1.0));
                out[n++] += lerp(w, a, b) * inv;
            }
        }
    } else if (sz == 1) {
        // the jar's own quirk: walks y as if it were z, with zCoord as the offset
        int n = 0;
        double inv = 1.0 / amp;
        for (int ix = 0; ix < sx; ++ix) {
            double px = (x + ix) * fx + xCoord;
            int ipx = int(px);
            if (px < ipx) ipx--;
            int X = ipx & 255;
            px -= ipx;
            double u = fade(px);
            for (int iy = 0; iy < sy; ++iy) {
                double pz = (y + iy) * fy + zCoord;
                int ipz = int(pz);
                if (pz < ipz) ipz--;
                int Z = ipz & 255;
                pz -= ipz;
                double w = fade(pz);
                int l = p[X] + 0, j1 = p[l] + Z, k1 = p[X + 1] + 0, l1 = p[k1] + Z;
                double a = lerp(u, grad2(p[j1], px, pz), grad(p[l1], px - 1.0, 0.0, pz));
                double b = lerp(u, grad(p[j1 + 1], px, 0.0, pz - 1.0), grad(p[l1 + 1], px - 1.0, 0.0, pz - 1.0));
                out[n++] += lerp(w, a, b) * inv;
            }
        }
    } else {
        add3D(out, x, y, z, sx, sy, sz, fx, fy, fz, amp);
    }
}

// ---------------------------------------------------------------- NoiseGeneratorOctaves

NoiseOctaves::NoiseOctaves(JavaRandom& r, int octaves) {
    gens.reserve(octaves);
    for (int i = 0; i < octaves; ++i) gens.emplace_back(r);
}

double NoiseOctaves::getVal(double x, double z) const {
    double v = 0.0, s = 1.0;
    for (const auto& g : gens) {
        v += g.getVal(x * s, z * s) / s;
        s /= 2.0;
    }
    return v;
}

void NoiseOctaves::generate(std::vector<double>& out, double x, double y, double z, int sx, int sy, int sz,
                            double fx, double fy, double fz) const {
    out.assign(size_t(sx) * sy * sz, 0.0);
    double s = 1.0;
    for (const auto& g : gens) {
        g.add3D(out.data(), x, y, z, sx, sy, sz, fx * s, fy * s, fz * s, s);
        s /= 2.0;
    }
}

void NoiseOctaves::generate2(std::vector<double>& out, double x, double y, double z, int sx, int sy, int sz,
                             double fx, double fy, double fz) const {
    out.assign(size_t(sx) * sy * sz, 0.0);
    double s = 1.0;
    for (const auto& g : gens) {
        g.add2D(out.data(), x, y, z, sx, sy, sz, fx * s, fy * s, fz * s, s);
        s /= 2.0;
    }
}

// ---------------------------------------------------------------- ChunkProviderGenerate

Terrain::Terrain(const GenWorld& w)
    : world(w), genRandom(w.seed), noiseGen1(genRandom, 16), noiseGen2(genRandom, 16), noiseGen3(genRandom, 8),
      beachNoise(genRandom, 4), soilNoise(genRandom, 4), noiseGen6(genRandom, 10), noiseGen7(genRandom, 16),
      treeNoise(genRandom, 8) {}

void Terrain::provideChunk(int cx, int cz, uint8_t* blocks) {
    genRandom.setSeed(add(mul(cx, 341873128712LL), mul(cz, 132897987541LL)));
    for (int i = 0; i < 32768; ++i) blocks[i] = 0;
    generateTerrain(cx, cz, blocks);
    replaceBlocksForBiome(cx, cz, blocks);
    caves(cx, cz, blocks);
}

void Terrain::generateTerrain(int cx, int cz, uint8_t* b) {
    const int n = 4, sea = 64, sx = n + 1, sy = 17, sz = n + 1;
    initializeNoiseField(cx * n, 0, cz * n, sx, sy, sz);
    const double* D = densities.data();
    for (int gx = 0; gx < n; ++gx)
        for (int gz = 0; gz < n; ++gz)
            for (int gy = 0; gy < 16; ++gy) {
                double d = 0.125;
                double d1 = D[((gx + 0) * sz + gz + 0) * sy + gy + 0];
                double d2 = D[((gx + 0) * sz + gz + 1) * sy + gy + 0];
                double d3 = D[((gx + 1) * sz + gz + 0) * sy + gy + 0];
                double d4 = D[((gx + 1) * sz + gz + 1) * sy + gy + 0];
                double d5 = (D[((gx + 0) * sz + gz + 0) * sy + gy + 1] - d1) * d;
                double d6 = (D[((gx + 0) * sz + gz + 1) * sy + gy + 1] - d2) * d;
                double d7 = (D[((gx + 1) * sz + gz + 0) * sy + gy + 1] - d3) * d;
                double d8 = (D[((gx + 1) * sz + gz + 1) * sy + gy + 1] - d4) * d;
                for (int l1 = 0; l1 < 8; ++l1) {
                    double d9 = 0.25, d10 = d1, d11 = d2;
                    double d12 = (d3 - d1) * d9, d13 = (d4 - d2) * d9;
                    for (int i2 = 0; i2 < 4; ++i2) {
                        int j2 = (i2 + gx * 4) << 11 | (0 + gz * 4) << 7 | (gy * 8 + l1);
                        double d14 = 0.25, d15 = d10, d16 = (d11 - d10) * d14;
                        for (int k2 = 0; k2 < 4; ++k2) {
                            int y = gy * 8 + l1, id = 0;
                            if (y < sea) id = (world.winter && y >= sea - 1) ? ICE : WATER;
                            if (d15 > 0.0) id = STONE;
                            b[j2] = uint8_t(id);
                            j2 += 128;
                            d15 += d16;
                        }
                        d10 += d12;
                        d11 += d13;
                    }
                    d1 += d5;
                    d2 += d6;
                    d3 += d7;
                    d4 += d8;
                }
            }
}

void Terrain::replaceBlocksForBiome(int cx, int cz, uint8_t* b) {
    const int sea = 64;
    const double d = 0.03125;
    beachNoise.generate2(sandBeach, cx * 16, cz * 16, 0.0, 16, 16, 1, d, d, 1.0);
    beachNoise.generate2(gravelBeach, cz * 16, 109.0134, cx * 16, 16, 1, 16, d, 1.0, d);
    soilNoise.generate2(soilThickness, cx * 16, cz * 16, 0.0, 16, 16, 1, d * 2.0, d * 2.0, d * 2.0);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            bool sandy = sandBeach[z + x * 16] + genRandom.nextDouble() * 0.2 > 0.0;
            bool gravelly = gravelBeach[x + z * 16] + genRandom.nextDouble() * 0.2 > 3.0;
            int soil = int(soilThickness[z + x * 16] / 3.0 + 3.0 + genRandom.nextDouble() * 0.25);
            int run = -1;
            uint8_t top = GRASS, filler = DIRT;
            for (int y = 127; y >= 0; --y) {
                int i = (x * 16 + z) * 128 + y;
                if (y <= 0 + genRandom.nextInt(6) - 1) {
                    b[i] = BEDROCK;
                    continue;
                }
                uint8_t id = b[i];
                if (id == 0) {
                    run = -1;
                } else if (id == STONE) {
                    if (run == -1) {
                        if (soil <= 0) {
                            top = 0;
                            filler = STONE;
                        } else if (y >= sea - 4 && y <= sea + 1) {
                            top = GRASS;
                            filler = DIRT;
                            if (gravelly) top = 0;
                            if (gravelly) filler = GRAVEL;
                            if (sandy) top = SAND;
                            if (sandy) filler = SAND;
                        }
                        if (y < sea && top == 0) top = WATER;
                        run = soil;
                        b[i] = y >= sea - 1 ? top : filler;
                    } else if (run > 0) {
                        run--;
                        b[i] = filler;
                    }
                }
            }
        }
}

void Terrain::initializeNoiseField(int x, int y, int z, int sx, int sy, int sz) {
    const double scaleX = 80.0, scaleY = 160.0, scaleZ = 80.0, floatyness = 0.0;
    densities.assign(size_t(sx) * sy * sz, 0.0);
    const double coord = 684.412, height = 684.412;
    noiseGen6.generate(hb1, x, y, z, sx, 1, sz, 1.0, 0.0, 1.0);
    noiseGen7.generate(hb2, x, y, z, sx, 1, sz, 100.0, 0.0, 100.0);
    noiseGen3.generate(nd1, x, y, z, sx, sy, sz, coord / scaleX, height / scaleY, coord / scaleZ);
    noiseGen1.generate(nd2, x, y, z, sx, sy, sz, coord, height, coord);
    noiseGen2.generate(nd3, x, y, z, sx, sy, sz, coord, height, coord);
    int k1 = 0, l1 = 0;
    for (int i2 = 0; i2 < sx; ++i2)
        for (int j2 = 0; j2 < sz; ++j2) {
            double mono = (hb1[l1] + 256.0) / 512.0;
            if (mono > 1.0) mono = 1.0;
            double d3 = floatyness;
            double bias = hb2[l1] / 8000.0;
            if (bias < 0.0) bias = -bias;
            bias = bias * 3.0 - 3.0;
            if (bias < 0.0) {
                bias /= 2.0;
                if (bias < -1.0) bias = -1.0;
                bias /= 1.4;
                bias /= 2.0;
                mono = 0.0;
            } else {
                if (bias > 1.0) bias = 1.0;
                bias /= 6.0;
            }
            mono += 0.5;
            bias = bias * sy / 16.0;
            double d5 = sy / 2.0 + bias * 4.0;
            l1++;
            for (int k2 = 0; k2 < sy; ++k2) {
                double d6 = 0.0;
                double d7 = (k2 - d5) * 12.0 / mono;
                if (d7 < 0.0) d7 *= 4.0;
                double d8 = nd2[k1] / 512.0, d9 = nd3[k1] / 512.0, d10 = (nd1[k1] / 10.0 + 1.0) / 2.0;
                if (d10 < 0.0) d6 = d8;
                else if (d10 > 1.0) d6 = d9;
                else d6 = d8 + (d9 - d8) * d10;
                d6 -= d7;
                if (k2 > sy - 4) {
                    double d11 = float(k2 - (sy - 4)) / 3.0f;
                    d6 = d6 * (1.0 - d11) + -10.0 * d11;
                }
                if (k2 < d3) {
                    double d12 = (d3 - k2) / 4.0;
                    if (d12 < 0.0) d12 = 0.0;
                    if (d12 > 1.0) d12 = 1.0;
                    d6 = d6 * (1.0 - d12) + -10.0 * d12;
                }
                densities[k1] = d6;
                k1++;
            }
        }
}

// ---------------------------------------------------------------- MapGenCaves

void Terrain::caves(int cx, int cz, uint8_t* b) {
    const int k = 8;
    caveRand.setSeed(world.seed);
    int64_t l = caveRand.nextLong() / 2 * 2 + 1;
    int64_t l1 = caveRand.nextLong() / 2 * 2 + 1;
    for (int i1 = cx - k; i1 <= cx + k; ++i1)
        for (int j1 = cz - k; j1 <= cz + k; ++j1) {
            caveRand.setSeed(add(mul(i1, l), mul(j1, l1)) ^ world.seed);
            caveStart(i1, j1, cx, cz, b);
        }
}

void Terrain::caveStart(int i, int j, int k, int l, uint8_t* b) {
    int a = caveRand.nextInt(40) + 1;
    int c = caveRand.nextInt(a) + 1;
    int i1 = caveRand.nextInt(c);
    if (caveRand.nextInt(15) != 0) i1 = 0;
    for (int j1 = 0; j1 < i1; ++j1) {
        double d = i * 16 + caveRand.nextInt(16);
        int h = caveRand.nextInt(120) + 8;
        double d1 = caveRand.nextInt(h);
        double d2 = j * 16 + caveRand.nextInt(16);
        int k1 = 1;
        if (caveRand.nextInt(4) == 0) {
            float w = 1.0f + caveRand.nextFloat() * 6.0f; // func_870_a
            tunnel(k, l, b, d, d1, d2, w, 0.0f, 0.0f, -1, -1, 0.5);
            k1 += caveRand.nextInt(4);
        }
        for (int l1 = 0; l1 < k1; ++l1) {
            float f = caveRand.nextFloat() * 3.141593f * 2.0f;
            float f1 = (caveRand.nextFloat() - 0.5f) * 2.0f / 8.0f;
            float f2a = caveRand.nextFloat() * 2.0f;
            float f2 = f2a + caveRand.nextFloat();
            tunnel(k, l, b, d, d1, d2, f2, f, f1, 0, 0, 1.0);
        }
    }
}

void Terrain::tunnel(int i, int j, uint8_t* abyte0, double d, double d1, double d2, float f, float f1, float f2,
                     int k, int l, double d3) {
    double d4 = i * 16 + 8, d5 = j * 16 + 8;
    float f3 = 0.0f, f4 = 0.0f;
    JavaRandom random(caveRand.nextLong());
    if (l <= 0) {
        int i1 = 8 * 16 - 16;
        l = i1 - random.nextInt(i1 / 4);
    }
    bool flag = false;
    if (k == -1) {
        k = l / 2;
        flag = true;
    }
    int j1 = random.nextInt(l / 2) + l / 4;
    for (bool flag1 = random.nextInt(6) == 0; k < l; ++k) {
        double d6 = 1.5 + sin(float(k) * 3.141593f / float(l)) * f * 1.0f;
        double d7 = d6 * d3;
        float f5 = cos(f2), f6 = sin(f2);
        d += cos(f1) * f5;
        d1 += f6;
        d2 += sin(f1) * f5;
        if (flag1) f2 *= 0.92f;
        else f2 *= 0.7f;
        f2 += f4 * 0.1f;
        f1 += f3 * 0.1f;
        f4 *= 0.9f;
        f3 *= 0.75f;
        {
            float r1 = random.nextFloat();
            float r2 = random.nextFloat();
            float r3 = random.nextFloat();
            f4 += (r1 - r2) * r3 * 2.0f;
        }
        {
            float r1 = random.nextFloat();
            float r2 = random.nextFloat();
            float r3 = random.nextFloat();
            f3 += (r1 - r2) * r3 * 4.0f;
        }
        if (!flag && k == j1 && f > 1.0f) {
            float wa = random.nextFloat() * 0.5f + 0.5f;
            tunnel(i, j, abyte0, d, d1, d2, wa, f1 - 1.570796f, f2 / 3.0f, k, l, 1.0);
            float wb = random.nextFloat() * 0.5f + 0.5f;
            tunnel(i, j, abyte0, d, d1, d2, wb, f1 + 1.570796f, f2 / 3.0f, k, l, 1.0);
            return;
        }
        if (flag || random.nextInt(4) != 0) {
            double d8 = d - d4, d9 = d2 - d5, d10 = l - k;
            double d11 = f + 2.0f + 16.0f;
            if (d8 * d8 + d9 * d9 - d10 * d10 > d11 * d11) return;
            if (!(d < d4 - 16.0 - d6 * 2.0) && !(d2 < d5 - 16.0 - d6 * 2.0) && !(d > d4 + 16.0 + d6 * 2.0) &&
                !(d2 > d5 + 16.0 + d6 * 2.0)) {
                d8 = floor_double(d - d6) - i * 16 - 1;
                int k1 = floor_double(d + d6) - i * 16 + 1;
                d9 = floor_double(d1 - d7) - 1;
                int l1 = floor_double(d1 + d7) + 1;
                d10 = floor_double(d2 - d6) - j * 16 - 1;
                int i2 = floor_double(d2 + d6) - j * 16 + 1;
                if (d8 < 0.0) d8 = 0.0;
                if (k1 > 16) k1 = 16;
                if (d9 < 1.0) d9 = 1.0;
                if (l1 > 120) l1 = 120;
                if (d10 < 0.0) d10 = 0.0;
                if (i2 > 16) i2 = 16;
                bool flag2 = false;
                for (int j2 = int(d8); !flag2 && j2 < k1; ++j2)
                    for (int l2 = int(d10); !flag2 && l2 < i2; ++l2)
                        for (int i3 = l1 + 1; !flag2 && i3 >= d9 - 1.0; --i3) {
                            int j3 = (j2 * 16 + l2) * 128 + i3;
                            if (i3 >= 0 && i3 < 128) {
                                if (abyte0[j3] == WATER_MOVING || abyte0[j3] == WATER) flag2 = true;
                                if (i3 != d9 - 1.0 && j2 != d8 && j2 != k1 - 1 && l2 != d10 && l2 != i2 - 1)
                                    i3 = int(d9);
                            }
                        }
                if (!flag2) {
                    for (int k2 = int(d8); k2 < k1; ++k2) {
                        double d12 = (k2 + i * 16 + 0.5 - d) / d6;
                        for (int k3 = int(d10); k3 < i2; ++k3) {
                            double d13 = (k3 + j * 16 + 0.5 - d2) / d6;
                            int l3 = (k2 * 16 + k3) * 128 + l1;
                            bool flag3 = false;
                            for (int i4 = l1 - 1; i4 >= d9; --i4) {
                                double d14 = (i4 + 0.5 - d1) / d7;
                                if (d14 > -0.7 && d12 * d12 + d14 * d14 + d13 * d13 < 1.0) {
                                    uint8_t byte0 = abyte0[l3];
                                    if (byte0 == GRASS) flag3 = true;
                                    if (byte0 == STONE || byte0 == DIRT || byte0 == GRASS) {
                                        if (i4 < 10) {
                                            abyte0[l3] = LAVA_MOVING;
                                        } else {
                                            abyte0[l3] = 0;
                                            if (flag3 && abyte0[l3 - 1] == DIRT) abyte0[l3 - 1] = GRASS;
                                        }
                                    }
                                }
                                l3--;
                            }
                        }
                    }
                    if (flag) break;
                }
            }
        }
    }
}

} // namespace alpha
