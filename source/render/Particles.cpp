#include "Particles.h"
#include <algorithm>
#include <cmath>
#include "Renderer.h"
#include "model/BlockModels.h"
#include "world/BlockProps.h"
#include "world/BlockTable.h"
#include "world/MathHelper.h"
#include "world/Physics.h"
#include "world/World.h"
#include "audio/Sound.h"

Particles* gParticles = nullptr;

namespace {
constexpr size_t MAX_PARTICLES = 400; // Old 3DS: Beta allows 4000
constexpr float PI_F = 3.1415927f;
inline int fl(double v) { return int(std::floor(v)); }
}

// EntityFX's constructor: a little random push, a random size and lifetime
Particles::P Particles::make(Type t, double x, double y, double z, double mx, double my, double mz) {
    P p{};
    p.type = t;
    p.x = p.px = x;
    p.y = p.py = y;
    p.z = p.pz = z;
    p.r = p.g = p.b = 1.0f;
    p.mx = mx + double(float(rnd() * 2.0 - 1.0) * 0.4f);
    p.my = my + double(float(rnd() * 2.0 - 1.0) * 0.4f);
    p.mz = mz + double(float(rnd() * 2.0 - 1.0) * 0.4f);
    float sp = float(rnd() + rnd() + 1.0) * 0.15f;
    float len = MathHelper::sqrt_double(p.mx * p.mx + p.my * p.my + p.mz * p.mz);
    if (len > 0.0f) {
        p.mx = p.mx / double(len) * double(sp) * double(0.4f);
        p.my = p.my / double(len) * double(sp) * double(0.4f) + double(0.1f);
        p.mz = p.mz / double(len) * double(sp) * double(0.4f);
    }
    p.jx = rand.nextFloat() * 3.0f;
    p.jy = rand.nextFloat() * 3.0f;
    p.scale = p.baseScale = (rand.nextFloat() * 0.5f + 0.5f) * 2.0f;
    p.maxAge = int(4.0f / (rand.nextFloat() * 0.9f + 0.1f));
    switch (t) {
    case SMOKE: case REDDUST:
        p.mx = p.mx * double(0.1f) + mx;
        p.my = p.my * double(0.1f) + my;
        p.mz = p.mz * double(0.1f) + mz;
        p.r = p.g = p.b = float(rnd() * double(0.3f));
        if (t == REDDUST) { p.r = float(rnd() * 0.2 + 0.8); p.g = p.b = 0.0f; }
        p.scale = p.baseScale = p.scale * 0.75f;
        p.maxAge = int(8.0 / (rnd() * 0.8 + 0.2));
        break;
    case FLAME:
        p.mx = p.mx * double(0.01f) + mx;
        p.my = p.my * double(0.01f) + my;
        p.mz = p.mz * double(0.01f) + mz;
        p.maxAge = int(8.0 / (rnd() * 0.8 + 0.2)) + 4;
        p.noClip = true;
        p.tex = 48;
        break;
    case EXPLODE:
        p.mx = mx + double(float(rnd() * 2.0 - 1.0) * 0.05f);
        p.my = my + double(float(rnd() * 2.0 - 1.0) * 0.05f);
        p.mz = mz + double(float(rnd() * 2.0 - 1.0) * 0.05f);
        p.r = p.g = p.b = rand.nextFloat() * 0.3f + 0.7f;
        p.scale = p.baseScale = rand.nextFloat() * rand.nextFloat() * 6.0f + 1.0f;
        p.maxAge = int(16.0 / (double(rand.nextFloat()) * 0.8 + 0.2)) + 2;
        break;
    case LAVA:
        p.mx *= double(0.8f);
        p.mz *= double(0.8f);
        p.my = double(rand.nextFloat() * 0.4f + 0.05f);
        p.scale = p.baseScale = p.scale * (rand.nextFloat() * 2.0f + 0.2f);
        p.maxAge = int(16.0 / (rnd() * 0.8 + 0.2));
        p.tex = 49;
        break;
    case PORTAL: { // EntityPortalFX: purple sparks
        float f = rand.nextFloat() * 0.6f + 0.4f;
        p.r = 0.9f * f; p.g = 0.3f * f; p.b = f;
        p.mx = mx; p.my = my; p.mz = mz;
        p.scale = p.baseScale = rand.nextFloat() * 0.2f + 0.5f;
        p.maxAge = int(rnd() * 10.0) + 40;
        p.tex = int(rnd() * 8.0);
        p.noClip = true;
        p.sx = x; p.sy = y; p.sz = z;
        break;
    }
    case NOTE: { // EntityNoteFX: a coloured note that floats up
        float f = float(mx);
        p.mx *= double(0.01f); p.my *= double(0.01f); p.mz *= double(0.01f);
        p.my += 0.2;
        p.r = std::sin((f + 0.0f) * 3.1415927f * 2.0f) * 0.65f + 0.35f;
        p.g = std::sin((f + 1.0f / 3.0f) * 3.1415927f * 2.0f) * 0.65f + 0.35f;
        p.b = std::sin((f + 2.0f / 3.0f) * 3.1415927f * 2.0f) * 0.65f + 0.35f;
        p.scale = p.baseScale = p.scale * 0.75f * 2.0f;
        p.maxAge = 6;
        p.tex = 64;
        break;
    }
    case SNOWPOOF: // EntitySlimeFX with the snowball: a little white puff
    case SLIME:    // ... and with the slimeball (TODO(particles): the item's own icon, as in the jar)
        p.r = p.b = t == SLIME ? 0.5f : 1.0f;
        p.g = 1.0f;
        p.tex = 7;
        p.gravity = 0.25f;
        p.scale = p.baseScale = p.scale / 2.0f;
        break;
    case SPLASH: // EntitySplashFX (EntityRainFX with less gravity, one tile on)
        p.mx *= double(0.3f);
        p.my = double(float(rnd()) * 0.2f + 0.1f);
        p.mz *= double(0.3f);
        p.r = p.g = p.b = 1.0f;
        p.tex = 19 + rand.nextInt(4) + 1;
        p.gravity = 0.04f;
        p.maxAge = int(8.0 / (rnd() * 0.8 + 0.2));
        if (my == 0.0 && (mx != 0.0 || mz != 0.0)) {
            p.mx = mx;
            p.my = my + 0.1;
            p.mz = mz;
        }
        break;
    case BUBBLE:
        p.r = p.g = p.b = 1.0f;
        p.tex = 32;
        p.mx = mx * double(0.2f) + double(float(rnd() * 2.0 - 1.0) * 0.02f);
        p.my = my * double(0.2f) + double(float(rnd() * 2.0 - 1.0) * 0.02f);
        p.mz = mz * double(0.2f) + double(float(rnd() * 2.0 - 1.0) * 0.02f);
        p.scale = p.baseScale = p.scale * (rand.nextFloat() * 0.6f + 0.2f);
        p.maxAge = int(8.0 / (rnd() * 0.8 + 0.2));
        break;
    default: break;
    }
    return p;
}

