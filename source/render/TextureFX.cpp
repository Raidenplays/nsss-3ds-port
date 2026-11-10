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
                int ox = int(MathHelper::sin(float(y) * 3.141593f * 2.0f / 16.0f) * 1.2f);
                int oy = int(MathHelper::sin(float(x) * 3.141593f * 2.0f / 16.0f) * 1.2f);
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

// a romfs .rgba ("RGBA", width, height, RGBA rows), as Java's getRGB ints (ARGB)
bool readArgb(const char* path, int w, int h, uint32_t* out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    uint16_t wh[2];
    bool ok = std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "RGBA", 4) == 0 && std::fread(wh, 2, 2, f) == 2 &&
              wh[0] == w && wh[1] == h;
    for (int n = 0; ok && n < w * h; ++n) {
        uint8_t p[4];
        ok = std::fread(p, 1, 4, f) == 4;
        out[n] = uint32_t(p[3]) << 24 | uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
    }
    std::fclose(f);
    return ok;
}
// a pixel into the tile as the FX classes write it: alpha all or nothing (>>> 24 > 128)
void putArgb(uint8_t* out, int n, uint32_t c) {
    out[n * 4 + 0] = uint8_t(c >> 16);
    out[n * 4 + 1] = uint8_t(c >> 8);
    out[n * 4 + 2] = uint8_t(c);
    out[n * 4 + 3] = (c >> 24) > 128 ? 255 : 0;
}

// TextureGearFX (the turning gear, tiles gear + 16 and + 17, one each way) and TextureGearIdleFX
// (gear + 0 and + 1, standing still): misc/gear.png turned about the middle, misc/gearmiddle.png over it
uint32_t gear[1024], gearMiddle[256];
int gearLoaded = 0; // 0 not tried, 1 ok, -1 missing
struct Gear {
    int tickCounter = 2, step;
    bool idle;
    Gear(int n, bool idle) : step((n << 1) - 1), idle(idle) {}
    void tick(uint8_t* out) {
        tickCounter = idle ? (1 + step) & 63 : (tickCounter + step) & 63;
        float a = MathHelper::sin(float(tickCounter) / 64.0f * 3.1415927f * 2.0f);
        float b = MathHelper::cos(float(tickCounter) / 64.0f * 3.1415927f * 2.0f);
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 16; ++j) {
                float n = (float(i) / 15.0f - 0.5f) * 31.0f, n2 = (float(j) / 15.0f - 0.5f) * 31.0f;
                float n3 = b * n - a * n2;
                n = b * n2 + a * n;
                int n4 = int(n3 + 16.0f), n5 = int(n + 16.0f);
                uint32_t c = 0;
                if (n4 >= 0 && n5 >= 0 && n4 < 32 && n5 < 32) {
                    c = gear[n4 + (n5 << 5)];
                    if ((gearMiddle[i + (j << 4)] >> 24) > 128) c = gearMiddle[i + (j << 4)];
                }
                putArgb(out, i + (j << 4), c);
            }
    }
};

// TextureLeavesFX (tiles 105 and 106): leaves.png's leaf shapes, filled with the season's colours from
// buds.png - winter the bare first frame, spring a frame further on each eighth of the season, summer
// the last, fall leaves.png's own colours
uint32_t leaves[512], buds[4096];
int leavesLoaded = 0;
struct Leaves {
    int n;
    int shown = -1; // what the tile holds now (season * 16 + bud frame), to upload only on a change
    explicit Leaves(int n) : n(n) {}
    bool tick(int season, float progress, uint8_t* out) {
        int frame = season == 0 ? 7 : season == 2 ? 0 : season == 3 ? int(std::floor(progress * 8.0f)) : 0;
        int key = season * 16 + frame;
        if (key == shown) return false;
        shown = key;
        for (int u = 0; u < 16; ++u)
            for (int v = 0; v < 16; ++v) {
                uint32_t leaf = leaves[n * 16 + u + v * 32], c = 0;
                if ((leaf >> 24) > 128) {
                    c = leaf;
                    if (season >= 0 && season != 1) c = buds[n * 16 + u + (v + 16 * frame) * 32];
                }
                putArgb(out, u + (v << 4), c);
            }
        return true;
    }
};

