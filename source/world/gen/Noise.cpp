#include "Noise.h"
#include <cmath>

namespace {
inline double lerp(double t, double a, double b) { return a + t * (b - a); }
inline double fade(double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
inline double grad(int h, double x, double y, double z) {
    int k = h & 15;
    double u = k < 8 ? x : y;
    double v = k < 4 ? y : (k != 12 && k != 14 ? z : x);
    return ((k & 1) == 0 ? u : -u) + ((k & 2) == 0 ? v : -v);
}
inline double grad2(int h, double x, double z) { // func_4110_a
    int k = h & 15;
    double u = double(1 - ((k & 8) >> 3)) * x;
    double v = k < 4 ? 0.0 : (k != 12 && k != 14 ? z : x);
    return ((k & 1) == 0 ? u : -u) + ((k & 2) == 0 ? v : -v);
}
inline int floorInt(double v) {
    int i = int(v);
    return v < double(i) ? i - 1 : i;
}
} // namespace

NoisePerlin::NoisePerlin(JavaRandom& r) {
    xCoord = r.nextDouble() * 256.0;
    yCoord = r.nextDouble() * 256.0;
    zCoord = r.nextDouble() * 256.0;
    for (int i = 0; i < 256; i++) p[i] = i;
    for (int i = 0; i < 256; ++i) {
        int j = r.nextInt(256 - i) + i;
        int t = p[i];
        p[i] = p[j];
        p[j] = t;
        p[i + 256] = p[i];
    }
}

double NoisePerlin::generateNoise(double x, double y, double z) const {
    x += xCoord;
    y += yCoord;
    z += zCoord;
    int X = floorInt(x), Y = floorInt(y), Z = floorInt(z);
    int xi = X & 255, yi = Y & 255, zi = Z & 255;
    x -= X;
    y -= Y;
    z -= Z;
    double u = fade(x), v = fade(y), w = fade(z);
    int a = p[xi] + yi, aa = p[a] + zi, ab = p[a + 1] + zi;
    int b = p[xi + 1] + yi, ba = p[b] + zi, bb = p[b + 1] + zi;
    return lerp(w, lerp(v, lerp(u, grad(p[aa], x, y, z), grad(p[ba], x - 1.0, y, z)),
                        lerp(u, grad(p[ab], x, y - 1.0, z), grad(p[bb], x - 1.0, y - 1.0, z))),
                lerp(v, lerp(u, grad(p[aa + 1], x, y, z - 1.0), grad(p[ba + 1], x - 1.0, y, z - 1.0)),
                     lerp(u, grad(p[ab + 1], x, y - 1.0, z - 1.0), grad(p[bb + 1], x - 1.0, y - 1.0, z - 1.0))));
}

void NoisePerlin::add(double* out, double x0, double y0, double z0, int sx, int sy, int sz, double fx, double fy,
                      double fz, double amp) const {
    double inv = 1.0 / amp;
    if (sy == 1) {
        int n = 0;
        for (int i = 0; i < sx; ++i) {
            double x = (x0 + double(i)) * fx + xCoord;
            int X = floorInt(x);
            int xi = X & 255;
            x -= double(X);
            double u = fade(x);
            for (int k = 0; k < sz; ++k) {
                double z = (z0 + double(k)) * fz + zCoord;
                int Z = floorInt(z);
                int zi = Z & 255;
                z -= double(Z);
                double w = fade(z);
                int a = p[xi] + 0;
                int aa = p[a] + zi;
                int b = p[xi + 1] + 0;
                int ba = p[b] + zi;
                double l1 = lerp(u, grad2(p[aa], x, z), grad(p[ba], x - 1.0, 0.0, z));
                double l2 = lerp(u, grad(p[aa + 1], x, 0.0, z - 1.0), grad(p[ba + 1], x - 1.0, 0.0, z - 1.0));
                out[n++] += lerp(w, l1, l2) * inv;
            }
        }
        return;
    }
    int n = 0;
    int lastY = -1;
    double l1 = 0, l2 = 0, l3 = 0, l4 = 0;
    for (int i = 0; i < sx; ++i) {
        double x = (x0 + double(i)) * fx + xCoord;
        int X = floorInt(x);
        int xi = X & 255;
        x -= double(X);
        double u = fade(x);
        for (int k = 0; k < sz; ++k) {
            double z = (z0 + double(k)) * fz + zCoord;
            int Z = floorInt(z);
            int zi = Z & 255;
            z -= double(Z);
            double w = fade(z);
            for (int j = 0; j < sy; ++j) {
                double y = (y0 + double(j)) * fy + yCoord;
                int Y = floorInt(y);
                int yi = Y & 255;
                y -= double(Y);
                double v = fade(y);
                if (j == 0 || yi != lastY) {
                    lastY = yi;
                    int a = p[xi] + yi, aa = p[a] + zi, ab = p[a + 1] + zi;
                    int b = p[xi + 1] + yi, ba = p[b] + zi, bb = p[b + 1] + zi;
                    l1 = lerp(u, grad(p[aa], x, y, z), grad(p[ba], x - 1.0, y, z));
                    l2 = lerp(u, grad(p[ab], x, y - 1.0, z), grad(p[bb], x - 1.0, y - 1.0, z));
                    l3 = lerp(u, grad(p[aa + 1], x, y, z - 1.0), grad(p[ba + 1], x - 1.0, y, z - 1.0));
                    l4 = lerp(u, grad(p[ab + 1], x, y - 1.0, z - 1.0), grad(p[bb + 1], x - 1.0, y - 1.0, z - 1.0));
                }
                double m1 = lerp(v, l1, l2);
                double m2 = lerp(v, l3, l4);
                out[n++] += lerp(w, m1, m2) * inv;
            }
        }
    }
}

NoiseOctaves::NoiseOctaves(JavaRandom& r, int n) {
    octaves.reserve(n);
    for (int i = 0; i < n; ++i) octaves.emplace_back(r);
}

void NoiseOctaves::generate(std::vector<double>& out, double x, double y, double z, int sx, int sy, int sz,
                            double fx, double fy, double fz) const {
    out.assign(size_t(sx) * sy * sz, 0.0);
    double amp = 1.0;
    for (const NoisePerlin& o : octaves) {
        o.add(out.data(), x, y, z, sx, sy, sz, fx * amp, fy * amp, fz * amp, amp);
        amp /= 2.0;
    }
}

double NoiseOctaves::value2D(double x, double z) const {
    double v = 0.0, f = 1.0;
    for (const NoisePerlin& o : octaves) {
        v += o.noise2(x * f, z * f) / f;
        f /= 2.0;
    }
    return v;
}

// ---------------------------------------------------------------- simplex (NoiseGenerator2)

namespace {
const int GRAD3[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
                          {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};
const double F2 = 0.5 * (std::sqrt(3.0) - 1.0);
const double G2 = (3.0 - std::sqrt(3.0)) / 6.0;
inline int wrap(double v) { return v > 0.0 ? int(v) : int(v) - 1; }
inline double dot(const int* g, double x, double y) { return double(g[0]) * x + double(g[1]) * y; }
} // namespace

NoiseSimplex::NoiseSimplex(JavaRandom& r) {
    xo = r.nextDouble() * 256.0;
    yo = r.nextDouble() * 256.0;
    zo = r.nextDouble() * 256.0;
    for (int i = 0; i < 256; i++) p[i] = i;
    for (int i = 0; i < 256; ++i) {
        int j = r.nextInt(256 - i) + i;
        int t = p[i];
        p[i] = p[j];
        p[j] = t;
        p[i + 256] = p[i];
    }
}

void NoiseSimplex::add(double* out, double x0, double z0, int sx, int sz, double fx, double fz, double amp) const {
    int n = 0;
    for (int i = 0; i < sx; ++i) {
        double x = (x0 + double(i)) * fx + xo;
        for (int k = 0; k < sz; ++k) {
            double y = (z0 + double(k)) * fz + yo;
            double s = (x + y) * F2;
            int ii = wrap(x + s), jj = wrap(y + s);
            double t = double(ii + jj) * G2;
            double X0 = double(ii) - t, Y0 = double(jj) - t;
            double xa = x - X0, ya = y - Y0;
            int i1, j1;
            if (xa > ya) { i1 = 1; j1 = 0; } else { i1 = 0; j1 = 1; }
            double xb = xa - double(i1) + G2, yb = ya - double(j1) + G2;
            double xc = xa - 1.0 + 2.0 * G2, yc = ya - 1.0 + 2.0 * G2;
            int ia = ii & 255, ja = jj & 255;
            int g0 = p[ia + p[ja]] % 12;
            int g1 = p[ia + i1 + p[ja + j1]] % 12;
            int g2 = p[ia + 1 + p[ja + 1]] % 12;
            double n0, n1, n2;
            double t0 = 0.5 - xa * xa - ya * ya;
            if (t0 < 0.0) n0 = 0.0; else { t0 *= t0; n0 = t0 * t0 * dot(GRAD3[g0], xa, ya); }
            double t1 = 0.5 - xb * xb - yb * yb;
            if (t1 < 0.0) n1 = 0.0; else { t1 *= t1; n1 = t1 * t1 * dot(GRAD3[g1], xb, yb); }
            double t2 = 0.5 - xc * xc - yc * yc;
            if (t2 < 0.0) n2 = 0.0; else { t2 *= t2; n2 = t2 * t2 * dot(GRAD3[g2], xc, yc); }
            out[n++] += 70.0 * (n0 + n1 + n2) * amp;
        }
    }
}

NoiseOctaves2::NoiseOctaves2(JavaRandom& r, int n) {
    octaves.reserve(n);
    for (int i = 0; i < n; ++i) octaves.emplace_back(r);
}

void NoiseOctaves2::generate(std::vector<double>& out, double x, double z, int sx, int sz, double fx, double fz,
                             double lac, double pers) const {
    fx /= 1.5;
    fz /= 1.5;
    if (out.size() >= size_t(sx) * sz) std::fill(out.begin(), out.end(), 0.0);
    else out.assign(size_t(sx) * sz, 0.0);
    double a = 1.0, f = 1.0;
    for (const NoiseSimplex& o : octaves) {
        o.add(out.data(), x, z, sx, sz, fx * f, fz * f, 0.55 / a);
        f *= lac;
        a *= pers;
    }
}