void Particles::spawn(Type t, double x, double y, double z, double mx, double my, double mz) {
    if (list.size() >= MAX_PARTICLES) list.erase(list.begin());
    list.push_back(make(t, x, y, z, mx, my, mz));
}

void Particles::blockBroken(const World& w, int x, int y, int z, int id, int meta) {
    if (id <= 0) return;
    (void)w;
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c) {
                double px = x + (a + 0.5) / 4.0, py = y + (b + 0.5) / 4.0, pz = z + (c + 0.5) / 4.0;
                P p = make(DIG, px, py, pz, px - x - 0.5, py - y - 0.5, pz - z - 0.5);
                p.tex = BLOCKS[id].tex[meta & 15][2];
                p.gravity = 1.0f;
                p.r = p.g = p.b = 0.6f;
                p.scale = p.baseScale = p.scale / 2.0f;
                if (list.size() >= MAX_PARTICLES) list.erase(list.begin());
                list.push_back(p);
            }
}

void Particles::blockHit(const World& w, int x, int y, int z, int side) {
    int id = w.getBlockId(x, y, z);
    if (id <= 0) return;
    const float e = 0.1f;
    double px = x + rnd() * (1.0 - e * 2.0) + e, py = y + rnd() * (1.0 - e * 2.0) + e, pz = z + rnd() * (1.0 - e * 2.0) + e;
    if (side == 0) py = y - e;
    if (side == 1) py = y + 1.0 + e;
    if (side == 2) pz = z - e;
    if (side == 3) pz = z + 1.0 + e;
    if (side == 4) px = x - e;
    if (side == 5) px = x + 1.0 + e;
    P p = make(DIG, px, py, pz, 0, 0, 0);
    p.tex = BLOCKS[id].tex[w.getBlockMetadata(x, y, z) & 15][2];
    p.gravity = 1.0f;
    p.r = p.g = p.b = 0.6f;
    p.scale /= 2.0f;
    // func_407_b(0.2) and func_405_d(0.6)
    p.mx *= double(0.2f);
    p.my = (p.my - double(0.1f)) * double(0.2f) + double(0.1f);
    p.mz *= double(0.2f);
    p.scale = p.baseScale = p.scale * 0.6f;
    if (list.size() >= MAX_PARTICLES) list.erase(list.begin());
    list.push_back(p);
}

