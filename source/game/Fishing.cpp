// EntityFish + ItemFishingRod + RenderFish: casting, the bobber floating and dipping when a fish
// bites, reeling in a fish (or a hooked mob), and the line sagging back to the rod.
#include <cmath>
#include "ItemEntities.h"
#include "Mobs.h"
#include "Player.h"
#include "audio/Sound.h"
#include "render/Particles.h"
#include "render/Renderer.h"
#include "world/MathHelper.h"
#include "world/Physics.h"
#include "world/World.h"

namespace {
constexpr float PI_F = 3.1415927f;
constexpr double SIZE = 0.25; // setSize(0.25, 0.25), yOffset 0
int fl(double v) { int i = int(v); return v < double(i) ? i - 1 : i; }
AABB boxAt(double x, double y, double z) { return {x - SIZE / 2, y, z - SIZE / 2, x + SIZE / 2, y + SIZE, z + SIZE / 2}; }
} // namespace

void Mobs::castBobber(const Player& p) {
    Bobber& b = bobber;
    b = Bobber{};
    b.active = true;
    float yaw = p.rotationYaw, pitch = p.rotationPitch;
    // setLocationAndAngles at the eyes, a little to the right and down
    b.x = p.posX - double(MathHelper::cos(yaw / 180.0f * PI_F) * 0.16f);
    b.y = p.posY - double(0.1f);
    b.z = p.posZ - double(MathHelper::sin(yaw / 180.0f * PI_F) * 0.16f);
    const float v = 0.4f;
    double mx = double(-MathHelper::sin(yaw / 180.0f * PI_F) * MathHelper::cos(pitch / 180.0f * PI_F) * v);
    double mz = double(MathHelper::cos(yaw / 180.0f * PI_F) * MathHelper::cos(pitch / 180.0f * PI_F) * v);
    double my = double(-MathHelper::sin(pitch / 180.0f * PI_F) * v);
    // func_4042_a(.., 1.5, 1.0)
    float l = MathHelper::sqrt_double(mx * mx + my * my + mz * mz);
    mx /= double(l);
    my /= double(l);
    mz /= double(l);
    mx += jr.nextGaussian() * double(0.0075f);
    my += jr.nextGaussian() * double(0.0075f);
    mz += jr.nextGaussian() * double(0.0075f);
    b.mx = mx * 1.5;
    b.my = my * 1.5;
    b.mz = mz * 1.5;
    float h = MathHelper::sqrt_double(b.mx * b.mx + b.mz * b.mz);
    b.yaw = float(std::atan2(b.mx, b.mz) * 180.0 / double(PI_F));
    b.pitch = float(std::atan2(b.my, double(h)) * 180.0 / double(PI_F));
    b.px = b.x;
    b.py = b.y;
    b.pz = b.z;
}

int Mobs::reelIn(const Player& p, ItemEntities& items) {
    Bobber& b = bobber;
    int wear = 0;
    double dx = p.posX - b.x, dy = p.posY - b.y, dz = p.posZ - b.z;
    double d = double(MathHelper::sqrt_double(dx * dx + dy * dy + dz * dz));
    if (Mob* m = b.hooked ? byUid(b.hooked) : nullptr) { // pull the hooked mob in
        m->mx += dx * 0.1;
        m->my += dy * 0.1 + double(MathHelper::sqrt_double(d)) * 0.08;
        m->mz += dz * 0.1;
        wear = 3;
    } else if (b.bite > 0) { // a fish, flung toward you
        ItemStack fish;
        fish.id = 349;
        fish.count = 1;
        items.spawnMoving(b.x, b.y, b.z, fish, dx * 0.1, dy * 0.1 + double(MathHelper::sqrt_double(d)) * 0.08, dz * 0.1, 10);
        wear = 1;
    }
    if (b.inGround) wear = 2;
    b.active = false;
    return wear;
}

