#include "ChunkGenerator.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "world/MathHelper.h"

// Java evaluates arguments and operands left to right; C++ doesn't promise that, so every
// expression below with more than one random call is split into separate statements.

namespace {
const float PI_F = 3.1415927f; // (float)Math.PI
const double PI = 3.141592653589793;
inline int64_t mul(int64_t a, int64_t b) { return int64_t(uint64_t(a) * uint64_t(b)); }
inline int64_t add(int64_t a, int64_t b) { return int64_t(uint64_t(a) + uint64_t(b)); }
inline int jfloor(double v) { return int(std::floor(v)); }
inline int64_t jround(double v) { return int64_t(std::floor(v + 0.5)); } // Math.round

// ChunkProviderGenerate's Axeolon "extreme terrain"
const double EXTREME_SCALE = 0.032, EXTREME_THRESHOLD = 0.37, EXTREME_WIDTH = 0.4, EXTREME_STRENGTH = 3.0,
             EXTREME_DEPTH = 2.0, EXTREME_HEADROOM = 3.0;
double extremeness(double noise) {
    double t = (noise - EXTREME_THRESHOLD) / EXTREME_WIDTH;
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);
}
} // namespace

// ================================================================ AxeolonBlend

void Blend::init(const std::string& dir) {
    old.clear();
    any = false;
    if (dir.empty()) return;
    std::string path = dir + "/axeolon_blend.dat";
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f) {
        uint8_t b[8];
        if (std::fread(b, 1, 4, f) == 4) {
            int32_t n = int32_t(uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3]);
            for (int i = 0; i < n && std::fread(b, 1, 8, f) == 8; ++i) {
                uint64_t v = 0;
                for (int k = 0; k < 8; ++k) v = v << 8 | b[k];
                old.insert(int64_t(v));
            }
        }
        std::fclose(f);
    } else {
        // Beta 1.2 c.*.dat chunk files would be listed here; this port only reads region worlds,
        // where Axeolon's scan finds nothing, so a new list is always empty.
        f = std::fopen(path.c_str(), "wb");
        if (f) {
            uint8_t zero[4] = {0, 0, 0, 0};
            std::fwrite(zero, 1, 4, f);
            std::fclose(f);
        }
    }
    any = !old.empty();
}

double Blend::factor(int x, int z) const {
    if (!any) return 1.0;
    const int RADIUS = 4;
    int cx = x >> 4, cz = z >> 4;
    double best = DBL_MAX;
    for (int dx = -RADIUS; dx <= RADIUS; ++dx)
        for (int dz = -RADIUS; dz <= RADIUS; ++dz) {
            if (!old.count(key(cx + dx, cz + dz))) continue;
            int x0 = (cx + dx) * 16, z0 = (cz + dz) * 16;
            int ex = x < x0 ? x0 - x : (x > x0 + 15 ? x - x0 - 15 : 0);
            int ez = z < z0 ? z0 - z : (z > z0 + 15 ? z - z0 - 15 : 0);
            double d = std::sqrt(double(ex * ex + ez * ez));
            if (d < best) best = d;
        }
    if (best == DBL_MAX) return 1.0;
    double t = (best - 1.0) / double(RADIUS * 16 - 8);
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    return t * t * (3.0 - 2.0 * t);
}

// ================================================================ AxeolonPeaks

Peaks::Peaks(int64_t s, const Blend& b) : seed(s), blend(b) {
    JavaRandom r(seed ^ int64_t(0x9E3779B97F4A7C15ULL));
    for (int i = 0; i < 256; ++i) perm[i] = i;
    for (int i = 255; i > 0; --i) {
        int j = r.nextInt(i + 1);
        int t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }
    for (int i = 0; i < 256; ++i) perm[256 + i] = perm[i];
}

namespace {
inline double pgrad(int h, double x, double y) {
    switch (h & 7) {
    case 0: return x + y;
    case 1: return -x + y;
    case 2: return x - y;
    case 3: return -x - y;
    case 4: return x;
    case 5: return -x;
    case 6: return y;
    default: return -y;
    }
}
inline double smooth(double a, double b, double v) {
    double t = std::max(0.0, std::min(1.0, (v - a) / (b - a)));
    return t * t * (3.0 - 2.0 * t);
}
} // namespace

