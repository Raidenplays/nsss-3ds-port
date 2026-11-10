#include "Nether.h"
#include <cmath>
#include <cstring>
#include "world/MathHelper.h"

namespace {
const float PI_F = 3.1415927f;
inline int64_t mul(int64_t a, int64_t b) { return int64_t(uint64_t(a) * uint64_t(b)); }
inline int64_t add(int64_t a, int64_t b) { return int64_t(uint64_t(a) + uint64_t(b)); }
enum : uint8_t { BEDROCK = 7, LAVA = 10, LAVA_STILL = 11, GRAVEL = 13, DIRT = 3, GRASS = 2, BLOODSTONE = 87, SLOWSAND = 88 };
} // namespace

NetherTerrain::NetherTerrain(int64_t s)
    : hellRNG(s), seed(s), n4169i(hellRNG, 16), n4168j(hellRNG, 16), n4167k(hellRNG, 8), n4166l(hellRNG, 4),
      n4165m(hellRNG, 4), n4177a(hellRNG, 10), n4176b(hellRNG, 16) {}

void NetherTerrain::provideChunk(int cx, int cz, uint8_t* out) {
    hellRNG.setSeed(add(mul(cx, 341873128712LL), mul(cz, 132897987541LL)));
    uint8_t* b = scratch;
    std::memset(b, 0, sizeof scratch);
    terrain(cx, cz, b);
    surface(cx, cz, b);
    caves(cx, cz, b);
    // Chunk.expandBlocks: 128 tall into the 256-tall layout
    std::memset(out, 0, 65536);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) std::memcpy(out + (x << 12 | z << 8), b + (x << 11 | z << 7), 128);
}

void NetherTerrain::syncAfterProvide(int cx, int cz) {
    // func_4058_b's draws from hellRNG don't depend on the blocks: replay just the draws
    hellRNG.setSeed(add(mul(cx, 341873128712LL), mul(cz, 132897987541LL)));
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            hellRNG.nextDouble();
            hellRNG.nextDouble();
            hellRNG.nextDouble();
            for (int y = 127; y >= 0; --y)
                if (!(y >= 127 - hellRNG.nextInt(5))) hellRNG.nextInt(5);
        }
}

void NetherTerrain::heightNoise(int x, int y, int z, int sx, int sy, int sz) { // func_4057_a
    const double A = 684.412, B = 2053.236;
    density.assign(size_t(sx * sy * sz), 0.0);
    n4177a.generate(n4172f, double(x), double(y), double(z), sx, 1, sz, 1.0, 0.0, 1.0);
    n4176b.generate(n4171g, double(x), double(y), double(z), sx, 1, sz, 100.0, 0.0, 100.0);
    n4167k.generate(n4175c, double(x), double(y), double(z), sx, sy, sz, A / 80.0, B / 60.0, A / 80.0);
    n4169i.generate(n4174d, double(x), double(y), double(z), sx, sy, sz, A, B, A);
    n4168j.generate(n4173e, double(x), double(y), double(z), sx, sy, sz, A, B, A);
    int i12 = 0, i13 = 0;
    std::vector<double> v14(size_t(sy), 0.0);
    for (int i = 0; i < sy; ++i) {
        v14[size_t(i)] = std::cos(double(i) * 3.141592653589793 * 6.0 / double(sy)) * 2.0;
        double d = double(i);
        if (i > sy / 2) d = double(sy - 1 - i);
        if (d < 4.0) {
            d = 4.0 - d;
            v14[size_t(i)] -= d * d * d * 10.0;
        }
    }
    for (int a = 0; a < sx; ++a)
        for (int c = 0; c < sz; ++c) {
            double v17 = (n4172f[size_t(i13)] + 256.0) / 512.0;
            if (v17 > 1.0) v17 = 1.0;
            double v19 = 0.0;
            double v21 = n4171g[size_t(i13)] / 8000.0;
            if (v21 < 0.0) v21 = -v21;
            v21 = v21 * 3.0 - 3.0;
            if (v21 < 0.0) {
                v21 /= 2.0;
                if (v21 < -1.0) v21 = -1.0;
                v21 /= 1.4;
                v21 /= 2.0;
                v17 = 0.0;
            } else {
                if (v21 > 1.0) v21 = 1.0;
                v21 /= 6.0;
            }
            v17 += 0.5;
            v21 = v21 * double(sy) / 16.0;
            ++i13;
            (void)v17;
            (void)v21;
            for (int k = 0; k < sy; ++k) {
                double v24;
                double v26 = v14[size_t(k)];
                double v28 = n4174d[size_t(i12)] / 512.0;
                double v30 = n4173e[size_t(i12)] / 512.0;
                double v32 = (n4175c[size_t(i12)] / 10.0 + 1.0) / 2.0;
                if (v32 < 0.0) v24 = v28;
                else if (v32 > 1.0) v24 = v30;
                else v24 = v28 + (v30 - v28) * v32;
                v24 -= v26;
                if (k > sy - 4) {
                    double v34 = double(float(k - (sy - 4)) / 3.0f);
                    v24 = v24 * (1.0 - v34) + -10.0 * v34;
                }
                if (double(k) < v19) {
                    double v34 = (v19 - double(k)) / 4.0;
                    if (v34 < 0.0) v34 = 0.0;
                    if (v34 > 1.0) v34 = 1.0;
                    v24 = v24 * (1.0 - v34) + -10.0 * v34;
                }
                density[size_t(i12)] = v24;
                ++i12;
            }
        }
}