void Mobs::tickBobber(World& w, const Player& p, bool holdingRod) {
    Bobber& b = bobber;
    if (!b.active) return;
    b.px = b.x;
    b.py = b.y;
    b.pz = b.z;
    double ddx = b.x - p.posX, ddy = b.y - (p.posY - double(Player::YOFFSET)), ddz = b.z - p.posZ;
    if (p.dead() || !holdingRod || ddx * ddx + ddy * ddy + ddz * ddz > 1024.0) {
        b.active = false;
        return;
    }
    if (b.hooked) { // riding along on the hooked mob
        if (Mob* m = byUid(b.hooked); m && !m->dead && m->health > 0) {
            b.x = m->x;
            b.y = m->box.minY + double(m->height) * 0.8;
            b.z = m->z;
            return;
        }
        b.hooked = 0;
    }
    if (b.shake > 0) --b.shake;
    if (b.inGround) {
        if (w.getBlockId(b.tx, b.ty, b.tz) == b.inTile) {
            if (++b.groundTicks == 1200) b.active = false;
            return;
        }
        b.inGround = false;
        b.mx *= double(jr.nextFloat() * 0.2f);
        b.my *= double(jr.nextFloat() * 0.2f);
        b.mz *= double(jr.nextFloat() * 0.2f);
        b.groundTicks = 0;
        b.airTicks = 0;
    } else {
        ++b.airTicks;
    }
    // what it would hit on the way this tick: a block, or something alive
    double len = std::sqrt(b.mx * b.mx + b.my * b.my + b.mz * b.mz);
    if (len > 1e-9) {
        double ux = b.mx / len, uy = b.my / len, uz = b.mz / len;
        RayHit h = w.rayTrace(float(b.x), float(b.y), float(b.z), float(ux), float(uy), float(uz), float(len));
        double reach = len;
        if (h.hit) {
            double cx = h.x + 0.5 - b.x, cy = h.y + 0.5 - b.y, cz = h.z + 0.5 - b.z;
            reach = std::max(0.0, std::sqrt(cx * cx + cy * cy + cz * cz) - 0.5);
        }
        double t;
        int v = mobOnPath(b.x, b.y, b.z, ux, uy, uz, reach, 0.3, t);
        if (v >= 0) {
            hurt(mobs[v], 0, true, b.x, b.z, nullptr, &w); // attackEntityFrom(angler, 0): it's hooked
            if (mobs[v].uid == 0) mobs[v].uid = nextUid++;
            b.hooked = mobs[v].uid;
        } else if (h.hit) {
            b.inGround = true;
            b.tx = h.x;
            b.ty = h.y;
            b.tz = h.z;
            b.inTile = w.getBlockId(h.x, h.y, h.z);
        }
    }
    if (b.inGround) return;
    // moveEntity
    AABB box = boxAt(b.x, b.y, b.z);
    double dx = b.mx, dy = b.my, dz = b.mz;
    Physics::collidingBoxes(w, box.addCoord(dx, dy, dz), boxes);
    for (const AABB& a : boxes) dy = a.calculateYOffset(box, dy);
    box.move(0.0, dy, 0.0);
    for (const AABB& a : boxes) dx = a.calculateXOffset(box, dx);
    box.move(dx, 0.0, 0.0);
    for (const AABB& a : boxes) dz = a.calculateZOffset(box, dz);
    box.move(0.0, 0.0, dz);
    bool onGround = dy != b.my && b.my < 0.0, collidedH = dx != b.mx || dz != b.mz;
    if (dx != b.mx) b.mx = 0.0;
    if (dy != b.my) b.my = 0.0;
    if (dz != b.mz) b.mz = 0.0;
    b.x = (box.minX + box.maxX) / 2.0;
    b.y = box.minY;
    b.z = (box.minZ + box.maxZ) / 2.0;
    float drag = (onGround || collidedH) ? 0.5f : 0.92f;
    // how deep it floats, and now and then a bite: it bobs under with a splash
    const int N = 5;
    double wet = 0.0;
    for (int i = 0; i < N; ++i) {
        double y0 = box.minY + (box.maxY - box.minY) * double(i) / double(N) - 0.125 + 0.125;
        double y1 = box.minY + (box.maxY - box.minY) * double(i + 1) / double(N) - 0.125 + 0.125;
        if (Physics::isMaterialInBB(w, AABB{box.minX, y0, box.minZ, box.maxX, y1, box.maxZ}, false)) wet += 1.0 / double(N);
    }
    if (wet > 0.0) {
        if (b.bite > 0) {
            --b.bite;
        } else if (jr.nextInt(500) == 0) {
            b.bite = jr.nextInt(30) + 10;
            b.my -= double(0.2f);
            sound::play("random.splash", b.x, b.y, b.z, 0.25f, 1.0f + (jr.nextFloat() - jr.nextFloat()) * 0.4f);
            float surface = float(fl(box.minY));
            const float wd = float(SIZE);
            if (gParticles) {
                for (int i = 0; float(i) < 1.0f + wd * 20.0f; ++i) {
                    float ox = (jr.nextFloat() * 2.0f - 1.0f) * wd, oz = (jr.nextFloat() * 2.0f - 1.0f) * wd;
                    gParticles->spawn(Particles::BUBBLE, b.x + double(ox), double(surface + 1.0f), b.z + double(oz), b.mx,
                                      b.my - double(jr.nextFloat() * 0.2f), b.mz);
                }
                for (int i = 0; float(i) < 1.0f + wd * 20.0f; ++i) {
                    float ox = (jr.nextFloat() * 2.0f - 1.0f) * wd, oz = (jr.nextFloat() * 2.0f - 1.0f) * wd;
                    gParticles->spawn(Particles::SPLASH, b.x + double(ox), double(surface + 1.0f), b.z + double(oz), b.mx, b.my, b.mz);
                }
            }
        }
    }
    if (b.bite > 0) b.my -= double(jr.nextFloat() * jr.nextFloat() * jr.nextFloat()) * 0.2;
    b.my += double(0.04f) * (wet * 2.0 - 1.0);
    if (wet > 0.0) {
        drag = float(double(drag) * 0.9);
        b.my *= 0.8;
    }
    b.mx *= double(drag);
    b.my *= double(drag);
    b.mz *= double(drag);
}