double Peaks::noise(double x, double y, int salt) const {
    const int* p = perm;
    int xi = jfloor(x), yi = jfloor(y);
    double xf = x - xi, yf = y - yi;
    double u = xf * xf * xf * (xf * (xf * 6 - 15) + 10), v = yf * yf * yf * (yf * (yf * 6 - 15) + 10);
    int X = (xi + salt * 31) & 255, Y = (yi + salt * 57) & 255;
    double a = pgrad(p[p[X] + Y], xf, yf), b = pgrad(p[p[X + 1] + Y], xf - 1, yf);
    double c = pgrad(p[p[X] + Y + 1], xf, yf - 1), d = pgrad(p[p[X + 1] + Y + 1], xf - 1, yf - 1);
    return ((a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v) * 1.4142;
}

double Peaks::region(int x, int z) const {
    const double SCALE = 700.0;
    double n = 0.5 + 0.5 * (noise(x / SCALE, z / SCALE, 1) * 0.75 +
                            noise(x / (SCALE * 0.4), z / (SCALE * 0.4), 2) * 0.25);
    return smooth(0.64, 0.80, n);
}

double Peaks::ridged(double x, double z, int salt) const {
    double r = 1.0 - std::fabs(noise(x, z, salt));
    return r * r;
}

double Peaks::lift(int x, int z) const {
    double m = region(x, z);
    if (m <= 0.0) return 0.0;
    double shape = 0.55 * ridged(x / 150.0, z / 150.0, 3) + 0.30 * ridged(x / 62.0, z / 62.0, 4) +
                   0.15 * ridged(x / 26.0, z / 26.0, 5);
    shape = std::pow(shape, 1.6);
    double h = 22.0 + 168.0 * shape;
    double spike = std::max(0.0, noise(x / 9.0, z / 9.0, 6) - 0.55) * 70.0 * shape;
    return m * (h + spike) * blend.factor(x, z);
}

uint8_t Peaks::strata(int x, int y, int z) const {
    double t = y + noise(x / 40.0, z / 40.0, 8) * 5.0;
    int band = (jfloor(t / 3.0) % 9 + 9) % 9;
    if (band == 2) return BlockId::StoneTypes + 6; // andesite
    if (band == 5) return BlockId::StoneTypes + 4; // granite
    if (band == 7 && y > 140) return BlockId::StoneTypes + 5; // diorite
    return BlockId::Stone;
}

void Peaks::raise(uint8_t* blocks, int cx, int cz) const {
    const int SNOW_LINE = 172, TREE_LINE = 150, MAX_HEIGHT = 244;
    int bx = cx * 16, bz = cz * 16;
    double lift[18 * 18];
    bool any = false;
    for (int i = 0; i < 18; ++i)
        for (int k = 0; k < 18; ++k) {
            double l = this->lift(bx + i - 1, bz + k - 1);
            lift[i * 18 + k] = l;
            if (l > 0.5) any = true;
        }
    if (!any) return;
    int base[18 * 18] = {};
    for (int i = 0; i < 16; ++i)
        for (int k = 0; k < 16; ++k) {
            int col = i << 12 | k << 8, top = 1;
            for (int y = 127; y > 0; --y) {
                uint8_t id = blocks[col | y];
                if (id != 0 && materialSolid(id)) { top = y; break; }
            }
            base[(i + 1) * 18 + k + 1] = top;
        }
    for (int i = 0; i < 18; ++i)
        for (int k = 0; k < 18; ++k)
            if (i == 0 || k == 0 || i == 17 || k == 17)
                base[i * 18 + k] = base[std::max(1, std::min(16, i)) * 18 + std::max(1, std::min(16, k))];
    int H[18 * 18];
    for (int j = 0; j < 18 * 18; ++j) H[j] = std::min(MAX_HEIGHT, base[j] + int(jround(lift[j])));
    for (int i = 0; i < 16; ++i)
        for (int k = 0; k < 16; ++k) {
            int g = (i + 1) * 18 + k + 1;
            int s = base[g], h = H[g];
            if (h <= s) continue;
            int col = i << 12 | k << 8;
            int x = bx + i, z = bz + k;
            int slope = 0;
            const int nb[4] = {g - 18, g + 18, g - 1, g + 1};
            for (int q = 0; q < 4; ++q) slope = std::max(slope, h - H[nb[q]]);
            int snowLine = SNOW_LINE + int(noise(x / 23.0, z / 23.0, 7) * 6.0);
            if (blocks[col | s] == BlockId::Grass) blocks[col | s] = BlockId::Dirt;
            for (int y = s + 1; y <= h; ++y) blocks[col | y] = strata(x, y, z);
            if (h >= snowLine) {
                blocks[col | h] = BlockId::SnowBlock;
                if (slope < 4 && h - 1 > s) blocks[col | (h - 1)] = BlockId::SnowBlock;
                if (h + 1 < 256) blocks[col | (h + 1)] = BlockId::Snow;
            } else if (slope >= 3 || h > TREE_LINE) {
                // bare cliff
            } else {
                blocks[col | h] = BlockId::Grass;
                for (int y = h - 1; y >= h - 3 && y > s; --y) blocks[col | y] = BlockId::Dirt;
            }
        }
}

// ================================================================ WorldChunkManager

ChunkManager::ChunkManager(int64_t seed, const Peaks& p)
    : tempNoise(JavaRandom(mul(seed, 9871)), 4), humNoise(JavaRandom(mul(seed, 39811)), 4),
      mixNoise(JavaRandom(mul(seed, 543321)), 2), peaks(p) {}

const BiomeOverride* ChunkManager::findOverride(int x, int z) const {
    for (int i = int(overrides.size()) - 1; i >= 0; --i)
        if (overrides[i].contains(x, z)) return &overrides[i];
    return nullptr;
}

void ChunkManager::load(std::vector<Biome>& out, int x, int z, int w, int h) {
    if (out.size() < size_t(w) * h) out.resize(size_t(w) * h);
    tempNoise.generate(temperature, x, z, w, w, double(0.025f), double(0.025f), 0.25);
    humNoise.generate(humidity, x, z, w, w, double(0.05f), double(0.05f), 1.0 / 3.0);
    mixNoise.generate(field_4196_c, x, z, w, w, 0.25, 0.25, 0.5882352941176471);
    int n = 0;
    for (int i = 0; i < w; ++i)
        for (int k = 0; k < h; ++k) {
            double mix = field_4196_c[n] * 1.1 + 0.5;
            double a = 0.01, b = 1.0 - a;
            double t = (temperature[n] * 0.15 + 0.7) * b + mix * a;
            a = 0.002;
            b = 1.0 - a;
            double hu = (humidity[n] * 0.15 + 0.5) * b + mix * a;
            t = 1.0 - (1.0 - t) * (1.0 - t);
            if (t < 0.0) t = 0.0;
            if (hu < 0.0) hu = 0.0;
            if (t > 1.0) t = 1.0;
            if (hu > 1.0) hu = 1.0;
            const BiomeOverride* o = overrides.empty() ? nullptr : findOverride(x + i, z + k);
            if (o) {
                temperature[n] = o->temperature;
                humidity[n] = o->humidity;
                out[n++] = o->biome;
                continue;
            }
            temperature[n] = t;
            humidity[n] = hu;
            out[n++] = peaks.isPeaks(x + i, z + k) ? Biome::Peaks : Biomes::lookup(t, hu);
        }
}

double ChunkManager::rawTemperature(int x, int z) const {
    std::vector<double> t;
    tempNoise.generate(t, double(x), double(z), 1, 1, double(0.025f), double(0.025f), 0.5);
    return t[0];
}

void ChunkManager::getTemperatures(std::vector<double>& out, int x, int z, int w, int h) {
    if (out.size() < size_t(w) * h) out.assign(size_t(w) * h, 0.0);
    tempNoise.generate(out, x, z, w, w, double(0.025f), double(0.025f), 0.25);
    mixNoise.generate(field_4196_c, x, z, w, w, 0.25, 0.25, 0.5882352941176471);
    int n = 0;
    for (int i = 0; i < w; ++i)
        for (int k = 0; k < h; ++k) {
            double mix = field_4196_c[n] * 1.1 + 0.5;
            double a = 0.01, b = 1.0 - a;
            double t = (out[n] * 0.15 + 0.7) * b + mix * a;
            t = 1.0 - (1.0 - t) * (1.0 - t);
            if (t < 0.0) t = 0.0;
            if (t > 1.0) t = 1.0;
            const BiomeOverride* o = overrides.empty() ? nullptr : findOverride(x + i, z + k);
            out[n] = o ? o->temperature : t;
            ++n;
        }
}

Biome ChunkManager::biomeAt(int x, int z) {
    std::vector<Biome> one;
    load(one, x, z, 1, 1);
    return one[0];
}

// ================================================================ ChunkProviderGenerate

ChunkGenerator::ChunkGenerator(int64_t s, const std::string& worldDir, int dimension)
    : seed(s), rand(s), n912k(rand, 16), n911l(rand, 16), n910m(rand, 8), n909n(rand, 4), n908o(rand, 4),
      n922a(rand, 10), n921b(rand, 16), mobNoise(rand, 8),
      extremeNoise(JavaRandom(add(mul(s, 0x5DEECE66DLL), 0x2545F491LL)), 2) {
    blend.init(worldDir);
    peaks.reset(new Peaks(seed, blend));
    manager.reset(new ChunkManager(seed, *peaks));
    if (dimension == -1) hell.reset(new NetherTerrain(seed));
}

ChunkGenerator::~ChunkGenerator() = default;

void ChunkGenerator::provideChunk(int cx, int cz, uint8_t* out) {
    if (hell) {
        hell->provideChunk(cx, cz, out);
        return;
    }
    rand.setSeed(add(mul(cx, 341873128712LL), mul(cz, 132897987541LL)));
    uint8_t* b = scratch; // per generator, so a second one can run on the other core
    std::memset(b, 0, 32768);
    manager->load(biomes, cx * 16, cz * 16, 16, 16);
    generateTerrain(cx, cz, b);
    replaceBlocksForBiome(cx, cz, b);
    mapGen(0, cx, cz, b);
    mapGen(1, cx, cz, b);
    stoneTypes(b, cx, cz);
    // Chunk.expandBlocks: 128 tall into the 256-tall layout
    std::memset(out, 0, 65536);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) std::memcpy(out + (x << 12 | z << 8), b + (x << 11 | z << 7), 128);
    peaks->raise(out, cx, cz);
}