void Particles::spawnLargeSmoke(double x, double y, double z) {
    spawn(SMOKE, x, y, z, 0, 0, 0);
    if (!list.empty()) { list.back().scale *= 2.0f; list.back().baseScale *= 2.0f; }
}

void Particles::ambient(const World& w, double px, double py, double pz) {
    int cx = fl(px), cy = fl(py), cz = fl(pz);
    for (int i = 0; i < 1000; ++i) {
        int x = cx + rand.nextInt(16) - rand.nextInt(16);
        int y = cy + rand.nextInt(16) - rand.nextInt(16);
        int z = cz + rand.nextInt(16) - rand.nextInt(16);
        if (y < 0 || y >= 256) continue;
        int id = w.getBlockId(x, y, z);
        switch (id) {
        case 51: { // BlockFire.randomDisplayTick: crackle and large smoke
            if (rand.nextInt(24) == 0)
                sound::play("fire.fire", x + 0.5, y + 0.5, z + 0.5, 1.0f + rand.nextFloat(), rand.nextFloat() * 0.7f + 0.3f);
            for (int k = 0; k < 3; ++k) {
                double sx = x + rand.nextFloat(), sy = y + rand.nextFloat() * 0.5f + 0.5f, sz = z + rand.nextFloat();
                spawn(SMOKE, sx, sy, sz, 0, 0, 0);
                if (!list.empty()) { list.back().scale *= 2.0f; list.back().baseScale *= 2.0f; }
            }
            break;
        }
        case 55: // BlockRedstoneWire.randomDisplayTick: powered dust sparkles
            if (w.getBlockMetadata(x, y, z) > 0)
                spawn(REDDUST, x + 0.5 + (rand.nextFloat() - 0.5) * 0.2, y + 1.0 / 16.0, z + 0.5 + (rand.nextFloat() - 0.5) * 0.2, 0, 0, 0);
            break;
        case 94: { // BlockPowerRepeater (lit)
            int m = w.getBlockMetadata(x, y, z) & 3;
            int fx = m == 1 ? -1 : m == 3 ? 1 : 0, fz = m == 0 ? 1 : m == 2 ? -1 : 0;
            double t = rand.nextBoolean() ? 0.25 : -0.15;
            spawn(REDDUST, x + 0.5 + fx * t + (rand.nextDouble() - 0.5) * 0.2, y + 0.45, z + 0.5 + fz * t + (rand.nextDouble() - 0.5) * 0.2, 0, 0, 0);
            break;
        }
        case 90: { // BlockPortal.randomDisplayTick: the hum, and sparks drifting off both faces
            if (rand.nextInt(100) == 0)
                sound::play("portal.portal", x + 0.5, y + 0.5, z + 0.5, 1.0f, rand.nextFloat() * 0.4f + 0.8f);
            bool alongX = w.getBlockId(x - 1, y, z) == 90 || w.getBlockId(x + 1, y, z) == 90;
            for (int k = 0; k < 4; ++k) {
                double sx = x + rand.nextFloat(), sy = y + rand.nextFloat(), sz = z + rand.nextFloat();
                int side = rand.nextInt(2) * 2 - 1;
                double mx = (rand.nextFloat() - 0.5) * 0.5, my = (rand.nextFloat() - 0.5) * 0.5, mz = (rand.nextFloat() - 0.5) * 0.5;
                if (!alongX) {
                    sx = x + 0.5 + 0.25 * side;
                    mx = rand.nextFloat() * 2.0f * side;
                } else {
                    sz = z + 0.5 + 0.25 * side;
                    mz = rand.nextFloat() * 2.0f * side;
                }
                spawn(PORTAL, sx, sy, sz, mx, my, mz);
            }
            break;
        }
        case 111: // BlockMagicLight.randomDisplayTick
            spawn(PORTAL, x + 0.5, y + 0.5, z + 0.5, (rand.nextDouble() - 0.5) * 0.6, (rand.nextDouble() - 0.5) * 0.6, (rand.nextDouble() - 0.5) * 0.6);
            break;
        case 50: { // BlockTorch.randomDisplayTick
            int m = w.getBlockMetadata(x, y, z);
            double fx = x + 0.5, fy = y + 0.7, fz = z + 0.5, up = 0.22, out = 0.27;
            if (m == 1) { spawn(SMOKE, fx - out, fy + up, fz, 0, 0, 0); spawn(FLAME, fx - out, fy + up, fz, 0, 0, 0); }
            else if (m == 2) { spawn(SMOKE, fx + out, fy + up, fz, 0, 0, 0); spawn(FLAME, fx + out, fy + up, fz, 0, 0, 0); }
            else if (m == 3) { spawn(SMOKE, fx, fy + up, fz - out, 0, 0, 0); spawn(FLAME, fx, fy + up, fz - out, 0, 0, 0); }
            else if (m == 4) { spawn(SMOKE, fx, fy + up, fz + out, 0, 0, 0); spawn(FLAME, fx, fy + up, fz + out, 0, 0, 0); }
            else { spawn(SMOKE, fx, fy, fz, 0, 0, 0); spawn(FLAME, fx, fy, fz, 0, 0, 0); }
            break;
        }
        case 62: { // BlockFurnace (lit)
            int m = w.getBlockMetadata(x, y, z);
            double fx = x + 0.5, fy = y + rand.nextFloat() * 6.0f / 16.0f, fz = z + 0.5, a = 0.52, b = rand.nextFloat() * 0.6f - 0.3f;
            if (m == 4) { spawn(SMOKE, fx - a, fy, fz + b, 0, 0, 0); spawn(FLAME, fx - a, fy, fz + b, 0, 0, 0); }
            else if (m == 5) { spawn(SMOKE, fx + a, fy, fz + b, 0, 0, 0); spawn(FLAME, fx + a, fy, fz + b, 0, 0, 0); }
            else if (m == 2) { spawn(SMOKE, fx + b, fy, fz - a, 0, 0, 0); spawn(FLAME, fx + b, fy, fz - a, 0, 0, 0); }
            else if (m == 3) { spawn(SMOKE, fx + b, fy, fz + a, 0, 0, 0); spawn(FLAME, fx + b, fy, fz + a, 0, 0, 0); }
            break;
        }
        case 10: case 11: // BlockFluids (lava): pops out of the surface
            if (w.getBlockId(x, y + 1, z) == 0 && !BP[w.getBlockId(x, y + 1, z)].opaqueCube && rand.nextInt(100) == 0)
                spawn(LAVA, x + rand.nextFloat(), y + 1.0, z + rand.nextFloat(), 0, 0, 0);
            break;
        case 76: // BlockRedstoneTorch (on)
            spawn(REDDUST, x + 0.5 + (rand.nextFloat() - 0.5f) * 0.2, y + 0.7 + (rand.nextFloat() - 0.5f) * 0.2,
                  z + 0.5 + (rand.nextFloat() - 0.5f) * 0.2, 0, 0, 0);
            break;
        default: break;
        }
    }
}