void NetherTerrain::terrain(int cx, int cz, uint8_t* b) { // func_4059_a
    const int N = 4, SEA = 32, SX = N + 1, SY = 17, SZ = N + 1;
    heightNoise(cx * N, 0, cz * N, SX, SY, SZ);
    const std::vector<double>& d = density;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            for (int k = 0; k < 16; ++k) {
                const double s = 0.125;
                double a = d[size_t(((i + 0) * SZ + j + 0) * SY + k + 0)];
                double bb = d[size_t(((i + 0) * SZ + j + 1) * SY + k + 0)];
                double c = d[size_t(((i + 1) * SZ + j + 0) * SY + k + 0)];
                double e = d[size_t(((i + 1) * SZ + j + 1) * SY + k + 0)];
                double da = (d[size_t(((i + 0) * SZ + j + 0) * SY + k + 1)] - a) * s;
                double db = (d[size_t(((i + 0) * SZ + j + 1) * SY + k + 1)] - bb) * s;
                double dc = (d[size_t(((i + 1) * SZ + j + 0) * SY + k + 1)] - c) * s;
                double de = (d[size_t(((i + 1) * SZ + j + 1) * SY + k + 1)] - e) * s;
                for (int l = 0; l < 8; ++l) {
                    const double t = 0.25;
                    double p = a, q = bb;
                    double dp = (c - a) * t, dq = (e - bb) * t;
                    for (int m = 0; m < 4; ++m) {
                        int idx = (m + i * 4) << 11 | (j * 4) << 7 | (k * 8 + l);
                        double v = p, dv = (q - p) * 0.25;
                        for (int n = 0; n < 4; ++n) {
                            int id = 0;
                            if (k * 8 + l < SEA) id = LAVA_STILL; // "lavaMoving" in MCP names is id 11
                            if (v > 0.0) id = BLOODSTONE;
                            b[idx] = uint8_t(id);
                            idx += 128;
                            v += dv;
                        }
                        p += dp;
                        q += dq;
                    }
                    a += da;
                    bb += db;
                    c += dc;
                    e += de;
                }
            }
}