void ChunkGenerator::heightNoise(int x, int y, int z, int sx, int sy, int sz) {
    heightField.assign(size_t(sx) * sy * sz, 0.0);
    const double A = 684.412, B = 684.412;
    const std::vector<double>& temp = manager->temperature;
    const std::vector<double>& hum = manager->humidity;
    n922a.generate2D(d4182, x, z, sx, sz, 1.121, 1.121, 0.5);
    n921b.generate2D(d4181, x, z, sx, sz, 200.0, 200.0, 0.5);
    n910m.generate(d4185, x, y, z, sx, sy, sz, A / 80.0, B / 160.0, A / 80.0);
    n912k.generate(d4184, x, y, z, sx, sy, sz, A, B, A);
    n911l.generate(d4183, x, y, z, sx, sy, sz, A, B, A);
    extremeNoise.generate(extreme, x, 0.0, z, sx, 1, sz, EXTREME_SCALE, 1.0, EXTREME_SCALE);
    int n = 0, m = 0;
    int step = 16 / sx;
    for (int i = 0; i < sx; ++i) {
        int ti = i * step + step / 2;
        for (int k = 0; k < sz; ++k) {
            int tk = k * step + step / 2;
            double t = temp[ti * 16 + tk];
            double h = hum[ti * 16 + tk] * t;
            double d = 1.0 - h;
            d *= d;
            d *= d;
            d = 1.0 - d;
            double scale = (d4182[m] + 256.0) / 512.0;
            scale *= d;
            if (scale > 1.0) scale = 1.0;
            double depth = d4181[m] / 8000.0;
            if (depth < 0.0) depth = -depth * 0.3;
            depth = depth * 3.0 - 2.0;
            if (depth < 0.0) {
                depth /= 2.0;
                if (depth < -1.0) depth = -1.0;
                depth /= 1.4;
                depth /= 2.0;
                scale = 0.0;
            } else {
                if (depth > 1.0) depth = 1.0;
                depth /= 8.0;
            }
            if (scale < 0.0) scale = 0.0;
            scale += 0.5;
            depth = depth * double(sy) / 16.0;
            double mid = double(sy) / 2.0 + depth * 4.0;
            double ex = extremeness(extreme[m]);
            scale *= 1.0 + EXTREME_STRENGTH * ex;
            mid += depth * 4.0 * EXTREME_DEPTH * ex;
            ++m;
            for (int j = 0; j < sy; ++j) {
                double v;
                double off = (double(j) - mid) * 12.0 / scale;
                if (off < 0.0) off *= 4.0;
                double lo = d4184[n] / 512.0;
                double hi = d4183[n] / 512.0;
                double sel = (d4185[n] / 10.0 + 1.0) / 2.0;
                if (sel < 0.0) v = lo;
                else if (sel > 1.0) v = hi;
                else v = lo + (hi - lo) * sel;
                v -= off;
                double fadeStart = double(sy - 4) - EXTREME_HEADROOM * ex;
                if (double(j) > fadeStart) {
                    double f = (double(j) - fadeStart) / (double(sy - 1) - fadeStart);
                    if (f > 1.0) f = 1.0;
                    v = v * (1.0 - f) + -10.0 * f;
                }
                heightField[n] = v;
                ++n;
            }
        }
    }
}