Fluid water, waterFlow;
Lava lava, lavaFlow;
Flames flames0, flames1;
Gear gearTurning0(16, false), gearTurning1(17, false), gearIdle0(0, true), gearIdle1(1, true);
Leaves leaves0(0), leaves1(1);
uint8_t buf[1024];
} // namespace

void tick(int season, float seasonProgress, const std::function<void(int, const uint8_t*)>& upload) {
    // the same order as Minecraft.startGame registers them
    lava.tick(false, buf);
    upload(237, buf);
    water.tick(false, buf);
    upload(205, buf);
    waterFlow.tick(true, buf);
    for (int t : {206, 207, 222, 223}) upload(t, buf);
    lavaFlow.tick(true, buf);
    for (int t : {238, 239, 254, 255}) upload(t, buf);
    flames0.tick(buf);
    upload(31, buf);
    flames1.tick(buf);
    upload(47, buf);
    if (gearLoaded == 0)
        gearLoaded = readArgb("romfs:/textures/misc/gear.rgba", 32, 32, gear) &&
                     readArgb("romfs:/textures/misc/gearmiddle.rgba", 16, 16, gearMiddle) ? 1 : -1;
    if (gearLoaded > 0) {
        const int GEAR = 62; // Block.gear.blockIndexInTexture
        gearTurning0.tick(buf);
        upload(GEAR + 16, buf);
        gearTurning1.tick(buf);
        upload(GEAR + 17, buf);
        gearIdle0.tick(buf);
        upload(GEAR + 0, buf);
        gearIdle1.tick(buf);
        upload(GEAR + 1, buf);
    }
    if (leavesLoaded == 0)
        leavesLoaded = readArgb("romfs:/textures/misc/leaves.rgba", 32, 16, leaves) &&
                       readArgb("romfs:/textures/misc/buds.rgba", 32, 128, buds) ? 1 : -1;
    if (leavesLoaded > 0) {
        if (leaves0.tick(season, seasonProgress, buf)) upload(105, buf);
        if (leaves1.tick(season, seasonProgress, buf)) upload(106, buf);
    }
}

namespace {
// a 16x16 piece of a romfs .rgba ("RGBA", width, height, RGBA rows)
bool readPiece(const char* path, int tile, uint8_t* out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    uint16_t wh[2];
    bool ok = std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "RGBA", 4) == 0 && std::fread(wh, 2, 2, f) == 2;
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
uint8_t compassBase[1024];
int itemsLoaded = 0; // 0 not tried, 1 ok, -1 missing
Needle compass;
} // namespace

void tickItems(const ItemView& v, const std::function<void(int, const uint8_t*)>& upload) {
    const int COMPASS = 54; // Item.compass
    if (itemsLoaded == 0) itemsLoaded = readPiece("romfs:/textures/gui/items.rgba", COMPASS, compassBase) ? 1 : -1;
    if (itemsLoaded < 0) return;
    // TextureCompassFX: the plain face, then the needle - to the spawn, or (yellow) to the block the
    // player is measuring from
    std::memcpy(buf, compassBase, sizeof buf);
    double want = 0.0;
    if (v.inWorld) {
        double dx = v.measuring ? v.measureX + 0.5 - v.px : v.spawnX + 0.5 - v.px;
        double dz = v.measuring ? v.measureZ + 0.5 - v.pz : v.spawnZ + 0.5 - v.pz;
        want = double(v.yaw - 90.0f) * 3.141592653589793 / 180.0 - std::atan2(dz, dx);
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
        put(int(8.5 + sn * i * 0.3), int(7.5 + cs * i * 0.3 * 0.5), tip ? 255 : 100, tip ? (v.measuring ? 255 : 20) : 100, tip ? 20 : 100);
    }
    upload(COMPASS, buf);
}
} // namespace texfx