void Mobs::renderBobber(Renderer& r, const Player& p, float partial, float camYaw, float camPitch) {
    const Bobber& b = bobber;
    if (!b.active) return;
    float x = float(b.px + (b.x - b.px) * partial), y = float(b.py + (b.y - b.py) * partial), z = float(b.pz + (b.z - b.pz) * partial);
    // the float: particles.png (1, 2), half a block, facing the camera
    float cy = MathHelper::cos(camYaw * PI_F / 180.0f), sy = MathHelper::sin(camYaw * PI_F / 180.0f);
    float sp = MathHelper::sin(camPitch * PI_F / 180.0f), cp = MathHelper::cos(camPitch * PI_F / 180.0f);
    float rx = cy, rz = sy, ux = -sy * sp, uy = cp, uz = cy * sp;
    float u0 = 8.0f / 128.0f, u1 = 16.0f / 128.0f, v0 = 16.0f / 128.0f, v1 = 24.0f / 128.0f;
    const float s = 0.5f;
    auto at = [&](float a, float c, float u, float v) {
        return axm::ModelVertex{x + (rx * a + ux * c) * s, y + (uy * c) * s, z + (rz * a + uz * c) * s, 1.0f, u, v};
    };
    std::vector<axm::ModelVertex> q;
    axm::ModelVertex v[4] = {at(-0.5f, -0.5f, u0, v1), at(0.5f, -0.5f, u1, v1), at(0.5f, 0.5f, u1, v0), at(-0.5f, 0.5f, u0, v0)};
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int i : T) q.push_back(v[i]);
    r.drawTriangles(q, r.texture("romfs:/textures/particles.rgba"));
    // the line: from the rod's tip (first person) sagging down to the float
    float yaw = p.rotationYaw * PI_F / 180.0f, pitch = p.rotationPitch * PI_F / 180.0f;
    double sn = double(MathHelper::sin(yaw)), cs = double(MathHelper::cos(yaw));
    double spn = double(MathHelper::sin(pitch)), cpn = double(MathHelper::cos(pitch));
    double ex = p.prevPosX + (p.posX - p.prevPosX) * double(partial) - cs * 0.7 - sn * 0.5 * cpn;
    double ey = p.prevPosY + (p.posY - p.prevPosY) * double(partial) - spn * 0.5;
    double ez = p.prevPosZ + (p.posZ - p.prevPosZ) * double(partial) - sn * 0.7 + cs * 0.5 * cpn;
    double dx = double(float(ex - x)), dy = double(float(ey - (double(y) + 0.25))), dz = double(float(ez - z));
    q.clear();
    const int SEG = 16;
    const float wdt = 0.012f;
    for (int i = 0; i < SEG; ++i) {
        float f0 = float(i) / SEG, f1 = float(i + 1) / SEG;
        float ax = x + float(dx) * f0, ay = y + float(dy * double(f0 * f0 + f0) * 0.5) + 0.25f, az = z + float(dz) * f0;
        float bx = x + float(dx) * f1, by = y + float(dy * double(f1 * f1 + f1) * 0.5) + 0.25f, bz = z + float(dz) * f1;
        // a thin strip turned to the camera
        axm::ModelVertex l[4] = {{ax - ux * wdt, ay - uy * wdt, az - uz * wdt, 0.0f, 0, 0}, {ax + ux * wdt, ay + uy * wdt, az + uz * wdt, 0.0f, 0, 0},
                                 {bx + ux * wdt, by + uy * wdt, bz + uz * wdt, 0.0f, 0, 0}, {bx - ux * wdt, by - uy * wdt, bz - uz * wdt, 0.0f, 0, 0}};
        axm::ModelVertex l2[4] = {{ax - rx * wdt, ay, az - rz * wdt, 0.0f, 0, 0}, {ax + rx * wdt, ay, az + rz * wdt, 0.0f, 0, 0},
                                  {bx + rx * wdt, by, bz + rz * wdt, 0.0f, 0, 0}, {bx - rx * wdt, by, bz - rz * wdt, 0.0f, 0, 0}};
        for (int k : T) q.push_back(l[k]);
        for (int k : T) q.push_back(l2[k]);
    }
    float black[4] = {0, 0, 0, 1};
    r.drawTriangles(q, nullptr, black, 0, false, true);
}