void ChunkGenerator::generateTerrain(int cx, int cz, uint8_t* b) {
    const int S = 4, SEA = 64, XS = S + 1, YS = 17, ZS = S + 1;
    heightNoise(cx * S, 0, cz * S, XS, YS, ZS);
    const std::vector<double>& q = heightField;
    const std::vector<double>& temp = manager->temperature;
    for (int i = 0; i < S; ++i)
        for (int k = 0; k < S; ++k)
            for (int j = 0; j < 16; ++j) {
                double ys = 0.125;
                double a = q[((i + 0) * ZS + k + 0) * YS + j];
                double bb = q[((i + 0) * ZS + k + 1) * YS + j];
                double c = q[((i + 1) * ZS + k + 0) * YS + j];
                double d = q[((i + 1) * ZS + k + 1) * YS + j];
                double da = (q[((i + 0) * ZS + k + 0) * YS + j + 1] - a) * ys;
                double db = (q[((i + 0) * ZS + k + 1) * YS + j + 1] - bb) * ys;
                double dc = (q[((i + 1) * ZS + k + 0) * YS + j + 1] - c) * ys;
                double dd = (q[((i + 1) * ZS + k + 1) * YS + j + 1] - d) * ys;
                for (int y8 = 0; y8 < 8; ++y8) {
                    double xs = 0.25;
                    double e = a, f = bb;
                    double de = (c - a) * xs, df = (d - bb) * xs;
                    for (int x4 = 0; x4 < 4; ++x4) {
                        int idx = (x4 + i * 4) << 11 | (0 + k * 4) << 7 | (j * 8 + y8);
                        double zs = 0.25;
                        double v = e, dv = (f - e) * zs;
                        for (int z4 = 0; z4 < 4; ++z4) {
                            double t = temp[(i * 4 + x4) * 16 + k * 4 + z4];
                            int id = 0;
                            if (j * 8 + y8 < SEA) {
                                if (t < 0.5 && j * 8 + y8 >= SEA - 1) id = BlockId::Ice;
                                else id = BlockId::WaterStill;
                            }
                            if (v > 0.0) id = BlockId::Stone;
                            b[idx] = uint8_t(id);
                            idx += 128;
                            v += dv;
                        }
                        e += de;
                        f += df;
                    }
                    a += da;
                    bb += db;
                    c += dc;
                    d += dd;
                }
            }
}

