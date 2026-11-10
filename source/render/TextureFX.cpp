#include "TextureFX.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "world/JavaRandom.h"
#include "world/MathHelper.h"

namespace texfx {
namespace {
JavaRandom mathRandom{0x7E57F00DLL}; // Math.random()
double rnd() { return mathRandom.nextDouble(); }

struct Fluid { // TextureWaterFX / TexureWaterFlowFX
    float g[256] = {}, h[256] = {}, i[256] = {}, j[256] = {};
    int ticks = 0;
    void tick(bool flow, uint8_t* out) {
        ++ticks;
        for (int x = 0; x < 16; ++x)
            for (int y = 0; y < 16; ++y) {
                float s = 0.0f;
                if (flow)
                    for (int k = y - 2; k <= y; ++k) s += g[(x & 15) + (k & 15) * 16];
                else
                    for (int k = x - 1; k <= x + 1; ++k) s += g[(k & 15) + (y & 15) * 16];
                h[x + y * 16] = s / (flow ? 3.2f : 3.3f) + i[x + y * 16] * 0.8f;
            }
        for (int x = 0; x < 16; ++x)
            for (int y = 0; y < 16; ++y) {
                int n = x + y * 16;
                i[n] += j[n] * 0.05f;
                if (i[n] < 0.0f) i[n] = 0.0f;
                j[n] -= flow ? 0.3f : 0.1f;
                if (rnd() < (flow ? 0.2 : 0.05)) j[n] = 0.5f;
            }
        std::swap(g, h);
        for (int n = 0; n < 256; ++n) {
            float v = flow ? g[(n - ticks * 16) & 255] : g[n];
            if (v > 1.0f) v = 1.0f;
            if (v < 0.0f) v = 0.0f;
            float q = v * v;
            out[n * 4 + 0] = uint8_t(int(32.0f + q * 32.0f));
            out[n * 4 + 1] = uint8_t(int(50.0f + q * 64.0f));
            out[n * 4 + 2] = 255;
            out[n * 4 + 3] = uint8_t(int(146.0f + q * 50.0f));
        }
    }
};

struct Lava { // TextureLavaFX / TextureLavaFlowFX
    float g[256] = {}, h[256] = {}, i[256] = {}, j[256] = {};
    int ticks = 0;
    void tick(bool flow, uint8_t* out) {
        if (flow) ++ticks;
        for (int x = 0; x < 16; ++x)
            for (int y = 0; y < 16; ++y) {
                float s = 0.0f;
                int ox = int(MathHelper::sin(float(y) * 3.1415927f * 2.0f / 16.0f) * 1.2f);
                int oy = int(MathHelper::sin(float(x) * 3.1415927f * 2.0f / 16.0f) * 1.2f);
                for (int a = x - 1; a <= x + 1; ++a)
                    for (int b = y - 1; b <= y + 1; ++b) s += g[((a + ox) & 15) + ((b + oy) & 15) * 16];
                int n = x + y * 16;
                h[n] = s / 10.0f + (i[(x & 15) + (y & 15) * 16] + i[((x + 1) & 15) + (y & 15) * 16] +
                                    i[((x + 1) & 15) + ((y + 1) & 15) * 16] + i[(x & 15) + ((y + 1) & 15) * 16]) / 4.0f * 0.8f;
                i[n] += j[n] * 0.01f;
                if (i[n] < 0.0f) i[n] = 0.0f;
                j[n] -= 0.06f;
                if (rnd() < 0.005) j[n] = 1.5f;
            }
        std::swap(g, h);
        for (int n = 0; n < 256; ++n) {
            float v = (flow ? g[(n - ticks / 3 * 16) & 255] : g[n]) * 2.0f;
            if (v > 1.0f) v = 1.0f;
            if (v < 0.0f) v = 0.0f;
            out[n * 4 + 0] = uint8_t(int(v * 100.0f + 155.0f));
            out[n * 4 + 1] = uint8_t(int(v * v * 255.0f));
            out[n * 4 + 2] = uint8_t(int(v * v * v * v * 128.0f));
            out[n * 4 + 3] = 255;
        }
    }
};

struct Flames { // TextureFlamesFX
    float g[320] = {}, h[320] = {};
    void tick(uint8_t* out) {
        for (int x = 0; x < 16; ++x)
            for (int y = 0; y < 20; ++y) {
                int n = 18;
                float s = g[x + (y + 1) % 20 * 16] * float(n);
                for (int a = x - 1; a <= x + 1; ++a)
                    for (int b = y; b <= y + 1; ++b) {
                        if (a >= 0 && b >= 0 && a < 16 && b < 20) s += g[a + b * 16];
                        ++n;
                    }
                h[x + y * 16] = s / (float(n) * 1.06f);
                if (y >= 19) h[x + y * 16] = float(rnd() * rnd() * rnd() * 4.0 + rnd() * double(0.1f) + double(0.2f));
            }
        std::swap(g, h);
        for (int n = 0; n < 256; ++n) {
            float v = g[n] * 1.8f;
            if (v > 1.0f) v = 1.0f;
            if (v < 0.0f) v = 0.0f;
            float v2 = v * v, v4 = v2 * v2, v8 = v4 * v4;
            out[n * 4 + 0] = uint8_t(int(v * 155.0f + 100.0f));
            out[n * 4 + 1] = uint8_t(int(v2 * 255.0f));
            out[n * 4 + 2] = uint8_t(int(v8 * v2 * 255.0f));
            out[n * 4 + 3] = v < 0.5f ? 0 : 255;
        }
    }
};

struct Portal { // TexturePortalFX: 32 frames made up front
    uint8_t frames[32][1024];
    int tick = 0;
    Portal() {
        JavaRandom r(100);
        for (int f = 0; f < 32; ++f)
            for (int x = 0; x < 16; ++x)
                for (int y = 0; y < 16; ++y) {
                    float v = 0.0f;
                    for (int k = 0; k < 2; ++k) {
                        float o = float(k * 8);
                        float a = (float(x) - o) / 16.0f * 2.0f, b = (float(y) - o) / 16.0f * 2.0f;
                        if (a < -1.0f) a += 2.0f;
                        if (a >= 1.0f) a -= 2.0f;
                        if (b < -1.0f) b += 2.0f;
                        if (b >= 1.0f) b -= 2.0f;
                        float d = a * a + b * b;
                        float t = float(std::atan2(double(b), double(a))) +
                                  (float(f) / 32.0f * 3.1415927f * 2.0f - d * 10.0f + float(k * 2)) * float(k * 2 - 1);
                        t = (MathHelper::sin(t) + 1.0f) / 2.0f;
                        t /= d + 1.0f;
                        v += t * 0.5f;
                    }
                    v += r.nextFloat() * 0.1f;
                    int n = y * 16 + x;
                    frames[f][n * 4 + 0] = uint8_t(int(v * v * 200.0f + 55.0f));
                    frames[f][n * 4 + 1] = uint8_t(int(v * v * v * v * 255.0f));
                    frames[f][n * 4 + 2] = uint8_t(int(v * 100.0f + 155.0f));
                    frames[f][n * 4 + 3] = uint8_t(int(v * 100.0f + 155.0f));
                }
    }
};

Fluid water, waterFlow;
Lava lava, lavaFlow;
Flames flames0, flames1;
Portal* portal = nullptr;
uint8_t buf[1024];
} // namespace

void tick(const std::function<void(int, const uint8_t*)>& upload) {
    // the same order as Minecraft.startGame registers them
    lava.tick(false, buf);
    upload(237, buf);
    water.tick(false, buf);
    upload(205, buf);
    if (!portal) portal = new Portal();
    ++portal->tick;
    upload(14, portal->frames[portal->tick & 31]);
    waterFlow.tick(true, buf);
    for (int t : {206, 207, 222, 223}) upload(t, buf);
    lavaFlow.tick(true, buf);
    for (int t : {238, 239, 254, 255}) upload(t, buf);
    flames0.tick(buf);
    upload(31, buf);
    flames1.tick(buf);
    upload(47, buf);
}

namespace {
// a 16x16 piece of a romfs .rgba ("AXTX", width, height, RGBA rows)
bool readPiece(const char* path, int tile, uint8_t* out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    uint16_t wh[2];
    bool ok = std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "AXTX", 4) == 0 && std::fread(wh, 2, 2, f) == 2;
    if (ok) {
        int x0 = (tile & 15) * 16, y0 = (tile >> 4) * 16;
        if (wh[0] < 16) x0 = 0;
        for (int y = 0; y < 16 && ok; ++y) {
            std::fseek(f, 8 + (long(y0 + y) * wh[0] + x0) * 4, SEEK_SET);
            ok = std::fread(out + y * 64, 1, 64, f) == 64;
        }
    }
    std::fclose(f);
    return ok;
}
struct Needle { double rot = 0, vel = 0; };
void swing(Needle& n, double want) {
    double d = want - n.rot;
    while (d < -3.141592653589793) d += 6.283185307179586;
    while (d >= 3.141592653589793) d -= 6.283185307179586;
    if (d < -1.0) d = -1.0;
    if (d > 1.0) d = 1.0;
    n.vel += d * 0.1;
    n.vel *= 0.8;
    n.rot += n.vel;
}
uint8_t compassBase[1024], clockBase[1024], dial[1024];
int itemsLoaded = 0; // 0 not tried, 1 ok, -1 missing
Needle compass, clock;
} // namespace