void NetherTerrain::surface(int cx, int cz, uint8_t* b) { // func_4058_b
    const int SEA = 64;
    const double s = 1.0 / 32.0;
    n4166l.generate(n4162p, double(cx * 16), double(cz * 16), 0.0, 16, 16, 1, s, s, 1.0);
    n4166l.generate(n4161q, double(cz * 16), 109.0134, double(cx * 16), 16, 1, 16, s, 1.0, s);
    n4165m.generate(n4160r, double(cx * 16), double(cz * 16), 0.0, 16, 16, 1, s * 2.0, s * 2.0, s * 2.0);
    for (int x = 0; x < 16; ++x)
        for (int z = 0; z < 16; ++z) {
            bool soul = n4162p[size_t(x + z * 16)] + hellRNG.nextDouble() * 0.2 > 0.0;
            bool gravel = n4161q[size_t(x + z * 16)] + hellRNG.nextDouble() * 0.2 > 0.0;
            int depth = int(n4160r[size_t(x + z * 16)] / 3.0 + 3.0 + hellRNG.nextDouble() * 0.25);
            int run = -1;
            uint8_t top = BLOODSTONE, fill = BLOODSTONE;
            for (int y = 127; y >= 0; --y) {
                int i = (x * 16 + z) * 128 + y;
                if (y >= 127 - hellRNG.nextInt(5)) {
                    b[i] = BEDROCK;
                } else if (y <= 0 + hellRNG.nextInt(5)) {
                    b[i] = BEDROCK;
                } else {
                    uint8_t cur = b[i];
                    if (cur == 0) {
                        run = -1;
                    } else if (cur == BLOODSTONE) {
                        if (run == -1) {
                            if (depth <= 0) {
                                top = 0;
                                fill = BLOODSTONE;
                            } else if (y >= SEA - 4 && y <= SEA + 1) {
                                top = BLOODSTONE;
                                fill = BLOODSTONE;
                                if (gravel) top = GRAVEL;
                                if (gravel) fill = BLOODSTONE;
                                if (soul) top = SLOWSAND;
                                if (soul) fill = SLOWSAND;
                            }
                            if (y < SEA && top == 0) top = LAVA_STILL;
                            run = depth;
                            b[i] = y >= SEA - 1 ? top : fill;
                        } else if (run > 0) {
                            --run;
                            b[i] = fill;
                        }
                    }
                }
            }
        }
}

// ---------------------------------------------------------------- MapGenCavesHell

void NetherTerrain::caves(int cx, int cz, uint8_t* b) { // MapGenBase.func_867_a
    const int R = 8;
    caveRand.setSeed(seed);
    int64_t a = caveRand.nextLong() / 2 * 2 + 1;
    int64_t c = caveRand.nextLong() / 2 * 2 + 1;
    for (int x = cx - R; x <= cx + R; ++x)
        for (int z = cz - R; z <= cz + R; ++z) {
            caveRand.setSeed(add(mul(x, a), mul(z, c)) ^ seed);
            caveChunk(x, z, cx, cz, b);
        }
}

void NetherTerrain::caveChunk(int chunkX, int chunkZ, int cx, int cz, uint8_t* b) { // func_868_a
    JavaRandom& r = caveRand;
    int n1 = r.nextInt(10) + 1;
    int n2 = r.nextInt(n1) + 1;
    int n = r.nextInt(n2);
    if (r.nextInt(5) != 0) n = 0;
    for (int i = 0; i < n; ++i) {
        double x = double(chunkX * 16 + r.nextInt(16));
        double y = double(r.nextInt(128));
        double z = double(chunkZ * 16 + r.nextInt(16));
        int k = 1;
        if (r.nextInt(4) == 0) { // func_4129_a: a big room
            float w = 1.0f + r.nextFloat() * 6.0f;
            tunnel(cx, cz, b, x, y, z, w, 0.0f, 0.0f, -1, -1, 0.5);
            k += r.nextInt(4);
        }
        for (int j = 0; j < k; ++j) {
            float yaw = r.nextFloat() * PI_F * 2.0f;
            float pitch = (r.nextFloat() - 0.5f) * 2.0f / 8.0f;
            float w1 = r.nextFloat() * 2.0f;
            float w = w1 + r.nextFloat();
            tunnel(cx, cz, b, x, y, z, w * 2.0f, yaw, pitch, 0, 0, 0.5);
        }
    }
}