void ChunkGenerator::replaceBlocksForBiome(int cx, int cz, uint8_t* b) {
    const int SEA = 64;
    const double s = 1.0 / 32.0;
    n909n.generate(sandNoise, cx * 16, cz * 16, 0.0, 16, 16, 1, s, s, 1.0);
    n909n.generate(gravelNoise, cz * 16, 109.0134, cx * 16, 16, 1, 16, s, 1.0, s);
    n908o.generate(stoneNoise, cx * 16, cz * 16, 0.0, 16, 16, 1, s * 2.0, s * 2.0, s * 2.0);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            Biome biome = biomes[x + z * 16];
            bool sand = sandNoise[x + z * 16] + rand.nextDouble() * 0.2 > 0.0;
            bool gravel = gravelNoise[x + z * 16] + rand.nextDouble() * 0.2 > 3.0;
            int depth = int(stoneNoise[x + z * 16] / 3.0 + 3.0 + rand.nextDouble() * 0.25);
            int run = -1;
            uint8_t top = Biomes::topBlock(biome), fill = Biomes::fillerBlock(biome);
            for (int y = 127; y >= 0; --y) {
                int idx = (x * 16 + z) * 128 + y;
                if (y <= 0 + rand.nextInt(5)) {
                    b[idx] = BlockId::Bedrock;
                    continue;
                }
                uint8_t id = b[idx];
                if (id == 0) {
                    run = -1;
                } else if (id == BlockId::Stone) {
                    if (run == -1) {
                        if (depth <= 0) {
                            top = 0;
                            fill = BlockId::Stone;
                        } else if (y >= SEA - 4 && y <= SEA + 1) {
                            top = Biomes::topBlock(biome);
                            fill = Biomes::fillerBlock(biome);
                            if (gravel) top = 0;
                            if (gravel) fill = BlockId::Gravel;
                            if (sand) top = BlockId::Sand;
                            if (sand) fill = BlockId::Sand;
                        }
                        if (y < SEA && top == 0) top = BlockId::WaterStill;
                        run = depth;
                        b[idx] = y >= SEA - 1 ? top : fill;
                    } else if (run > 0) {
                        --run;
                        b[idx] = fill;
                        if (run == 0 && fill == BlockId::Sand) {
                            run = rand.nextInt(4);
                            fill = BlockId::Sandstone;
                        }
                    }
                }
            }
        }
}

// ================================================================ MapGenBase / MapGenCaves

void ChunkGenerator::mapGen(int kind, int cx, int cz, uint8_t* b) {
    const int R = 8;
    JavaRandom& r = kind ? ravineRand : caveRand;
    r.setSeed(seed);
    int64_t a = r.nextLong() / 2 * 2 + 1;
    int64_t c = r.nextLong() / 2 * 2 + 1;
    for (int x = cx - R; x <= cx + R; ++x)
        for (int z = cz - R; z <= cz + R; ++z) {
            r.setSeed(add(mul(x, a), mul(z, c)) ^ seed);
            if (kind) ravineChunk(x, z, cx, cz, b);
            else caveChunk(x, z, cx, cz, b);
        }
}

void ChunkGenerator::caveChunk(int chunkX, int chunkZ, int cx, int cz, uint8_t* b) {
    JavaRandom& r = caveRand;
    int n = r.nextInt(r.nextInt(r.nextInt(40) + 1) + 1);
    if (r.nextInt(15) != 0) n = 0;
    for (int i = 0; i < n; ++i) {
        double x = double(chunkX * 16 + r.nextInt(16));
        double y = double(r.nextInt(r.nextInt(120) + 8));
        double z = double(chunkZ * 16 + r.nextInt(16));
        int k = 1;
        if (r.nextInt(4) == 0) {
            float w = 1.0f + r.nextFloat() * 6.0f; // func_870_a: a big room
            caveTunnel(cx, cz, b, x, y, z, w, 0.0f, 0.0f, -1, -1, 0.5);
            k += r.nextInt(4);
        }
        for (int j = 0; j < k; ++j) {
            float yaw = r.nextFloat() * PI_F * 2.0f;
            float pitch = (r.nextFloat() - 0.5f) * 2.0f / 8.0f;
            float w = r.nextFloat() * 2.0f;
            w += r.nextFloat();
            caveTunnel(cx, cz, b, x, y, z, w, yaw, pitch, 0, 0, 1.0);
        }
    }
}