void Particles::move(const World& w, P& p) {
    if (p.noClip) {
        p.x += p.mx;
        p.y += p.my;
        p.z += p.mz;
        return;
    }
    // Entity.moveEntity for a 0.2 box (yOffset = half its height)
    double h = 0.1 * (p.type == DIG ? 1.0 : 1.0);
    AABB b{p.x - h, p.y - h, p.z - h, p.x + h, p.y + h, p.z + h};
    static std::vector<AABB> boxes;
    Physics::collidingBoxes(w, b.addCoord(p.mx, p.my, p.mz), boxes);
    double dx = p.mx, dy = p.my, dz = p.mz;
    for (const AABB& o : boxes) dy = o.calculateYOffset(b, dy);
    b.move(0, dy, 0);
    for (const AABB& o : boxes) dx = o.calculateXOffset(b, dx);
    b.move(dx, 0, 0);
    for (const AABB& o : boxes) dz = o.calculateZOffset(b, dz);
    b.move(0, 0, dz);
    p.onGround = dy != p.my && p.my < 0.0;
    if (dx != p.mx) p.mx = 0;
    if (dy != p.my) p.my = 0;
    if (dz != p.mz) p.mz = 0;
    p.x = (b.minX + b.maxX) / 2.0;
    p.y = (b.minY + b.maxY) / 2.0;
    p.z = (b.minZ + b.maxZ) / 2.0;
}