void NetherTerrain::tunnel(int cx, int cz, uint8_t* b, double x, double y, double z, float width, float yaw, float pitch,
                           int step, int steps, double hScale) { // func_4128_a
    double ox = double(cx * 16 + 8), oz = double(cz * 16 + 8);
    float dYaw = 0.0f, dPitch = 0.0f;
    JavaRandom r(caveRand.nextLong());
    if (steps <= 0) {
        int m = 8 * 16 - 16;
        steps = m - r.nextInt(m / 4);
    }
    bool room = false;
    if (step == -1) {
        step = steps / 2;
        room = true;
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
            float a = r.nextFloat(), c = r.nextFloat();
            dPitch += (a - c) * r.nextFloat() * 2.0f;
        }
        {
            float a = r.nextFloat(), c = r.nextFloat();
            dYaw += (a - c) * r.nextFloat() * 4.0f;
        }
        if (!room && step == branch && width > 1.0f) {
            float w1 = r.nextFloat() * 0.5f + 0.5f;
            tunnel(cx, cz, b, x, y, z, w1, yaw - PI_F * 0.5f, pitch / 3.0f, step, steps, 1.0);
            float w2 = r.nextFloat() * 0.5f + 0.5f;
            tunnel(cx, cz, b, x, y, z, w2, yaw + PI_F * 0.5f, pitch / 3.0f, step, steps, 1.0);
            return;
        }
        if (room || r.nextInt(4) != 0) {
            double dx = x - ox, dz = z - oz, left = double(steps - step), reach = double(width + 2.0f + 16.0f);
            if (dx * dx + dz * dz - left * left > reach * reach) return;
            if (x >= ox - 16.0 - hw * 2.0 && z >= oz - 16.0 - hw * 2.0 && x <= ox + 16.0 + hw * 2.0 && z <= oz + 16.0 + hw * 2.0) {
                int x0 = MathHelper::floor_double(x - hw) - cx * 16 - 1;
                int x1 = MathHelper::floor_double(x + hw) - cx * 16 + 1;
                int y0 = MathHelper::floor_double(y - hh) - 1;
                int y1 = MathHelper::floor_double(y + hh) + 1;
                int z0 = MathHelper::floor_double(z - hw) - cz * 16 - 1;
                int z1 = MathHelper::floor_double(z + hw) - cz * 16 + 1;
                if (x0 < 0) x0 = 0;
                if (x1 > 16) x1 = 16;
                if (y0 < 1) y0 = 1;
                if (y1 > 120) y1 = 120;
                if (z0 < 0) z0 = 0;
                if (z1 > 16) z1 = 16;
                bool lava = false;
                for (int a = x0; !lava && a < x1; ++a)
                    for (int c = z0; !lava && c < z1; ++c)
                        for (int d = y1 + 1; !lava && d >= y0 - 1; --d) {
                            int i = (a * 16 + c) * 128 + d;
                            if (d >= 0 && d < 128) {
                                if (b[i] == LAVA_STILL || b[i] == LAVA) lava = true;
                                if (d != y0 - 1 && a != x0 && a != x1 - 1 && c != z0 && c != z1 - 1) d = y0;
                            }
                        }
                if (!lava) {
                    for (int a = x0; a < x1; ++a) {
                        double fx = (double(a + cx * 16) + 0.5 - x) / hw;
                        for (int c = z0; c < z1; ++c) {
                            double fz = (double(c + cz * 16) + 0.5 - z) / hw;
                            int i = (a * 16 + c) * 128 + y1;
                            for (int d = y1 - 1; d >= y0; --d) {
                                double fy = (double(d) + 0.5 - y) / hh;
                                if (fy > -0.7 && fx * fx + fy * fy + fz * fz < 1.0) {
                                    uint8_t id = b[i];
                                    if (id == BLOODSTONE || id == DIRT || id == GRASS) b[i] = 0;
                                }
                                --i;
                            }
                        }
                    }
                    if (room) break;
                }
            }
        }
    }
}