void ChunkGenerator::caveTunnel(int cx, int cz, uint8_t* b, double x, double y, double z, float width, float yaw,
                                float pitch, int step, int steps, double hScale) {
    double midX = double(cx * 16 + 8), midZ = double(cz * 16 + 8);
    float dYaw = 0.0f, dPitch = 0.0f;
    JavaRandom r(caveRand.nextLong());
    if (steps <= 0) {
        int range = 8 * 16 - 16;
        steps = range - r.nextInt(range / 4);
    }
    bool single = false;
    if (step == -1) {
        step = steps / 2;
        single = true;
    }
    int branch = r.nextInt(steps / 2) + steps / 4;
    for (bool steep = r.nextInt(6) == 0; step < steps; ++step) {
        double hw = 1.5 + double(MathHelper::sin(float(step) * PI_F / float(steps)) * width * 1.0f);
        double hh = hw * hScale;
        float cp = MathHelper::cos(pitch), sp = MathHelper::sin(pitch);
        x += double(MathHelper::cos(yaw) * cp);
        y += double(sp);
        z += double(MathHelper::sin(yaw) * cp);
        if (steep) pitch *= 0.92f;
        else pitch *= 0.7f;
        pitch += dPitch * 0.1f;
        yaw += dYaw * 0.1f;
        dPitch *= 0.9f;
        dYaw *= 12.0f / 16.0f;
        {
            float r1 = r.nextFloat();
            float r2 = r.nextFloat();
            float r3 = r.nextFloat();
            dPitch += (r1 - r2) * r3 * 2.0f;
        }
        {
            float r1 = r.nextFloat();
            float r2 = r.nextFloat();
            float r3 = r.nextFloat();
            dYaw += (r1 - r2) * r3 * 4.0f;
        }
        if (!single && step == branch && width > 1.0f) {
            float w1 = r.nextFloat() * 0.5f + 0.5f;
            caveTunnel(cx, cz, b, x, y, z, w1, yaw - PI_F * 0.5f, pitch / 3.0f, step, steps, 1.0);
            float w2 = r.nextFloat() * 0.5f + 0.5f;
            caveTunnel(cx, cz, b, x, y, z, w2, yaw + PI_F * 0.5f, pitch / 3.0f, step, steps, 1.0);
            return;
        }
        if (!single && r.nextInt(4) == 0) continue;
        double ddx = x - midX, ddz = z - midZ, left = double(steps - step), reach = double(width + 2.0f + 16.0f);
        if (ddx * ddx + ddz * ddz - left * left > reach * reach) return;
        if (!(x >= midX - 16.0 - hw * 2.0 && z >= midZ - 16.0 - hw * 2.0 && x <= midX + 16.0 + hw * 2.0 &&
              z <= midZ + 16.0 + hw * 2.0))
            continue;
        int x1 = MathHelper::floor_double(x - hw) - cx * 16 - 1, x2 = MathHelper::floor_double(x + hw) - cx * 16 + 1;
        int y1 = MathHelper::floor_double(y - hh) - 1, y2 = MathHelper::floor_double(y + hh) + 1;
        int z1 = MathHelper::floor_double(z - hw) - cz * 16 - 1, z2 = MathHelper::floor_double(z + hw) - cz * 16 + 1;
        if (x1 < 0) x1 = 0;
        if (x2 > 16) x2 = 16;
        if (y1 < 1) y1 = 1;
        if (y2 > 120) y2 = 120;
        if (z1 < 0) z1 = 0;
        if (z2 > 16) z2 = 16;
        bool water = false;
        for (int bx = x1; !water && bx < x2; ++bx)
            for (int bz = z1; !water && bz < z2; ++bz)
                for (int by = y2 + 1; !water && by >= y1 - 1; --by) {
                    if (by >= 0 && by < 128) {
                        uint8_t id = b[(bx * 16 + bz) * 128 + by];
                        if (id == BlockId::WaterStill || id == BlockId::WaterMoving) water = true;
                        if (by != y1 - 1 && bx != x1 && bx != x2 - 1 && bz != z1 && bz != z2 - 1) by = y1;
                    }
                }
        if (water) continue;
        for (int bx = x1; bx < x2; ++bx) {
            double dx = (double(bx + cx * 16) + 0.5 - x) / hw;
            for (int bz = z1; bz < z2; ++bz) {
                double dz = (double(bz + cz * 16) + 0.5 - z) / hw;
                int idx = (bx * 16 + bz) * 128 + y2;
                bool grass = false;
                if (dx * dx + dz * dz < 1.0) {
                    for (int by = y2 - 1; by >= y1; --by) {
                        double dy = (double(by) + 0.5 - y) / hh;
                        if (dy > -0.7 && dx * dx + dy * dy + dz * dz < 1.0) {
                            uint8_t id = b[idx];
                            if (id == BlockId::Grass) grass = true;
                            if (id == BlockId::Stone || id == BlockId::Dirt || id == BlockId::Grass) {
                                if (by < 10) {
                                    b[idx] = BlockId::LavaStill;
                                } else {
                                    b[idx] = 0;
                                    if (grass && b[idx - 1] == BlockId::Dirt) b[idx - 1] = BlockId::Grass;
                                }
                            }
                        }
                        --idx;
                    }
                }
            }
        }
        if (single) break;
    }
}