void Particles::tick(const World& w) {
    for (P& p : list) {
        p.px = p.x;
        p.py = p.y;
        p.pz = p.z;
        if (p.age++ >= p.maxAge) { p.maxAge = -1; continue; }
        switch (p.type) {
        case SMOKE: case EXPLODE: case REDDUST:
            p.tex = 7 - p.age * 8 / std::max(1, p.maxAge);
            p.my += 0.004;
            move(w, p);
            if (p.type != EXPLODE && p.y == p.py) { p.mx *= 1.1; p.mz *= 1.1; }
            { double f = p.type == EXPLODE ? double(0.9f) : double(0.96f); p.mx *= f; p.my *= f; p.mz *= f; }
            break;
        case FLAME:
            move(w, p);
            p.mx *= double(0.96f); p.my *= double(0.96f); p.mz *= double(0.96f);
            break;
        case PORTAL: { // EntityPortalFX.onUpdate: swept out along its motion, then pulled back in
            float f1 = float(p.age - 1) / float(p.maxAge), f = -f1 + f1 * f1 * 2.0f;
            f = 1.0f - f;
            p.x = p.sx + p.mx * f;
            p.y = p.sy + p.my * f + double(1.0f - f1);
            p.z = p.sz + p.mz * f;
            break;
        }
        case NOTE:
            move(w, p);
            if (p.y == p.py) { p.mx *= 1.1; p.mz *= 1.1; }
            p.mx *= double(0.66f); p.my *= double(0.66f); p.mz *= double(0.66f);
            if (p.onGround) { p.mx *= double(0.7f); p.mz *= double(0.7f); }
            break;
        case LAVA: {
            float t = float(p.age) / float(p.maxAge);
            if (rand.nextFloat() > t && list.size() < MAX_PARTICLES) {} // (the smoke trail is left out on 3DS)
            p.my -= 0.03;
            move(w, p);
            p.mx *= double(0.999f); p.my *= double(0.999f); p.mz *= double(0.999f);
            break;
        }
        case SPLASH: { // EntityRainFX.onUpdate
            p.my -= double(p.gravity);
            move(w, p);
            p.mx *= double(0.98f); p.my *= double(0.98f); p.mz *= double(0.98f);
            if (p.onGround) {
                if (rnd() < 0.5) p.maxAge = -1;
                p.mx *= double(0.7f);
                p.mz *= double(0.7f);
            }
            int bx = fl(p.x), by = fl(p.y), bz = fl(p.z);
            int id = w.getBlockId(bx, by, bz);
            Mat mt = BP[id].mat;
            if (matLiquid(mt) || matSolid(mt)) {
                int meta = w.getBlockMetadata(bx, by, bz);
                if (meta >= 8) meta = 0;
                double surface = double(float(by + 1) - float(meta + 1) / 9.0f);
                if (p.y < surface) p.maxAge = -1;
            }
            break;
        }
        case BUBBLE: {
            p.my += 0.002;
            move(w, p);
            p.mx *= 0.85; p.my *= 0.85; p.mz *= 0.85;
            int id = w.getBlockId(fl(p.x), fl(p.y), fl(p.z));
            if (id != 8 && id != 9) p.maxAge = -1;
            break;
        }
        default: // EntityFX.onUpdate (block bits)
            p.my -= 0.04 * double(p.gravity);
            move(w, p);
            p.mx *= double(0.98f); p.my *= double(0.98f); p.mz *= double(0.98f);
            break;
        }
        if (p.onGround) { p.mx *= double(0.7f); p.mz *= double(0.7f); }
    }
    list.erase(std::remove_if(list.begin(), list.end(), [](const P& p) { return p.maxAge < 0; }), list.end());
}