void tickItems(const ItemView& v, const std::function<void(int, const uint8_t*)>& upload) {
    const int COMPASS = 54, CLOCK = 70; // Item.compass, Item.pocketSundial
    if (itemsLoaded == 0) {
        bool ok = readPiece("romfs:/textures/gui/items.rgba", COMPASS, compassBase) &&
                  readPiece("romfs:/textures/gui/items.rgba", CLOCK, clockBase) && readPiece("romfs:/textures/misc/dial.rgba", 0, dial);
        itemsLoaded = ok ? 1 : -1;
    }
    if (itemsLoaded < 0) return;
    // TextureCompassFX: the plain face, then the needle
    std::memcpy(buf, compassBase, sizeof buf);
    double want = 0.0;
    if (v.inWorld) {
        double dx = double(v.spawnX) - v.px, dz = double(v.spawnZ) - v.pz;
        want = double(v.yaw - 90.0f) * 3.141592653589793 / 180.0 - std::atan2(dz, dx);
        if (v.nether) want = rnd() * double(3.1415927f) * 2.0;
    }
    swing(compass, want);
    double sn = std::sin(compass.rot), cs = std::cos(compass.rot);
    auto put = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        int n = (y * 16 + x) & 255;
        buf[n * 4 + 0] = r; buf[n * 4 + 1] = g; buf[n * 4 + 2] = b; buf[n * 4 + 3] = 255;
    };
    for (int i = -4; i <= 4; ++i) put(int(8.5 + cs * i * 0.3), int(7.5 - sn * i * 0.3 * 0.5), 100, 100, 100);
    for (int i = -8; i <= 16; ++i) {
        bool tip = i >= 0;
        put(int(8.5 + sn * i * 0.3), int(7.5 + cs * i * 0.3 * 0.5), tip ? 255 : 100, tip ? 20 : 100, tip ? 20 : 100);
    }
    upload(COMPASS, buf);
    // TextureWatchFX: the dial seen through the clock's window, turned to the sun's angle
    want = 0.0;
    if (v.inWorld) {
        want = double(-v.celestial * 3.1415927f * 2.0f);
        if (v.nether) want = rnd() * double(3.1415927f) * 2.0;
    }
    swing(clock, want);
    sn = std::sin(clock.rot);
    cs = std::cos(clock.rot);
    for (int n = 0; n < 256; ++n) {
        int a = clockBase[n * 4 + 3], r = clockBase[n * 4 + 0], g = clockBase[n * 4 + 1], b = clockBase[n * 4 + 2];
        if (r == b && g == 0 && b > 0) {
            double u = -(double(n % 16) / 15.0 - 0.5), w = double(n / 16) / 15.0 - 0.5;
            int k = r;
            int dx = int((u * cs + w * sn + 0.5) * 16.0), dy = int((w * cs - u * sn + 0.5) * 16.0);
            int d = (dx & 15) + (dy & 15) * 16;
            a = dial[d * 4 + 3];
            r = dial[d * 4 + 0] * r / 255;
            g = dial[d * 4 + 1] * k / 255;
            b = dial[d * 4 + 2] * k / 255;
        }
        buf[n * 4 + 0] = uint8_t(r);
        buf[n * 4 + 1] = uint8_t(g);
        buf[n * 4 + 2] = uint8_t(b);
        buf[n * 4 + 3] = uint8_t(a);
    }
    upload(CLOCK, buf);
}
} // namespace texfx