// ================================================================ MapGenRavine (Axeolon)

void ChunkGenerator::ravineChunk(int chunkX, int chunkZ, int cx, int cz, uint8_t* b) {
    JavaRandom r(ravineRand.nextLong() ^ 0x5A3C9E1LL);
    if (r.nextInt(50) == 0) {
        double x = double(chunkX * 16 + r.nextInt(16));
        double y = double(r.nextInt(r.nextInt(40) + 8) + 20);
        double z = double(chunkZ * 16 + r.nextInt(16));
        float yaw = r.nextFloat() * PI_F * 2.0f;
        float pitch = (r.nextFloat() - 0.5f) * 2.0f / 8.0f;
        float w = r.nextFloat() * 2.0f;
        w = (w + r.nextFloat()) * 2.0f;
        int64_t s = r.nextLong();
        ravine(s, cx, cz, b, x, y, z, w, yaw, pitch, 0, 0, 3.0);
    }
    if (r.nextInt(45) == 0) {
        int64_t s = r.nextLong();
        cavern(s, chunkX, chunkZ, cx, cz, b);
    }
}

void ChunkGenerator::ravine(int64_t s, int cx, int cz, uint8_t* b, double x, double y, double z, float width,
                            float yaw, float pitch, int step, int steps, double hScale) {
    JavaRandom r(s);
    double midX = double(cx * 16 + 8), midZ = double(cz * 16 + 8);
    float dYaw = 0.0f, dPitch = 0.0f;
    if (steps <= 0) {
        int range = 8 * 16 - 16;
        steps = range - r.nextInt(range / 4);
    }
    bool single = false;
    if (step == -1) {
        step = steps / 2;
        single = true;
    }
    float f = 1.0f;
    for (int i = 0; i < 128; ++i) {
        if (i == 0 || r.nextInt(3) == 0) {
            float a = r.nextFloat();
            f = 1.0f + a * r.nextFloat();
        }
        widthByY[i] = f * f;
    }
    for (; step < steps; ++step) {
        double hw = 1.5 + double(MathHelper::sin(float(step) * PI_F / float(steps)) * width);
        double hh = hw * hScale;
        hw *= double(r.nextFloat()) * 0.25 + 0.75;
        hh *= double(r.nextFloat()) * 0.25 + 0.75;
        float cp = MathHelper::cos(pitch), sp = MathHelper::sin(pitch);
        x += double(MathHelper::cos(yaw) * cp);
        y += double(sp);
        z += double(MathHelper::sin(yaw) * cp);
        pitch *= 0.7f;
        pitch += dPitch * 0.05f;
        yaw += dYaw * 0.05f;
        dPitch *= 0.8f;
        dYaw *= 0.5f;
        {
            float r1 = r.nextFloat();
            float r2 = r.nextFloat();
            float r3 = r.nextFloat();
            dPitch += (r1 - r2) * r3 * 2.0f;
        }
        {
            float r1 = r.nextFloat();
            float r2 = r.nextFloat();
            float r3 = r.nextFloat();
            dYaw += (r1 - r2) * r3 * 4.0f;
        }
        if (single || r.nextInt(4) != 0) {
            double ddx = x - midX, ddz = z - midZ, left = double(steps - step),
                   reach = double(width + 2.0f + 16.0f);
            if (ddx * ddx + ddz * ddz - left * left > reach * reach) return;
            if (x >= midX - 16.0 - hw * 2.0 && z >= midZ - 16.0 - hw * 2.0 && x <= midX + 16.0 + hw * 2.0 &&
                z <= midZ + 16.0 + hw * 2.0) {
                carve(cx, cz, b, x, y, z, hw, hh, true);
                if (single) break;
            }
        }
    }
}

void ChunkGenerator::cavern(int64_t s, int chunkX, int chunkZ, int cx, int cz, uint8_t* b) {
    JavaRandom r(s);
    double x0 = double(chunkX * 16 + r.nextInt(16));
    double z0 = double(chunkZ * 16 + r.nextInt(16));
    double y0 = double(14 + r.nextInt(r.nextInt(30) + 1));
    int domes = 4 + r.nextInt(4);
    for (int i = 0; i < domes; ++i) {
        double a = r.nextDouble() * PI * 2.0;
        double d = i == 0 ? 0.0 : 6.0 + r.nextDouble() * 12.0;
        double x = x0 + std::cos(a) * d, z = z0 + std::sin(a) * d;
        double y = y0 + (r.nextDouble() - 0.5) * 6.0;
        double hw = 7.0 + r.nextDouble() * 7.0;
        double hh = 4.0 + r.nextDouble() * 4.0;
        if (x + hw < double(cx * 16) - 1.0 || x - hw > double(cx * 16 + 16) + 1.0 || z + hw < double(cz * 16) - 1.0 ||
            z - hw > double(cz * 16 + 16) + 1.0)
            continue;
        for (int k = 0; k < 128; ++k) widthByY[k] = 1.0f;
        carve(cx, cz, b, x, y, z, hw, hh, false);
    }
}