void Particles::render(const World& w, Renderer& r, float partial, float yaw, float pitch) {
    if (list.empty()) return;
    // EffectRenderer.func_1189_a: the camera's right and up
    float f1 = MathHelper::cos(yaw * PI_F / 180.0f), f2 = MathHelper::sin(yaw * PI_F / 180.0f);
    float f3 = -f2 * MathHelper::sin(pitch * PI_F / 180.0f), f4 = f1 * MathHelper::sin(pitch * PI_F / 180.0f);
    float f5 = MathHelper::cos(pitch * PI_F / 180.0f);
    quads[0].clear();
    quads[1].clear();
    for (const P& p : list) {
        float x = float(p.px + (p.x - p.px) * partial), y = float(p.py + (p.y - p.py) * partial),
              z = float(p.pz + (p.z - p.pz) * partial);
        float t = (float(p.age) + partial) / float(std::max(1, p.maxAge));
        float s = p.baseScale;
        if (p.type == SMOKE || p.type == REDDUST) s *= std::min(1.0f, std::max(0.0f, t * 32.0f));
        else if (p.type == FLAME) s *= 1.0f - t * t * 0.5f;
        else if (p.type == LAVA) s *= 1.0f - t * t;
        float sz = 0.1f * s;
        float light = w.getBrightness(fl(x), fl(y), fl(z));
        if (p.type == LAVA) light = 1.0f;
        else if (p.type == FLAME) { float a = std::min(1.0f, std::max(0.0f, t)); light = light * a + (1.0f - a); }
        float shade = light * (p.r + p.g + p.b) / 3.0f;
        float u0, u1, v0, v1;
        int layer;
        if (p.type == DIG) { // a quarter of the block's own texture
            layer = 1;
            int tile = p.tex;
            u0 = (float(tile & 15) + p.jx / 4.0f) / 16.0f;
            u1 = u0 + 0.999f / 64.0f;
            v0 = (float(tile >> 4) * 16.0f + p.jy * 4.0f) / 1024.0f;
            v1 = v0 + 3.996f / 1024.0f;
        } else {
            layer = 0;
            u0 = float(p.tex % 16) / 16.0f;
            u1 = u0 + 0.999f / 16.0f;
            v0 = float(p.tex / 16) / 16.0f;
            v1 = v0 + 0.999f / 16.0f;
        }
        axm::ModelVertex q[4] = {
            {x - f1 * sz - f3 * sz, y - f5 * sz, z - f2 * sz - f4 * sz, shade, u1, v1},
            {x - f1 * sz + f3 * sz, y + f5 * sz, z - f2 * sz + f4 * sz, shade, u1, v0},
            {x + f1 * sz + f3 * sz, y + f5 * sz, z + f2 * sz + f4 * sz, shade, u0, v0},
            {x + f1 * sz - f3 * sz, y - f5 * sz, z + f2 * sz - f4 * sz, shade, u0, v1},
        };
        static const int T[6] = {0, 1, 2, 0, 2, 3};
        for (int i : T) quads[layer].push_back(q[i]);
    }
    if (!quads[0].empty()) r.drawTriangles(quads[0], r.texture("romfs:/textures/particles.rgba"));
    if (!quads[1].empty()) r.drawTriangles(quads[1], r.terrainTexture());
}