void ChunkGenerator::carve(int cx, int cz, uint8_t* b, double x, double y, double z, double hw, double hh,
                           bool isRavine) {
    int x1 = MathHelper::floor_double(x - hw) - cx * 16 - 1, x2 = MathHelper::floor_double(x + hw) - cx * 16 + 1;
    int y1 = MathHelper::floor_double(y - hh) - 1, y2 = MathHelper::floor_double(y + hh) + 1;
    int z1 = MathHelper::floor_double(z - hw) - cz * 16 - 1, z2 = MathHelper::floor_double(z + hw) - cz * 16 + 1;
    if (x1 < 0) x1 = 0;
    if (x2 > 16) x2 = 16;
    if (y1 < 1) y1 = 1;
    if (y2 > 120) y2 = 120;
    if (z1 < 0) z1 = 0;
    if (z2 > 16) z2 = 16;
    if (x1 >= x2 || z1 >= z2 || y1 >= y2) return;
    for (int bx = x1; bx < x2; ++bx)
        for (int bz = z1; bz < z2; ++bz)
            for (int by = y2 + 1; by >= y1 - 1; --by) {
                if (by < 0 || by >= 128) continue;
                uint8_t id = b[(bx * 16 + bz) * 128 + by];
                if (id == BlockId::WaterStill || id == BlockId::WaterMoving) return;
                if (by != y1 - 1 && bx != x1 && bx != x2 - 1 && bz != z1 && bz != z2 - 1) by = y1;
            }
    for (int bx = x1; bx < x2; ++bx) {
        double dx = (double(bx + cx * 16) + 0.5 - x) / hw;
        for (int bz = z1; bz < z2; ++bz) {
            double dz = (double(bz + cz * 16) + 0.5 - z) / hw;
            int idx = (bx * 16 + bz) * 128 + y2;
            bool grass = false;
            if (dx * dx + dz * dz < 1.0) {
                for (int by = y2 - 1; by >= y1; --by) {
                    double dy = (double(by) + 0.5 - y) / hh;
                    bool inside = isRavine ? (dx * dx + dz * dz) * double(widthByY[by]) + dy * dy / 6.0 < 1.0
                                           : dy > -0.55 && dx * dx + dy * dy + dz * dz < 1.0;
                    if (inside) {
                        uint8_t id = b[idx];
                        if (id == BlockId::Grass) grass = true;
                        if (id == BlockId::Stone || id == BlockId::Dirt || id == BlockId::Grass ||
                            id == BlockId::Gravel) {
                            if (by < 10) {
                                b[idx] = BlockId::LavaStill;
                            } else {
                                b[idx] = 0;
                                if (grass && b[idx - 1] == BlockId::Dirt) b[idx - 1] = BlockId::Grass;
                            }
                        }
                    }
                    --idx;
                }
            }
        }
    }
}

// ================================================================ AxeolonStone.decorate

void ChunkGenerator::stoneTypes(uint8_t* b, int cx, int cz) {
    JavaRandom r(seed ^ add(add(mul(cx, 735632791LL), mul(cz, 428376251LL)), 0x5703ELL));
    const int FROST = 0, OCHRE = 1, DEEP = 2, VERDANT = 3;
    for (int a = 0; a < 16; ++a)
        for (int c = 0; c < 16; ++c) {
            int base = (a * 16 + c) * 128;
            Biome bi = biomes[a + c * 16];
            int layer;
            switch (bi) {
            case Biome::Taiga: case Biome::Tundra: case Biome::IceDesert: layer = FROST; break;
            case Biome::Desert: case Biome::Plains: case Biome::Savanna: case Biome::Shrubland: layer = OCHRE; break;
            case Biome::Forest: case Biome::Rainforest: case Biome::SeasonalForest: case Biome::Swampland:
            case Biome::BirchForest: case Biome::DenseForest: case Biome::Thrams: case Biome::Drogwoods:
                layer = VERDANT;
                break;
            default: layer = -1;
            }
            int top = -1;
            for (int y = 127; y >= 0; --y)
                if (b[base + y] == BlockId::Stone) { top = y; break; }
            int layerBottom = top - 26 - r.nextInt(6);
            int deepTop = 12 + r.nextInt(6);
            uint8_t deep = BlockId::StoneTypes + DEEP;
            uint8_t lay = layer < 0 ? 0 : uint8_t(BlockId::StoneTypes + layer);
            for (int y = 0; y <= top; ++y) {
                if (b[base + y] != BlockId::Stone) continue;
                if (y <= deepTop || (y <= deepTop + 4 && r.nextInt(4) == 0)) b[base + y] = deep;
                else if (layer >= 0 && y >= layerBottom) b[base + y] = lay;
            }
        }
}
