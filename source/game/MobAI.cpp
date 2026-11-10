// Mob behaviour: Entity / EntityLiving / EntityCreature / EntityMobs / EntityAnimals and the
// mobs' own onLivingUpdate / attackEntity, plus SpawnerAnimals, Pathfinder, EntityArrow (from
// skeletons) and Explosion (creepers). Everything here reads the world with the "peek" calls,
// so a mob never makes a chunk load or generate.
#include "Mobs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include "Player.h"
#include "Inventory.h"
#include "ItemEntities.h"
#include "audio/Sound.h"
#include "render/Particles.h"
#include "world/BlockProps.h"
#include "world/BlockResistance.h"
#include "world/Blocks.h"
#include "world/MathHelper.h"
#include "world/World.h"
#include "game/Options.h"
#include "world/Redstone.h"
#include "world/gen/ChunkGenerator.h"

namespace {

inline int fl(double v) { return int(std::floor(v)); }
constexpr float PI_F = 3.1415927f;

enum Type : uint8_t { ANIMAL, MONSTER, WATER, OTHER };
struct Stats {
    float w, h, speed;
    int health, attack, drop;
    const char *living, *hurt, *death;
    float volume;
    int soundInterval; // func_421_b
    Type type;
    bool burns;        // zombies and skeletons catch fire in daylight
};
// Same order as Mobs::Kind
const Stats STATS[] = {
    {0.9f, 0.9f, 0.7f, 10, 0, 319, "mob.pig", "mob.pig", "mob.pigdeath", 1.0f, 120, ANIMAL, false},
    {0.9f, 1.3f, 0.7f, 10, 0, 0, "mob.sheep", "mob.sheep", "mob.sheep", 1.0f, 120, ANIMAL, false},
    {0.9f, 1.3f, 0.7f, 10, 0, 334, "mob.cow", "mob.cowhurt", "mob.cowhurt", 0.4f, 120, ANIMAL, false},
    {0.3f, 0.4f, 0.7f, 4, 0, 288, "mob.chicken", "mob.chickenhurt", "mob.chickenhurt", 1.0f, 120, ANIMAL, false},
    {0.6f, 1.8f, 0.7f, 20, 0, 289, nullptr, "mob.creeper", "mob.creeperdeath", 1.0f, 80, MONSTER, false},
    {0.6f, 1.8f, 0.7f, 20, 0, 262, "mob.skeleton", "mob.skeletonhurt", "mob.skeletonhurt", 1.0f, 80, MONSTER, true},
    {0.6f, 1.8f, 0.5f, 20, 5, 288, "mob.zombie", "mob.zombiehurt", "mob.zombiedeath", 1.0f, 80, MONSTER, true},
    {1.4f, 0.9f, 0.8f, 20, 2, 287, "mob.spider", "mob.spider", "mob.spiderdeath", 1.0f, 80, MONSTER, false},
    {0.6f, 0.6f, 0.7f, 1, 0, 341, nullptr, "mob.slime", "mob.slime", 1.0f, 80, MONSTER, false},
    {3.6f, 10.8f, 0.5f, 100, 50, 0, "mob.zombie", "mob.zombiehurt", "mob.zombiedeath", 1.0f, 80, OTHER, false},
};
const char* const SAVE_IDS[] = {"Pig", "Sheep", "Cow", "Chicken", "Creeper", "Skeleton", "Zombie", "Spider", "Slime", "Giant"};

// ---------------------------------------------------------------- Pathfinder
struct PPoint {
    int x, y, z, hash;
    int index = -1;
    float total = 0, toNext = 0, toTarget = 0;
    int previous = -1;
    bool isFirst = false;
    float dist(const PPoint& o) const {
        float a = float(o.x - x), b = float(o.y - y), c = float(o.z - z);
        return MathHelper::sqrt_float(a * a + b * b + c * c);
    }
};
struct Pathfinder {
    const World& w;
    std::vector<PPoint> pts;
    std::unordered_map<int, int> map;
    std::vector<int> heap;
    explicit Pathfinder(const World& world) : w(world) {}

    int open(int x, int y, int z) {
        int h = x | y << 10 | z << 20;
        auto it = map.find(h);
        if (it != map.end()) return it->second;
        pts.push_back(PPoint{x, y, z, h});
        map[h] = int(pts.size()) - 1;
        return int(pts.size()) - 1;
    }
    // Path.sortBack / sortForward / addPoint / dequeue / changeDistance
    void sortBack(int i) {
        int p = heap[i];
        float d = pts[p].toTarget;
        while (i > 0) {
            int up = (i - 1) >> 1;
            int q = heap[up];
            if (d >= pts[q].toTarget) break;
            heap[i] = q;
            pts[q].index = i;
            i = up;
        }
        heap[i] = p;
        pts[p].index = i;
    }
    void sortForward(int i) {
        int p = heap[i];
        float d = pts[p].toTarget;
        int n = int(heap.size());
        while (true) {
            int a = 1 + (i << 1), b = a + 1;
            if (a >= n) break;
            float da = pts[heap[a]].toTarget, db = b >= n ? INFINITY : pts[heap[b]].toTarget;
            if (da < db) {
                if (da >= d) break;
                heap[i] = heap[a];
                pts[heap[i]].index = i;
                i = a;
            } else {
                if (db >= d) break;
                heap[i] = heap[b];
                pts[heap[i]].index = i;
                i = b;
            }
        }
        heap[i] = p;
        pts[p].index = i;
    }
    void add(int p) {
        heap.push_back(p);
        pts[p].index = int(heap.size()) - 1;
        sortBack(int(heap.size()) - 1);
    }
    int dequeue() {
        int p = heap[0];
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) sortForward(0);
        pts[p].index = -1;
        return p;
    }
    void change(int p, float d) {
        float old = pts[p].toTarget;
        pts[p].toTarget = d;
        if (d < old) sortBack(pts[p].index);
        else sortForward(pts[p].index);
    }
    // getVerticalOffset (with Beta's bug: it tests the corner block for the whole box)
    int vertical(int x, int y, int z, int sx, int sy, int sz) const {
        for (int a = 0; a < sx; ++a)
            for (int b = 0; b < sy; ++b)
                for (int c = 0; c < sz; ++c) {
                    Mat m = BP[y >= 0 && y < 256 ? w.getBlockId(x, y, z) : 0].mat;
                    if (matSolid(m)) return 0;
                    if (m == Mat::water || m == Mat::lava) return -1;
                }
        return 1;
    }
    int safe(int x, int y, int z, int sx, int sy, int sz, int up) {
        int p = -1;
        if (vertical(x, y, z, sx, sy, sz) > 0) p = open(x, y, z);
        if (p < 0 && vertical(x, y + up, z, sx, sy, sz) > 0) {
            p = open(x, y + up, z);
            y += up;
        }
        if (p >= 0) {
            int drop = 0;
            for (; y > 0; --y) {
                int v = vertical(x, y - 1, z, sx, sy, sz);
                if (v <= 0) break;
                if (++drop >= 4) return -1;
            }
            if (y > 0) p = open(x, y, z);
        }
        return p;
    }
    bool find(double ex0, double ey0, double ez0, float width, float height, double tx, double ty, double tz,
              float range, std::vector<int>& out) {
        pts.clear();
        pts.reserve(512);
        map.clear();
        heap.clear();
        int start = open(fl(ex0), fl(ey0), fl(ez0));
        int end = open(fl(tx - double(width / 2.0f)), fl(ty), fl(tz - double(width / 2.0f)));
        int sx = MathHelper::floor_float(width + 1.0f), sy = MathHelper::floor_float(height + 1.0f), sz = sx;
        pts[start].total = 0;
        pts[start].toNext = pts[start].dist(pts[end]);
        pts[start].toTarget = pts[start].toNext;
        add(start);
        int best = start;
        int guard = 0;
        while (!heap.empty() && ++guard < 2000) {
            int cur = dequeue();
            if (pts[cur].hash == pts[end].hash) { best = cur; break; }
            if (pts[cur].dist(pts[end]) < pts[best].dist(pts[end])) best = cur;
            pts[cur].isFirst = true;
            int up = vertical(pts[cur].x, pts[cur].y + 1, pts[cur].z, sx, sy, sz) > 0 ? 1 : 0;
            int cx = pts[cur].x, cy = pts[cur].y, cz = pts[cur].z;
            int opts[4] = {safe(cx, cy, cz + 1, sx, sy, sz, up), safe(cx - 1, cy, cz, sx, sy, sz, up),
                           safe(cx + 1, cy, cz, sx, sy, sz, up), safe(cx, cy, cz - 1, sx, sy, sz, up)};
            for (int o : opts) {
                if (o < 0 || pts[o].isFirst || pts[o].dist(pts[end]) >= range) continue;
                float t = pts[cur].total + pts[cur].dist(pts[o]);
                if (pts[o].index < 0 || t < pts[o].total) {
                    pts[o].previous = cur;
                    pts[o].total = t;
                    pts[o].toNext = pts[o].dist(pts[end]);
                    if (pts[o].index >= 0) {
                        change(o, pts[o].total + pts[o].toNext);
                    } else {
                        pts[o].toTarget = pts[o].total + pts[o].toNext;
                        add(o);
                    }
                }
            }
        }
        if (best == start) return false;
        out.clear();
        for (int p = best; p >= 0; p = pts[p].previous) out.push_back(p);
        std::reverse(out.begin(), out.end());
        return true;
    }
};

} // namespace

// ================================================================ setup and physics

void Mobs::setSize(Mob& m, float w, float h) {
    m.width = w;
    m.height = h;
}

void Mobs::setPosition(Mob& m, double x, double y, double z) {
    m.x = x;
    m.y = y;
    m.z = z;
    float hw = m.width / 2.0f;
    m.box = {x - hw, y + m.ySize, z - hw, x + hw, y + m.ySize + m.height, z + hw};
}

void Mobs::initMob(Mob& m) {
    const Stats& s = STATS[m.kind];
    float scale = m.kind == SLIME ? float(m.slimeSize) : 1.0f;
    setSize(m, s.w * scale, s.h * scale);
    m.moveSpeed = s.speed;
    setPosition(m, m.x, m.y, m.z);
    m.soundTimer = -s.soundInterval;
    m.eggTimer = m.kind == SHEEP ? 0 : int(jr.nextInt(6000)) + 6000; // (EntitySheep: the wool timer starts at 0)
}

bool Mobs::waterMovement(const World& w, Mob& m) {
    return Physics::waterAcceleration(w, m.box.expand(0.0, double(-0.4f), 0.0), m.mx, m.my, m.mz);
}

bool Mobs::lavaMovement(const World& w, const Mob& m) const {
    return Physics::isMaterialInBB(w, m.box.expand(double(-0.1f), double(-0.4f), double(-0.1f)), true);
}

void Mobs::playSound(const Mob& m, const char* name, float volume) {
    if (!name) return;
    sound::play(name, m.x, m.y, m.z, volume, (jr.nextFloat() - jr.nextFloat()) * 0.2f + 1.0f);
}

// Entity.moveEntity (no sneaking for mobs)
void Mobs::moveEntity(const World& w, Mob& m, double dx, double dy, double dz) {
    double x0 = m.x, z0 = m.z;
    double odx = dx, ody = dy, odz = dz;
    AABB start = m.box;
    Physics::collidingBoxes(w, m.box.addCoord(dx, dy, dz), boxes);
    for (const AABB& b : boxes) dy = b.calculateYOffset(m.box, dy);
    m.box.move(0.0, dy, 0.0);
    bool stepOk = m.onGround || (ody != dy && ody < 0.0);
    for (const AABB& b : boxes) dx = b.calculateXOffset(m.box, dx);
    m.box.move(dx, 0.0, 0.0);
    for (const AABB& b : boxes) dz = b.calculateZOffset(m.box, dz);
    m.box.move(0.0, 0.0, dz);
    const float STEP = 0.5f;
    if (stepOk && m.ySize < 0.05f && (odx != dx || odz != dz)) {
        double sx = dx, sy = dy, sz = dz;
        dx = odx;
        dy = double(STEP);
        dz = odz;
        AABB moved = m.box;
        m.box = start;
        Physics::collidingBoxes(w, m.box.addCoord(odx, dy, odz), boxes);
        for (const AABB& b : boxes) dy = b.calculateYOffset(m.box, dy);
        m.box.move(0.0, dy, 0.0);
        for (const AABB& b : boxes) dx = b.calculateXOffset(m.box, dx);
        m.box.move(dx, 0.0, 0.0);
        for (const AABB& b : boxes) dz = b.calculateZOffset(m.box, dz);
        m.box.move(0.0, 0.0, dz);
        if (sx * sx + sz * sz >= dx * dx + dz * dz) {
            dx = sx;
            dy = sy;
            dz = sz;
            m.box = moved;
        } else {
            m.ySize = float(double(m.ySize) + 0.5);
        }
    }
    m.x = (m.box.minX + m.box.maxX) / 2.0;
    m.y = m.box.minY - double(m.ySize);
    m.z = (m.box.minZ + m.box.maxZ) / 2.0;
    m.collidedH = odx != dx || odz != dz;
    m.collidedV = ody != dy;
    m.onGround = ody != dy && ody < 0.0;
    // updateFallState + EntityLiving.fall
    if (m.onGround) {
        if (m.fallDistance > 0.0f) {
            float fd = m.fallDistance;
            int dmg = int(std::ceil(double(fd - 3.0f)));
            if (dmg > 0 && m.kind != CHICKEN) hurt(m, dmg, false, 0, 0, nullptr, &w);
            m.fallDistance = 0.0f;
        }
    } else if (dy < 0.0) {
        m.fallDistance = float(double(m.fallDistance) - dy);
    }
    if (odx != dx) m.mx = 0.0;
    if (ody != dy) m.my = 0.0;
    if (odz != dz) m.mz = 0.0;
    (void)x0;
    (void)z0;
    // onEntityCollidedWithBlock: soul sand, cactus
    int bx0 = fl(m.box.minX), by0 = fl(m.box.minY), bz0 = fl(m.box.minZ);
    int bx1 = fl(m.box.maxX), by1 = fl(m.box.maxY), bz1 = fl(m.box.maxZ);
    if (by0 >= 0 && by1 < 256)
        for (int x = bx0; x <= bx1; ++x)
            for (int y = by0; y <= by1; ++y)
                for (int z = bz0; z <= bz1; ++z) {
                    int id = w.getBlockId(x, y, z);
                    if (id == 88) { m.mx *= 0.4; m.mz *= 0.4; }
                    else if (id == 81) hurt(m, 1, false, 0, 0, nullptr, &w);
                }
    m.ySize *= 0.4f;
    bool wet = waterMovement(w, m);
    bool burning = false; // World.isBoundingBoxBurning
    for (int x = fl(m.box.minX); x < fl(m.box.maxX + 1.0) && !burning; ++x)
        for (int y = fl(m.box.minY); y < fl(m.box.maxY + 1.0) && !burning; ++y)
            for (int z = fl(m.box.minZ); z < fl(m.box.maxZ + 1.0); ++z) {
                int id = (y >= 0 && y < 256) ? w.getBlockId(x, y, z) : 0;
                if (id == 51 || id == 10 || id == 11) { burning = true; break; }
            }
    if (burning) {
        hurt(m, 1, false, 0, 0, nullptr, &w);
        if (!wet) {
            ++m.fire;
            if (m.fire == 0) m.fire = 300;
        }
    } else if (m.fire <= 0) {
        m.fire = -1; // fireResistance 1
    }
    if (wet && m.fire > 0) m.fire = -1;
}

// EntityLiving.moveEntityWithHeading
void Mobs::moveWithHeading(const World& w, Mob& m, float s, float f) {
    auto moveFlying = [&](float a, float b, float accel) {
        float l = MathHelper::sqrt_float(a * a + b * b);
        if (l >= 0.01f) {
            if (l < 1.0f) l = 1.0f;
            l = accel / l;
            a *= l;
            b *= l;
            float sn = MathHelper::sin(m.yaw * PI_F / 180.0f), cs = MathHelper::cos(m.yaw * PI_F / 180.0f);
            m.mx += double(a * cs - b * sn);
            m.mz += double(b * cs + a * sn);
        }
    };
    auto offsetInLiquid = [&](double ox, double oy, double oz) {
        AABB b = m.box.offset(ox, oy, oz);
        Physics::collidingBoxes(w, b, boxes);
        return boxes.empty() ? !Physics::isAnyLiquid(w, b) : false;
    };
    bool ladder = m.kind == SPIDER ? m.collidedH : false;
    if (m.kind != SPIDER) {
        int lx = fl(m.x), ly = fl(m.box.minY), lz = fl(m.z);
        ladder = w.getBlockId(lx, ly, lz) == 65 || w.getBlockId(lx, ly + 1, lz) == 65;
    }
    if (waterMovement(w, m)) {
        double y0 = m.y;
        moveFlying(s, f, 0.02f);
        moveEntity(w, m, m.mx, m.my, m.mz);
        m.mx *= double(0.8f);
        m.my *= double(0.8f);
        m.mz *= double(0.8f);
        m.my -= 0.02;
        if (m.collidedH && offsetInLiquid(m.mx, m.my + double(0.6f) - m.y + y0, m.mz)) m.my = double(0.3f);
    } else if (lavaMovement(w, m)) {
        double y0 = m.y;
        moveFlying(s, f, 0.02f);
        moveEntity(w, m, m.mx, m.my, m.mz);
        m.mx *= 0.5;
        m.my *= 0.5;
        m.mz *= 0.5;
        m.my -= 0.02;
        if (m.collidedH && offsetInLiquid(m.mx, m.my + double(0.6f) - m.y + y0, m.mz)) m.my = double(0.3f);
    } else {
        auto friction = [&]() {
            float fr = 0.91f;
            if (m.onGround) {
                fr = 546.0f * 0.1f * 0.1f * 0.1f;
                int id = w.getBlockId(fl(m.x), fl(m.box.minY) - 1, fl(m.z));
                if (id > 0) fr = Physics::slipperiness(id) * 0.91f;
            }
            return fr;
        };
        float fr = friction();
        float k = 0.16277136f / (fr * fr * fr);
        moveFlying(s, f, m.onGround ? 0.1f * k : 0.02f);
        fr = friction();
        if (ladder) {
            m.fallDistance = 0.0f;
            if (m.my < -0.15) m.my = -0.15;
        }
        moveEntity(w, m, m.mx, m.my, m.mz);
        if (m.collidedH && ladder) m.my = 0.2;
        m.my -= 0.08;
        m.my *= double(0.98f);
        m.mx *= double(fr);
        m.mz *= double(fr);
    }
    // limb swing
    m.prevLimbAmount = m.limbAmount;
    double ddx = m.x - m.px, ddz = m.z - m.pz;
    float sp = MathHelper::sqrt_double(ddx * ddx + ddz * ddz) * 4.0f;
    if (sp > 1.0f) sp = 1.0f;
    m.limbAmount += (sp - m.limbAmount) * 0.4f;
    m.limbSwing += m.limbAmount;
}

// ================================================================ damage

// EntityLiving.attackEntityFrom (+ EntityMobs: a hurt monster goes after the player)
bool Mobs::hurt(Mob& m, int dmg, bool fromPlayer, double fx, double fz, ItemEntities* items, const World* w) {
    if (m.health <= 0 || m.dead) return false;
    m.age = 0;
    bool fresh = true;
    if (float(m.hurtResist) > 20.0f / 2.0f) {
        if (dmg <= m.lastDamage) return false;
        m.health -= dmg - m.lastDamage;
        m.lastDamage = dmg;
        fresh = false;
    } else {
        m.lastDamage = dmg;
        m.prevHealth = m.health;
        m.hurtResist = 20;
        m.health -= dmg;
        m.hurtTime = m.maxHurtTime = 10;
    }
    m.attackedAtYaw = 0.0f;
    if (fresh) {
        if (fromPlayer) {
            double dx = fx - m.x, dz = fz - m.z;
            while (dx * dx + dz * dz < 1.0e-4) {
                dx = (jr.nextDouble() - jr.nextDouble()) * 0.01;
                dz = (jr.nextDouble() - jr.nextDouble()) * 0.01;
            }
            m.attackedAtYaw = float(std::atan2(dz, dx) * 180.0 / double(PI_F)) - m.yaw;
            float len = MathHelper::sqrt_double(dx * dx + dz * dz);
            m.mx /= 2.0;
            m.my /= 2.0;
            m.mz /= 2.0;
            m.mx -= dx / double(len) * double(0.4f);
            m.my += double(0.4f);
            m.mz -= dz / double(len) * double(0.4f);
            if (m.my > double(0.4f)) m.my = double(0.4f);
        } else {
            m.attackedAtYaw = float(int(jr.nextDouble() * 2.0) * 180);
        }
    }
    const Stats& s = STATS[m.kind];
    if (fromPlayer && s.type == MONSTER) m.target = true; // EntityMobs: a hurt monster goes after you
    if (m.health <= 0) {
        if (fresh) playSound(m, s.death, s.volume);
        if (items) die(m, items, w);
    } else if (fresh) {
        playSound(m, s.hurt, s.volume);
    }
    // EntitySheep.attackEntityFrom: hitting an unsheared sheep shears it
    if (m.kind == SHEEP && fromPlayer && !m.sheared && items) {
        m.sheared = true;
        int n = 1 + jr.nextInt(3);
        for (int i = 0; i < n; ++i) {
            ItemStack st;
            st.id = 35;
            st.count = 1;
            items->spawnMoving(m.x, m.y + 1.0, m.z, st, double((jr.nextFloat() - jr.nextFloat()) * 0.1f),
                               0.2 + double(jr.nextFloat() * 0.05f), double((jr.nextFloat() - jr.nextFloat()) * 0.1f), 10);
            m.eggTimer = jr.nextInt(1200); // the wool grows back
        }
    }
    return true;
}

// EntityLiving.onDeath -> func_21066_o: what it drops
void Mobs::die(Mob& m, ItemEntities* items, const World*) {
    const Stats& s = STATS[m.kind];
    auto drop = [&](int id, int count, int damage = 0) {
        for (int i = 0; i < count; ++i) {
            ItemStack st;
            st.id = uint16_t(id);
            st.count = 1;
            st.damage = int16_t(damage);
            items->spawn(m.x, m.y, m.z, st, 10);
        }
    };
    if (!items) return;
    if (m.kind == SKELETON) {
        drop(262, jr.nextInt(3));
        drop(352, jr.nextInt(3));
    } else if (m.kind == SLIME) {
        if (m.slimeSize == 1) drop(341, jr.nextInt(3));
    } else if (s.drop > 0) {
        drop(s.drop, jr.nextInt(3));
    }
}

// ================================================================ AI

bool Mobs::canSee(const World& w, const Mob& m, const Player& p) const {
    float ox = float(m.x), oy = float(m.y + m.height * 0.85f), oz = float(m.z);
    float dx = float(p.posX) - ox, dy = float(p.posY) - oy, dz = float(p.posZ) - oz;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 0.01f) return true;
    RayHit h = w.rayTrace(ox, oy, oz, dx / len, dy / len, dz / len, len);
    return !h.hit;
}

void Mobs::faceTowards(Mob& m, double tx, double ty, double tz, float maxTurn) {
    double dx = tx - m.x, dz = tz - m.z, dy = ty - (m.y + double(m.height * 0.85f));
    double h = double(MathHelper::sqrt_double(dx * dx + dz * dz));
    float yaw = float(std::atan2(dz, dx) * 180.0 / double(PI_F)) - 90.0f;
    float pitch = float(std::atan2(dy, h) * 180.0 / double(PI_F));
    auto turn = [](float cur, float want, float max) {
        float d = want - cur;
        while (d < -180.0f) d += 360.0f;
        while (d >= 180.0f) d -= 360.0f;
        if (d > max) d = max;
        if (d < -max) d = -max;
        return cur + d;
    };
    m.pitch = -turn(m.pitch, pitch, maxTurn);
    m.yaw = turn(m.yaw, yaw, maxTurn);
}

float Mobs::pathWeight(const World& w, const Mob& m, int x, int y, int z) const {
    const Stats& s = STATS[m.kind];
    if (s.type == MONSTER || m.kind == GIANT) return 0.5f - w.getBrightness(x, y, z); // EntityMobs
    if (s.type == ANIMAL) return w.getBlockId(x, y - 1, z) == 2 ? 10.0f : w.getBrightness(x, y, z) - 0.5f;
    return 0.0f;
}

bool Mobs::findPath(const World& w, Mob& m, double tx, double ty, double tz, float range) {
    Pathfinder pf(w);
    std::vector<int> idx;
    bool ok = pf.find(m.box.minX, m.box.minY, m.box.minZ, m.width, m.height, tx, ty, tz, range, idx);
    m.path.clear();
    m.pathIndex = 0;
    if (!ok) return false;
    for (int i : idx) m.path.push_back({pf.pts[i].x, pf.pts[i].y, pf.pts[i].z});
    return true;
}

// EntityLiving.updatePlayerActionState: despawning, looking around
void Mobs::actionState(World& w, Mob& m, Player& p) {
    ++m.age;
    double dx = p.posX - m.x, dy = p.posY - m.y, dz = p.posZ - m.z;
    double d2 = dx * dx + dy * dy + dz * dz;
    bool canDespawn = m.kind != GIANT;
    if (canDespawn && d2 > 16384.0) m.dead = true;
    if (canDespawn && m.age > 600 && jr.nextInt(800) == 0) {
        if (d2 < 1024.0) m.age = 0;
        else m.dead = true;
    }
    m.moveStrafing = 0.0f;
    m.moveForward = 0.0f;
    if (jr.nextFloat() < 0.02f) {
        if (d2 < 64.0) {
            m.lookAtPlayer = true;
            m.lookTicks = 10 + jr.nextInt(20);
        } else {
            m.randomYawVelocity = (jr.nextFloat() - 0.5f) * 20.0f;
        }
    }
    if (m.lookAtPlayer) {
        faceTowards(m, p.posX, p.posY, p.posZ, 10.0f);
        if (m.lookTicks-- <= 0 || d2 > 64.0) m.lookAtPlayer = false;
    } else {
        if (jr.nextFloat() < 0.05f) m.randomYawVelocity = (jr.nextFloat() - 0.5f) * 20.0f;
        m.yaw += m.randomYawVelocity;
        m.pitch = 0.0f;
    }
    if (waterMovement(w, m) || lavaMovement(w, m)) m.jumping = jr.nextFloat() < 0.8f;
}

// EntityCreature.updatePlayerActionState (+ EntityMobs / the mobs' attackEntity)
void Mobs::creatureAI(World& w, Mob& m, Player& p) {
    m.hasAttacked = false;
    const Stats& s = STATS[m.kind];
    bool hostile = s.type == MONSTER;
    float bright = w.getBrightness(fl(m.x), fl(m.y + m.height * 0.66f), fl(m.z));
    double dx = p.posX - m.x, dz = p.posZ - m.z;
    double eyeDy = p.posY - (m.y + double(m.height * 0.85f));
    float dist = MathHelper::sqrt_double(dx * dx + eyeDy * eyeDy + dz * dz);
    bool playerOk = !p.dead() && !p.invulnerable;
    if (!m.target) {
        // findPlayerToAttack
        if (hostile && playerOk && dist < 16.0f) {
            if (m.kind == SPIDER) m.target = bright < 0.5f;
            else m.target = canSee(w, m, p);
        }
        if (m.target) findPath(w, m, p.posX, p.boundingBox.minY, p.posZ, 16.0f);
    } else if (!playerOk) {
        m.target = false;
    } else if (canSee(w, m, p)) {
        // attackEntity
        if (m.kind == CREEPER) {
            if ((m.fuse <= 0 && dist < 3.0f) || (m.fuse > 0 && dist < 7.0f)) {
                if (m.fuse == 0) sound::play("random.fuse", m.x, m.y, m.z, 1.0f, 0.5f);
                ++m.fuse;
                m.hasAttacked = true;
            } else {
                if (--m.fuse < 0) m.fuse = 0;
            }
        } else if (m.kind == SKELETON) {
            if (dist < 10.0f) {
                double ax = p.posX - m.x, az = p.posZ - m.z;
                if (m.attackTime == 0) { // EntityArrow(world, shooter) + setArrowHeading
                    Arrow a{};
                    float yaw = m.yaw, pitch = m.pitch;
                    a.x = m.x - double(MathHelper::cos(yaw / 180.0f * PI_F) * 0.16f);
                    a.y = m.y - 0.1 + double(1.4f); // EntityArrow(world, shooter), then posY += 1.4F
                    a.z = m.z - double(MathHelper::sin(yaw / 180.0f * PI_F) * 0.16f);
                    (void)pitch;
                    double vy = p.posY - double(0.2f) - a.y;
                    float spread = MathHelper::sqrt_double(ax * ax + az * az) * 0.2f;
                    double hx = ax, hy = vy + double(spread), hz = az;
                    float len = MathHelper::sqrt_double(hx * hx + hy * hy + hz * hz);
                    hx /= double(len);
                    hy /= double(len);
                    hz /= double(len);
                    hx += jr.nextGaussian() * 0.0075 * 12.0;
                    hy += jr.nextGaussian() * 0.0075 * 12.0;
                    hz += jr.nextGaussian() * 0.0075 * 12.0;
                    a.mx = hx * 0.6;
                    a.my = hy * 0.6;
                    a.mz = hz * 0.6;
                    float h = MathHelper::sqrt_double(a.mx * a.mx + a.mz * a.mz);
                    a.yaw = float(std::atan2(a.mx, a.mz) * 180.0 / double(PI_F));
                    a.pitch = float(std::atan2(a.my, double(h)) * 180.0 / double(PI_F));
                    a.px = a.x;
                    a.py = a.y;
                    a.pz = a.z;
                    arrows.push_back(a);
                    sound::play("random.bow", m.x, m.y, m.z, 1.0f, 1.0f / (jr.nextFloat() * 0.4f + 0.8f));
                    m.attackTime = 30;
                }
                m.yaw = float(std::atan2(az, ax) * 180.0 / double(PI_F)) - 90.0f;
                m.hasAttacked = true;
            }
        } else if (m.kind == SPIDER && bright > 0.5f && jr.nextInt(100) == 0) {
            m.target = false;
        } else if (m.kind == SPIDER && dist > 2.0f && dist < 6.0f && jr.nextInt(10) == 0) {
            if (m.onGround) { // the leap
                float d = MathHelper::sqrt_double(dx * dx + dz * dz);
                m.mx = dx / double(d) * 0.5 * double(0.8f) + m.mx * double(0.2f);
                m.mz = dz / double(d) * 0.5 * double(0.8f) + m.mz * double(0.2f);
                m.my = double(0.4f);
            }
        } else if (s.attack > 0 && dist < 2.5f && p.boundingBox.maxY > m.box.minY && p.boundingBox.minY < m.box.maxY) {
            m.attackTime = 20;
            bool monster = s.type == MONSTER || m.kind == GIANT; // EntityMobs
            p.attack(s.attack, !monster ? Player::OTHER : (m.kind == SPIDER || m.kind == ZOMBIE || m.kind == SKELETON) ? Player::MOB_ALWAYS : Player::MOB, m.x, m.z);
        }
    }

    if (m.hasAttacked || !m.target || (!m.path.empty() && jr.nextInt(20) != 0)) {
        if ((m.path.empty() && jr.nextInt(80) == 0) || jr.nextInt(80) == 0) {
            // wander: the best-liked of 10 random spots
            bool found = false;
            int bx = -1, by = -1, bz = -1;
            float best = -99999.0f;
            for (int i = 0; i < 10; ++i) {
                int a = fl(m.x + double(jr.nextInt(13)) - 6.0);
                int b = fl(m.y + double(jr.nextInt(7)) - 3.0);
                int c = fl(m.z + double(jr.nextInt(13)) - 6.0);
                float wgt = pathWeight(w, m, a, b, c);
                if (wgt > best) {
                    best = wgt;
                    bx = a;
                    by = b;
                    bz = c;
                    found = true;
                }
            }
            if (found) findPath(w, m, double(float(bx) + 0.5f), double(float(by) + 0.5f), double(float(bz) + 0.5f), 10.0f);
        }
    } else {
        findPath(w, m, p.posX, p.boundingBox.minY, p.posZ, 16.0f);
    }

    int feet = fl(m.box.minY + 0.5);
    bool water = waterMovement(w, m), lava = lavaMovement(w, m);
    m.pitch = 0.0f;
    if (!m.path.empty() && jr.nextInt(100) != 0) {
        int sz = int(m.width + 1.0f);
        auto pos = [&](double& px, double& py, double& pz) {
            const PathPoint& pp = m.path[m.pathIndex];
            px = double(pp.x) + double(sz) * 0.5;
            py = double(pp.y);
            pz = double(pp.z) + double(sz) * 0.5;
        };
        double tx, ty, tz;
        bool have = true;
        pos(tx, ty, tz);
        double reach = double(m.width * 2.0f);
        while (have && (tx - m.x) * (tx - m.x) + (tz - m.z) * (tz - m.z) < reach * reach) {
            ++m.pathIndex;
            if (m.pathIndex >= m.path.size()) {
                have = false;
                m.path.clear();
            } else {
                pos(tx, ty, tz);
            }
        }
        m.jumping = false;
        if (have) {
            double ddx = tx - m.x, ddz = tz - m.z, ddy = ty - double(feet);
            float want = float(std::atan2(ddz, ddx) * 180.0 / double(PI_F)) - 90.0f;
            float turn = want - m.yaw;
            m.moveForward = m.moveSpeed;
            while (turn < -180.0f) turn += 360.0f;
            while (turn >= 180.0f) turn -= 360.0f;
            if (turn > 30.0f) turn = 30.0f;
            if (turn < -30.0f) turn = -30.0f;
            m.yaw += turn;
            if (m.hasAttacked && m.target) {
                double ax = p.posX - m.x, az = p.posZ - m.z;
                float before = m.yaw;
                m.yaw = float(std::atan2(az, ax) * 180.0 / double(PI_F)) - 90.0f;
                float t = (before - m.yaw + 90.0f) * PI_F / 180.0f;
                m.moveStrafing = -MathHelper::sin(t) * m.moveForward * 1.0f;
                m.moveForward = MathHelper::cos(t) * m.moveForward * 1.0f;
            }
            if (ddy > 0.0 && ddx * ddx + ddz * ddz < double(std::max(1.0f, m.width * m.width)) * 6.25) m.jumping = true;
        }
        if (m.target) faceTowards(m, p.posX, p.posY, p.posZ, 30.0f);
        if (m.collidedH) m.jumping = true;
        if (jr.nextFloat() < 0.8f && (water || lava)) m.jumping = true;
    } else {
        actionState(w, m, p);
        m.path.clear();
    }
}

// ================================================================ the tick

void Mobs::livingUpdate(World& w, Mob& m, Player& p, ItemEntities& items, bool editWorld) {
    const Stats& s = STATS[m.kind];
    // Entity.onEntityUpdate
    ++m.ticksExisted;
    m.px = m.x;
    m.py = m.y;
    m.pz = m.z;
    m.prevYaw = m.yaw;
    m.prevPitch = m.pitch;
    m.prevRenderYaw = m.renderYaw;
    if (waterMovement(w, m)) {
        m.fallDistance = 0.0f;
        m.inWater = true;
        m.fire = 0;
    } else {
        m.inWater = false;
    }
    if (m.fire > 0) {
        if (m.fire % 20 == 0) hurt(m, 1, false, 0, 0, &items, &w);
        --m.fire;
    }
    if (lavaMovement(w, m)) {
        hurt(m, 4, false, 0, 0, &items, &w);
        m.fire = 600;
    }
    if (m.y < -64.0) hurt(m, 4, false, 0, 0, &items, &w);
    // EntityLiving.onEntityUpdate
    if (jr.nextInt(1000) < m.soundTimer++) {
        m.soundTimer = -s.soundInterval;
        if (s.living) playSound(m, s.living, s.volume);
    }
    if (m.health > 0) { // suffocating
        int ex = fl(m.x), ey = fl(m.y + double(m.height * 0.85f)), ez = fl(m.z);
        if (ey >= 0 && ey < 256 && BP[w.getBlockId(ex, ey, ez)].opaqueCube) hurt(m, 1, false, 0, 0, &items, &w);
    }
    if (m.health > 0) { // drowning
        int ex = fl(m.x), ey = fl(m.y + double(m.height * 0.85f)), ez = fl(m.z);
        int id = (ey >= 0 && ey < 256) ? w.getBlockId(ex, ey, ez) : 0;
        bool under = false;
        if (id == 8 || id == 9) {
            int meta = w.getBlockMetadata(ex, ey, ez);
            if (meta >= 8) meta = 0;
            float hgt = float(meta + 1) / 9.0f - 1.0f / 9.0f;
            under = m.y + double(m.height * 0.85f) < double(float(ey + 1) - hgt);
        }
        if (under) {
            if (--m.air == -20) {
                m.air = 0;
                hurt(m, 2, false, 0, 0, &items, &w);
            }
            m.fire = 0;
        } else {
            m.air = 300;
        }
    }
    if (m.attackTime > 0) --m.attackTime;
    if (m.hurtTime > 0) --m.hurtTime;
    if (m.hurtResist > 0) --m.hurtResist;
    if (m.health <= 0) {
        if (++m.deathTime > 20) {
            m.dead = true;
            if (gParticles) // the puff it leaves
                for (int i = 0; i < 20; ++i)
                    gParticles->spawn(Particles::EXPLODE, m.x + double(jr.nextFloat() * m.width * 2.0f) - double(m.width),
                                      m.y + double(jr.nextFloat() * m.height), m.z + double(jr.nextFloat() * m.width * 2.0f) - double(m.width),
                                      jr.nextGaussian() * 0.02, jr.nextGaussian() * 0.02, jr.nextGaussian() * 0.02);
        }
    }

    // the mob's own onLivingUpdate first
    if (s.burns && w.skylightSubtracted < 4) {
        float b = w.getBrightness(fl(m.x), fl(m.y + m.height * 0.66f), fl(m.z));
        const Chunk* c = w.getChunk(fl(m.x) >> 4, fl(m.z) >> 4);
        bool sky = c && fl(m.y) >= c->heightMap[(fl(m.z) & 15) << 4 | (fl(m.x) & 15)];
        if (b > 0.5f && sky && jr.nextFloat() * 30.0f < (b - 0.4f) * 2.0f) m.fire = 300;
    }
    if (s.type == MONSTER) { // EntityMobs: lit monsters age (and despawn) faster
        float b = w.getBrightness(fl(m.x), fl(m.y + m.height * 0.66f), fl(m.z));
        if (b > 0.5f) m.age += 2;
    }

    // EntityLiving.onLivingUpdate
    bool still = m.kind == GIANT;
    if (m.health <= 0) {
        m.jumping = false;
        m.moveStrafing = m.moveForward = m.randomYawVelocity = 0.0f;
    } else if (still) {
        m.moveStrafing = m.moveForward = 0.0f;
        m.jumping = false;
    } else if (m.kind == SLIME) {
        // EntitySlime.updatePlayerActionState: hops, toward the player when close
        actionState(w, m, p);
        double dx = p.posX - m.x, dz = p.posZ - m.z;
        double d2 = dx * dx + dz * dz;
        if (d2 < 256.0 && !p.dead() && !p.invulnerable) faceTowards(m, p.posX, p.posY, p.posZ, 10.0f);
        if (m.onGround && m.slimeJumpDelay-- <= 0) {
            m.slimeJumpDelay = jr.nextInt(20) + 10;
            if (d2 < 256.0) m.slimeJumpDelay /= 3;
            m.jumping = true;
            m.squish = 1.0f;
            if (m.slimeSize > 1) sound::play("mob.slime", m.x, m.y, m.z, 0.4f * m.slimeSize, ((jr.nextFloat() - jr.nextFloat()) * 0.2f + 1.0f) * 0.8f);
            m.moveStrafing = 1.0f - jr.nextFloat() * 2.0f;
            m.moveForward = float(1 * m.slimeSize);
        } else {
            m.jumping = false;
            if (m.onGround) m.moveStrafing = m.moveForward = 0.0f;
        }
        // squashing a player (size > 1)
        if (m.slimeSize > 1 && d2 < double(0.6f * m.slimeSize) * double(0.6f * m.slimeSize) * 1.5 && m.attackTime <= 0 &&
            p.boundingBox.intersects(m.box.expand(0.2, 0, 0.2))) {
            m.attackTime = 20;
            p.attack(m.slimeSize, Player::OTHER, m.x, m.z);
            sound::play("mob.slimeattack", m.x, m.y, m.z, 1.0f, (jr.nextFloat() - jr.nextFloat()) * 0.2f + 1.0f);
        }
    } else {
        creatureAI(w, m, p);
    }
    if (m.kind == CREEPER && m.fuse >= 30) {
        explode(w, m, p, items, editWorld);
        return;
    }
    bool water = waterMovement(w, m), lava = lavaMovement(w, m);
    if (m.jumping) {
        if (water || lava) m.my += double(0.04f);
        else if (m.onGround) {
            m.my = double(0.42f);
        }
    }
    m.moveStrafing *= 0.98f;
    m.moveForward *= 0.98f;
    m.randomYawVelocity *= 0.9f;
    moveWithHeading(w, m, m.moveStrafing, m.moveForward);

    // EntityChicken.onLivingUpdate: flapping slows the fall, and the eggs
    if (m.kind == SHEEP && m.sheared && --m.eggTimer <= 0) { // EntitySheep.onLivingUpdate: the wool's back
        sound::play("mob.chickenplop", m.x, m.y, m.z, 1.0f, (jr.nextFloat() - jr.nextFloat()) * 0.2f + 1.0f);
        m.sheared = false;
    }
    if (m.kind == CHICKEN) { // EntityChicken.onLivingUpdate: the wings
        m.prevWing = m.wing;
        m.prevFlapSpeed = m.flapSpeed;
        m.flapSpeed = float(double(m.flapSpeed) + double(m.onGround ? -1 : 4) * 0.3);
        if (m.flapSpeed < 0.0f) m.flapSpeed = 0.0f;
        if (m.flapSpeed > 1.0f) m.flapSpeed = 1.0f;
        if (!m.onGround && m.flapAccel < 1.0f) m.flapAccel = 1.0f;
        m.flapAccel = float(double(m.flapAccel) * 0.9);
        if (!m.onGround && m.my < 0.0) m.my *= 0.6;
        m.wing += m.flapAccel * 2.0f;
        if (--m.eggTimer <= 0 && m.health > 0) {
            sound::play("mob.chickenplop", m.x, m.y, m.z, 1.0f, (jr.nextFloat() - jr.nextFloat()) * 0.2f + 1.0f);
            ItemStack egg;
            egg.id = 344;
            egg.count = 1;
            items.spawn(m.x, m.y, m.z, egg, 10);
            m.eggTimer = jr.nextInt(6000) + 6000;
        }
    }

    // EntityLiving.onUpdate tail: the body turns toward where it walks
    double ddx = m.x - m.px, ddz = m.z - m.pz;
    float moved = MathHelper::sqrt_double(ddx * ddx + ddz * ddz);
    float body = m.renderYaw;
    if (moved > 0.05f) body = float(std::atan2(ddz, ddx)) * 180.0f / PI_F - 90.0f;
    float d = body - m.renderYaw;
    while (d < -180.0f) d += 360.0f;
    while (d >= 180.0f) d -= 360.0f;
    m.renderYaw += d * 0.3f;
    float h = m.yaw - m.renderYaw;
    while (h < -180.0f) h += 360.0f;
    while (h >= 180.0f) h -= 360.0f;
    if (h < -75.0f) h = -75.0f;
    if (h >= 75.0f) h = 75.0f;
    m.renderYaw = m.yaw - h;
    if (h * h > 2500.0f) m.renderYaw += h * 0.2f;
    while (m.yaw - m.prevYaw < -180.0f) m.prevYaw -= 360.0f;
    while (m.yaw - m.prevYaw >= 180.0f) m.prevYaw += 360.0f;
    while (m.renderYaw - m.prevRenderYaw < -180.0f) m.prevRenderYaw -= 360.0f;
    while (m.renderYaw - m.prevRenderYaw >= 180.0f) m.prevRenderYaw += 360.0f;
}

// Explosion.func_12248_a + func_12247_b (createExplosion, size 3)
void Mobs::explode(World& w, Mob& m, Player& p, ItemEntities& items, bool editWorld) {
    explodeAt(w, m.x, m.y, m.z, 3.0f, editWorld, false, true, &m, p, items, false);
    m.dead = true;
    m.health = 0;
}

// Explosion: blocks within reach of the rays break (breakBlocks) or, for the Fireball spell, only
// the empty spots count and some catch fire (fire); everything nearby is hurt and thrown
void Mobs::explodeAt(World& w, double ex, double ey, double ez, float size, bool editWorld, bool fire, bool breakBlocks,
                     const Mob* source, Player& p, ItemEntities& items, bool byPlayer) {
    const Mob* srcMob = source;
    std::vector<int64_t> hit;
    auto key = [](int x, int y, int z) { return (int64_t(x) & 0x3FFFFFF) << 38 | (int64_t(z) & 0x3FFFFFF) << 12 | (y & 0xFFF); };
    std::vector<std::array<int, 3>> blocks;
    if (editWorld) {
        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 16; ++j)
                for (int k = 0; k < 16; ++k) {
                    if (!(i == 0 || i == 15 || j == 0 || j == 15 || k == 0 || k == 15)) continue;
                    double dx = double(float(i) / 15.0f * 2.0f - 1.0f), dy = double(float(j) / 15.0f * 2.0f - 1.0f),
                           dz = double(float(k) / 15.0f * 2.0f - 1.0f);
                    double l = std::sqrt(dx * dx + dy * dy + dz * dz);
                    dx /= l;
                    dy /= l;
                    dz /= l;
                    float power = size * (0.7f + w.rand.nextFloat() * 0.6f);
                    double x = ex, y = ey, z = ez;
                    for (const float step = 0.3f; power > 0.0f; power -= step * 0.75f) {
                        int bx = fl(x), by = fl(y), bz = fl(z);
                        int id = (by >= 0 && by < 256) ? w.getBlockId(bx, by, bz) : 0;
                        if (id > 0) power -= (BLOCK_RESISTANCE[id] + 0.3f) * step;
                        if (power > 0.0f && by >= 0 && by < 256) {
                            int64_t kk = key(bx, by, bz);
                            if (std::find(hit.begin(), hit.end(), kk) == hit.end()) {
                                hit.push_back(kk);
                                blocks.push_back({bx, by, bz});
                            }
                        }
                        x += dx * double(step);
                        y += dy * double(step);
                        z += dz * double(step);
                    }
                }
    }
    // entities (the player, other mobs) within twice the size
    const double reach = double(size * 2.0f);
    auto exposure = [&](const AABB& b) { // World.func_675_a
        double sx = 1.0 / ((b.maxX - b.minX) * 2.0 + 1.0), sy = 1.0 / ((b.maxY - b.minY) * 2.0 + 1.0),
               sz = 1.0 / ((b.maxZ - b.minZ) * 2.0 + 1.0);
        int open = 0, total = 0;
        for (float a = 0.0f; a <= 1.0f; a = float(double(a) + sx))
            for (float c = 0.0f; c <= 1.0f; c = float(double(c) + sy))
                for (float d = 0.0f; d <= 1.0f; d = float(double(d) + sz)) {
                    double px = b.minX + (b.maxX - b.minX) * double(a), py = b.minY + (b.maxY - b.minY) * double(c),
                           pz = b.minZ + (b.maxZ - b.minZ) * double(d);
                    float vx = float(ex - px), vy = float(ey - py), vz = float(ez - pz);
                    float len = std::sqrt(vx * vx + vy * vy + vz * vz);
                    if (len < 0.001f || !w.rayTrace(float(px), float(py), float(pz), vx / len, vy / len, vz / len, len).hit) ++open;
                    ++total;
                }
        return total ? float(open) / float(total) : 0.0f;
    };
    auto push = [&](double x, double y, double z, const AABB& b, double& mx, double& my, double& mz) -> int {
        double dx = x - ex, dy = y - ey, dz = z - ez;
        double d = std::sqrt(dx * dx + dy * dy + dz * dz) / reach;
        if (d > 1.0) return -1;
        double l = double(MathHelper::sqrt_double(dx * dx + dy * dy + dz * dz));
        if (l > 0) { dx /= l; dy /= l; dz /= l; }
        double ex2 = double(exposure(b));
        double f = (1.0 - d) * ex2;
        mx += dx * f;
        my += dy * f;
        mz += dz * f;
        return int((f * f + f) / 2.0 * 8.0 * reach + 1.0);
    };
    if (!byPlayer) {
        double kx = 0, ky = 0, kz = 0; // Explosion: not with the Bear Charm
        int dmg = push(p.posX, p.posY, p.posZ, p.boundingBox, kx, ky, kz);
        p.motionX += kx; p.motionY += ky; p.motionZ += kz;
        if (dmg > 0) p.attack(dmg, srcMob ? Player::MOB : Player::OTHER, ex, ez); // a creeper's blast scales, TNT's doesn't
    }
    if (!breakBlocks) { // the Fireball: only empty spots, some set alight
        std::vector<std::array<int, 3>> air;
        for (auto& b : blocks)
            if (w.getBlockId(b[0], b[1], b[2]) == 0) air.push_back(b);
        blocks.swap(air);
    }
    if (fire && editWorld)
        for (int i = int(blocks.size()) - 1; i >= 0; --i) {
            int x = blocks[i][0], y = blocks[i][1], z = blocks[i][2];
            if (w.getBlockId(x, y, z) == 0 && BP[w.getBlockId(x, y - 1, z)].opaqueLookup && w.rand.nextInt(3) == 0)
                w.setBlockWithNotify(x, y, z, 51);
        }
    for (Mob& o : mobs) {
        if (&o == srcMob || o.dead) continue;
        int dmg = push(o.x, o.y, o.z, o.box, o.mx, o.my, o.mz);
        if (dmg > 0) hurt(o, dmg, byPlayer, ex, ez, &items, &w);
    }
    sound::play("random.explode", ex, ey, ez, 4.0f, (1.0f + (w.rand.nextFloat() - w.rand.nextFloat()) * 0.2f) * 0.7f);
    if (gParticles) // Explosion.func_12247_b: smoke from where the blocks were (fewer of them on 3DS)
        for (size_t i = 0; i < blocks.size(); i += 3) {
            double x = blocks[i][0] + double(w.rand.nextFloat()), y = blocks[i][1] + double(w.rand.nextFloat()),
                   z = blocks[i][2] + double(w.rand.nextFloat());
            double dx = x - ex, dy = y - ey, dz = z - ez;
            double d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d < 1e-3) continue;
            double k = 0.5 / (d / double(size) + 0.1) * double(w.rand.nextFloat() * w.rand.nextFloat() + 0.3f);
            dx = dx / d * k; dy = dy / d * k; dz = dz / d * k;
            gParticles->spawn(Particles::EXPLODE, (x + ex) / 2.0, (y + ey) / 2.0, (z + ez) / 2.0, dx, dy, dz);
            gParticles->spawn(Particles::SMOKE, x, y, z, dx, dy, dz);
        }
    for (int i = int(blocks.size()) - 1; i >= 0; --i) {
        int x = blocks[i][0], y = blocks[i][1], z = blocks[i][2];
        int id = w.getBlockId(x, y, z);
        if (id <= 0) continue;
        if (id == 46) { // BlockTNT.onBlockDestroyedByExplosion: a short fuse
            w.setBlockWithNotify(x, y, z, 0);
            primeTnt(x, y, z, true);
            continue;
        }
        if (w.rand.nextFloat() <= 0.3f) Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
        w.setBlockWithNotify(x, y, z, 0);
    }
}

// EntityArrow.onUpdate for skeleton arrows (they hurt the player; they can't be picked up)
void Mobs::tickArrows(World& w, Player& p) {
    for (Arrow& a : arrows) {
        a.px = a.x;
        a.py = a.y;
        a.pz = a.z;
        if (a.inGround) {
            if (w.getBlockId(a.tileX, a.tileY, a.tileZ) != a.inTile) {
                a.inGround = false;
                a.mx *= double(jr.nextFloat() * 0.2f);
                a.my *= double(jr.nextFloat() * 0.2f);
                a.mz *= double(jr.nextFloat() * 0.2f);
                a.ticksInGround = 0;
                a.ticksInAir = 0;
            } else {
                if (++a.ticksInGround == 1200) a.dead = true;
                // onCollideWithPlayer: your own arrows go back in the quiver
                AABB reach = p.boundingBox.expand(1.0, 0.0, 1.0);
                if (a.fromPlayer && !p.dead() && p.inv && a.x >= reach.minX && a.x <= reach.maxX && a.y >= reach.minY - 0.25 &&
                    a.y <= reach.maxY + 0.25 && a.z >= reach.minZ && a.z <= reach.maxZ && p.inv->addItem(262, 1)) {
                    sound::play("random.pop", a.x, a.y, a.z, 0.2f, ((jr.nextFloat() - jr.nextFloat()) * 0.7f + 1.0f) * 2.0f);
                    a.dead = true;
                    pickedUpSomething = true;
                }
                continue;
            }
        } else {
            ++a.ticksInAir;
        }
        float len = std::sqrt(float(a.mx * a.mx + a.my * a.my + a.mz * a.mz));
        RayHit h;
        if (len > 0.0f) h = w.rayTrace(float(a.x), float(a.y), float(a.z), float(a.mx) / len, float(a.my) / len, float(a.mz) / len, len);
        if (a.fromPlayer || a.fromDispenser) { // the player's (or a dispenser's) arrow: mobs in the way
            double t;
            int v = len > 0.0f ? mobOnPath(a.x, a.y, a.z, a.mx / len, a.my / len, a.mz / len, len, 0.3, t) : -1;
            if (v >= 0) {
                if (hurt(mobs[v], 4, true, a.x, a.z, nullptr, &w)) {
                    sound::play("random.drr", a.x, a.y, a.z, 1.0f, 1.2f / (jr.nextFloat() * 0.2f + 0.9f));
                    a.dead = true;
                    continue;
                }
                a.mx *= double(-0.1f);
                a.my *= double(-0.1f);
                a.mz *= double(-0.1f);
                a.yaw += 180.0f;
                a.ticksInAir = 0;
            }
        }
        // the player in the way?
        AABB pb = p.boundingBox.expand(0.3, 0.3, 0.3);
        bool hitPlayer = false;
        if (!a.fromPlayer || a.ticksInAir >= 5) {
            for (int s = 0; s <= 8; ++s) {
                double t = double(s) / 8.0;
                double x = a.x + a.mx * t, y = a.y + a.my * t, z = a.z + a.mz * t;
                if (x >= pb.minX && x <= pb.maxX && y >= pb.minY && y <= pb.maxY && z >= pb.minZ && z <= pb.maxZ) {
                    hitPlayer = true;
                    break;
                }
            }
        }
        if (hitPlayer && !p.dead()) {
            if (p.attack(4, Player::MOB_ALWAYS, a.x, a.z)) { // EntityArrow
                sound::play("random.drr", a.x, a.y, a.z, 1.0f, 1.2f / (jr.nextFloat() * 0.2f + 0.9f));
                a.dead = true;
                continue;
            }
            a.mx *= -0.1;
            a.my *= -0.1;
            a.mz *= -0.1;
            a.yaw += 180.0f;
            a.ticksInAir = 0;
        } else if (h.hit) {
            a.tileX = h.x;
            a.tileY = h.y;
            a.tileZ = h.z;
            a.inTile = w.getBlockId(h.x, h.y, h.z);
            // stop just short of the block
            double cx = double(h.x) + 0.5 - a.x, cy = double(h.y) + 0.5 - a.y, cz = double(h.z) + 0.5 - a.z;
            (void)cx; (void)cy; (void)cz;
            a.x += a.mx * 0.5;
            a.y += a.my * 0.5;
            a.z += a.mz * 0.5;
            sound::play("random.drr", a.x, a.y, a.z, 1.0f, 1.2f / (jr.nextFloat() * 0.2f + 0.9f));
            a.inGround = true;
            continue;
        }
        a.x += a.mx;
        a.y += a.my;
        a.z += a.mz;
        float hz = std::sqrt(float(a.mx * a.mx + a.mz * a.mz));
        a.yaw = float(std::atan2(a.mx, a.mz) * 180.0 / double(PI_F));
        a.pitch = float(std::atan2(a.my, double(hz)) * 180.0 / double(PI_F));
        float drag = 0.99f;
        if (w.getBlockId(fl(a.x), fl(a.y), fl(a.z)) == 8 || w.getBlockId(fl(a.x), fl(a.y), fl(a.z)) == 9) drag = 0.8f;
        a.mx *= double(drag);
        a.my *= double(drag);
        a.mz *= double(drag);
        a.my -= double(0.03f);
        if (a.y < -64.0) a.dead = true;
    }
    arrows.erase(std::remove_if(arrows.begin(), arrows.end(), [](const Arrow& a) { return a.dead; }), arrows.end());
}

void Mobs::tick(World& w, Player& p, ItemEntities& items, bool editWorld, bool peaceful) {
    curItems = &items;
    for (size_t i = 0; i < mobs.size(); ++i) {
        Mob& m = mobs[i];
        if (m.dead) continue;
        if (m.uid == 0) m.uid = nextUid++;
        // only mobs in loaded chunks move (Java's 32-block-padded update area)
        if (!w.isChunkLoaded(fl(m.x) >> 4, fl(m.z) >> 4)) continue;
        if (peaceful && (STATS[m.kind].type == MONSTER || m.kind == GIANT)) { m.dead = true; continue; } // EntityMobs.onUpdate
        m.prevFuse = m.fuse;
        bool wasOnGround = m.onGround;
        if (m.kind == SLIME) m.prevSquish = m.squish;
        livingUpdate(w, mobs[i], p, items, editWorld);
        if (i < mobs.size() && mobs[i].kind == SLIME) { // EntitySlime.onUpdate: it splats as it lands
            Mob& sl = mobs[i];
            if (sl.onGround && !wasOnGround) {
                if (gParticles)
                    for (int k = 0; k < sl.slimeSize * 8; ++k) {
                        float a = jr.nextFloat() * 3.141593f * 2.0f, r = jr.nextFloat() * 0.5f + 0.5f;
                        float ox = MathHelper::sin(a) * float(sl.slimeSize) * 0.5f * r, oz = MathHelper::cos(a) * float(sl.slimeSize) * 0.5f * r;
                        gParticles->spawn(Particles::SLIME, sl.x + ox, sl.box.minY, sl.z + oz, 0, 0, 0);
                    }
                if (sl.slimeSize > 2)
                    sound::play("mob.slime", sl.x, sl.y, sl.z, 0.4f * sl.slimeSize, ((jr.nextFloat() - jr.nextFloat()) * 0.2f + 1.0f) / 0.8f);
                sl.squish = -0.5f;
            }
            sl.squish *= 0.6f;
        }
        if (editWorld && i < mobs.size() && !mobs[i].dead) Redstone::entityTouching(w, mobs[i].box); // pressure plates
    }
    // Entity.updateRidden: a rider sits on its mount (EntitySpider.getMountedYOffset: 0.75 of its
    // height less half a block), whatever its own legs did this tick
    for (Mob& r : mobs) {
        if (!r.vehicle || r.dead) continue;
        if (r.vehicle < 0) { // in a minecart: EntityMinecart's mounted offset (-0.3) and its own yOffset (0)
            double cx, cy, cz;
            if (!cartSeat || !cartSeat(-r.vehicle, cx, cy, cz)) { r.vehicle = 0; continue; }
            r.mx = r.my = r.mz = 0.0;
            r.fallDistance = 0.0f;
            setPosition(r, cx, cy, cz);
            r.onGround = true;
            continue;
        }
        Mob* v = byUid(r.vehicle);
        if (!v || v->dead || v->health <= 0) { r.vehicle = 0; continue; }
        double y = v->y + double(v->height) * 0.75 - 0.5;
        r.mx = r.my = r.mz = 0.0;
        r.fallDistance = 0.0f;
        setPosition(r, v->x, y, v->z);
        r.onGround = true;
    }
    // Entity.applyEntityCollision: mobs push each other and the player apart
    for (size_t i = 0; i < mobs.size(); ++i) {
        Mob& a = mobs[i];
        if (a.dead) continue;
        auto pushApart = [&](double ax, double az, double bx, double bz, double& amx, double& amz, double& bmx, double& bmz) {
            double dx = bx - ax, dz = bz - az;
            double d = std::max(std::fabs(dx), std::fabs(dz));
            if (d >= 0.01) {
                d = std::sqrt(d);
                dx /= d;
                dz /= d;
                double f = 1.0 / d;
                if (f > 1.0) f = 1.0;
                dx *= f * 0.05;
                dz *= f * 0.05;
                amx -= dx;
                amz -= dz;
                bmx += dx;
                bmz += dz;
            }
        };
        AABB ab = a.box.expand(0.2, 0.0, 0.2);
        for (size_t j = i + 1; j < mobs.size(); ++j) {
            Mob& b = mobs[j];
            if (b.dead || !ab.intersects(b.box)) continue;
            if (a.vehicle == b.uid || b.vehicle == a.uid) continue; // a rider and its mount
            pushApart(a.x, a.z, b.x, b.z, a.mx, a.mz, b.mx, b.mz);
        }
        if (ab.intersects(p.boundingBox) && !p.dead()) {
            double pmx = p.motionX, pmz = p.motionZ;
            pushApart(a.x, a.z, p.posX, p.posZ, a.mx, a.mz, pmx, pmz);
            p.motionX = pmx;
            p.motionZ = pmz;
        }
    }
    tickArrows(w, p);
    tickTnt(w, p, items, editWorld);
    tickFalling(w, items);
    tickThrown(w, p, items);
    mobs.erase(std::remove_if(mobs.begin(), mobs.end(), [](const Mob& m) { return m.dead; }), mobs.end());
}

// ================================================================ SpawnerAnimals

namespace {
// The order Java's HashSet<ChunkCoordIntPair> gives the 9 x 9 chunks round the player: by bucket
// of ChunkCoordIntPair.hashCode in a 128-bucket table, then in the order they went in
std::vector<std::pair<int, int>> spawnChunkOrder(int pcx, int pcz) {
    struct E { int bucket, order, x, z; };
    std::vector<E> e;
    int n = 0;
    for (int dx = -4; dx <= 4; ++dx)
        for (int dz = -4; dz <= 4; ++dz) {
            int x = dx + pcx, z = dz + pcz;
            int32_t h = int32_t((x < 0 ? 0x80000000u : 0u) | uint32_t(x & 0x7FFF) << 16 | (z < 0 ? 0x8000u : 0u) | uint32_t(z & 0x7FFF));
            int32_t hh = h ^ int32_t(uint32_t(h) >> 16);
            e.push_back({hh & 127, n++, x, z});
        }
    std::stable_sort(e.begin(), e.end(), [](const E& a, const E& b) { return a.bucket < b.bucket; });
    std::vector<std::pair<int, int>> out;
    for (const E& k : e) out.emplace_back(k.x, k.z);
    return out;
}
} // namespace

// PlayerControllerSP.updateController: SpawnerMonsters (zombies, skeletons, creepers, spiders,
// slimes; 200) then SpawnerAnimals (sheep, pigs, cows, chickens; 15), every tick
void Mobs::spawn(World& w, const Player& p, bool, bool) {
    for (int type = 0; type < 2; ++type) {
        const bool monsterSpawner = type == 0;
        int have = 0; // World.getMatchingEntities(IMobs / EntityAnimals)
        for (const Mob& m : mobs)
            if (!m.dead && (monsterSpawner ? (STATS[m.kind].type == MONSTER || m.kind == GIANT) : STATS[m.kind].type == ANIMAL)) ++have;
        float ratio = monsterSpawner ? 2.0f - w.mobRatio() : w.mobRatio();
        if (!(float(have) < float(monsterSpawner ? 200 : 15) * ratio)) continue;
        static const Kind MONSTERS[5] = {ZOMBIE, SKELETON, CREEPER, SPIDER, SLIME};
        static const Kind ANIMALS[4] = {SHEEP, PIG, COW, CHICKEN};
        for (int roll = 0; roll < 3; ++roll) { // rollForMobSpawn
            int hostileAttempts = gOptions.difficulty;
            for (const auto& ch : spawnChunkOrder(fl(p.posX / 16.0), fl(p.posZ / 16.0))) {
                if (w.rand.nextInt(10) != 0) continue;
                Kind kind = monsterSpawner ? MONSTERS[w.rand.nextInt(5)] : ANIMALS[w.rand.nextInt(4)];
                int cx = ch.first * 16 + w.rand.nextInt(16), cy, cz;
                if (monsterSpawner) { // SpawnerMonsters.getSpawnLoc
                    if (w.skylightSubtracted > 3)
                        cy = w.rand.nextInt(4) > 0 ? w.rand.nextInt(60) + 68 : w.rand.nextInt(w.rand.nextInt(120) + 8);
                    else
                        cy = w.rand.nextInt(4) > 0 ? w.rand.nextInt(60) : w.rand.nextInt(w.rand.nextInt(120) + 8);
                } else {
                    cy = w.rand.nextInt(128);
                }
                cz = ch.second * 16 + w.rand.nextInt(16);
                if (!w.isChunkLoaded(cx >> 4, cz >> 4)) continue;
                bool normal = BP[w.getBlockId(cx, cy, cz)].opaqueCube; // isBlockNormalCube
                if (normal || BP[w.getBlockId(cx, cy, cz)].mat != Mat::air) {
                    if (cy >= 64 || !monsterSpawner || w.skylightSubtracted >= 3 || hostileAttempts <= 0) goto rolled;
                    --hostileAttempts;
                    continue;
                }
                for (int g = 0; g < 3; ++g) {
                    int x = cx, y = cy, z = cz;
                    for (int t = 0; t < 2; ++t) {
                        x += w.rand.nextInt(6) - w.rand.nextInt(6);
                        y += w.rand.nextInt(1) - w.rand.nextInt(1);
                        z += w.rand.nextInt(6) - w.rand.nextInt(6);
                        if (!w.isChunkLoaded(x >> 4, z >> 4)) continue;
                        auto opaque = [&](int a, int b, int c) { return BP[w.getBlockId(a, b, c)].opaqueCube; };
                        if (!opaque(x, y - 1, z) || opaque(x, y, z) || matLiquid(BP[w.getBlockId(x, y, z)].mat) || opaque(x, y + 1, z))
                            continue; // (getIsGroundCover: the liquids)
                        float fx = float(x) + 0.5f, fy = float(y), fz = float(z) + 0.5f;
                        double pdx = double(fx) - p.posX, pdy = double(fy) - (p.posY - 1.62), pdz = double(fz) - p.posZ;
                        if (!p.dead() && pdx * pdx + pdy * pdy + pdz * pdz < 576.0) continue; // getClosestPlayer(24)
                        float sx = fx - float(w.info.spawnX), sy = fy - float(w.info.spawnY), sz = fz - float(w.info.spawnZ);
                        if (sx * sx + sy * sy + sz * sz < 576.0f) continue;
                        Mob m;
                        m.kind = kind;
                        m.x = m.px = double(fx);
                        m.y = m.py = double(fy);
                        m.z = m.pz = double(fz);
                        m.yaw = m.prevYaw = m.renderYaw = m.prevRenderYaw = w.rand.nextFloat() * 360.0f;
                        m.pitch = 0;
                        if (kind == SLIME) m.slimeSize = 1 << jr.nextInt(3);
                        initMob(m);
                        m.health = m.prevHealth = kind == SLIME ? m.slimeSize * m.slimeSize : STATS[kind].health;
                        // shouldSpawnOnTile
                        int fx2 = fl(m.x), fy2 = fl(m.box.minY), fz2 = fl(m.z);
                        Physics::collidingBoxes(w, m.box, boxes);
                        bool roomy = boxes.empty() && !Physics::isAnyLiquid(w, m.box);
                        bool ok;
                        if (kind == SLIME) { // (the chunk is looked up by x and *y*, as in the jar)
                            int scx = fl(m.x) >> 4, scz = fl(m.y) >> 4;
                            JavaRandom cr(w.info.seed + int64_t(scx * scx * 4987142) + int64_t(scx * 5947611) +
                                          int64_t(scz * scz) * 4392871LL + int64_t(scz * 389711) ^ 987234911LL);
                            ok = (m.slimeSize == 1 || gOptions.difficulty > 0) && jr.nextInt(10) == 0 && cr.nextInt(10) == 0 && m.y < 16.0;
                        } else if (STATS[kind].type == ANIMAL) {
                            ok = w.getBlockId(fx2, fy2 - 1, fz2) == 2 && w.getBlockLightValue(fx2, fy2, fz2) > 8 && roomy &&
                                 pathWeight(w, m, fx2, fy2, fz2) >= 0.0f;
                        } else {
                            const Chunk* cc = w.getChunk(fx2 >> 4, fz2 >> 4);
                            int skyl = cc ? cc->getSkyLight(fx2 & 15, fy2, fz2 & 15) : 15;
                            ok = !(skyl > jr.nextInt(32)) && w.getBlockLightValue(fx2, fy2, fz2) <= jr.nextInt(8) && roomy &&
                                 pathWeight(w, m, fx2, fy2, fz2) >= 0.0f;
                        }
                        if (!ok) continue;
                        if (kind == SPIDER && w.rand.nextInt(100) == 0) { // a spider jockey
                            m.uid = nextUid++;
                            Mob rider;
                            rider.kind = SKELETON;
                            rider.x = rider.px = m.x;
                            rider.y = rider.py = m.y;
                            rider.z = rider.pz = m.z;
                            rider.yaw = rider.prevYaw = rider.renderYaw = rider.prevRenderYaw = m.yaw;
                            rider.pitch = 0;
                            initMob(rider);
                            rider.health = rider.prevHealth = STATS[SKELETON].health;
                            rider.uid = nextUid++;
                            rider.vehicle = m.uid;
                            mobs.push_back(std::move(m));
                            mobs.push_back(std::move(rider));
                        } else {
                            mobs.push_back(std::move(m));
                        }
                    }
                }
            }
        rolled:;
        }
    }
}

// ================================================================ the player's sword

int Mobs::pick(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const {
    int best = -1;
    dist = reach;
    for (size_t i = 0; i < mobs.size(); ++i) {
        const Mob& m = mobs[i];
        if (m.dead || m.health <= 0) continue;
        AABB b = m.box.expand(0.1, 0.1, 0.1); // getCollisionBorderSize
        // slab test
        double t0 = 0.0, t1 = dist;
        const double o[3] = {ox, oy, oz}, d[3] = {dx, dy, dz};
        const double lo[3] = {b.minX, b.minY, b.minZ}, hi[3] = {b.maxX, b.maxY, b.maxZ};
        bool ok = true;
        for (int a = 0; a < 3 && ok; ++a) {
            if (std::fabs(d[a]) < 1e-9) {
                if (o[a] < lo[a] || o[a] > hi[a]) ok = false;
                continue;
            }
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) ok = false;
        }
        if (ok && t0 < dist) {
            dist = t0;
            best = int(i);
        }
    }
    return best;
}

void Mobs::attack(int index, Player& p, int damage, ItemEntities& items) {
    if (index < 0 || index >= int(mobs.size())) return;
    hurt(mobs[index], damage, true, p.posX, p.posZ, &items, nullptr);
}

// ================================================================ primed TNT

void Mobs::primeTnt(int bx, int by, int bz, bool byExplosion) {
    PrimedTnt t{};
    t.x = t.px = bx + 0.5;
    t.y = t.py = by + 0.5;
    t.z = t.pz = bz + 0.5;
    float a = float(jr.nextDouble() * double(PI_F) * 2.0); // Math.random(), used as degrees (as Beta does)
    t.mx = double(-MathHelper::sin(a * PI_F / 180.0f) * 0.02f);
    t.my = double(0.2f);
    t.mz = double(-MathHelper::cos(a * PI_F / 180.0f) * 0.02f);
    t.fuse = byExplosion ? jr.nextInt(80 / 4) + 80 / 8 : 80;
    tnts.push_back(t);
}

void Mobs::tickTnt(World& w, Player& p, ItemEntities& items, bool editWorld) {
    std::vector<AABB> boxes;
    for (size_t i = 0; i < tnts.size(); ++i) {
        PrimedTnt t = tnts[i];
        t.px = t.x;
        t.py = t.y;
        t.pz = t.z;
        t.my -= double(0.04f);
        { // Entity.moveEntity (0.98 cube, posY 0.49 above its bottom)
            AABB bb{t.x - 0.49, t.y - 0.49, t.z - 0.49, t.x + 0.49, t.y + 0.49, t.z + 0.49};
            double dx = t.mx, dy = t.my, dz = t.mz, ox = dx, oy = dy, oz = dz;
            boxes.clear();
            Physics::collidingBoxes(w, bb.addCoord(dx, dy, dz), boxes);
            for (auto& b : boxes) dy = b.calculateYOffset(bb, dy);
            bb = bb.offset(0, dy, 0);
            for (auto& b : boxes) dx = b.calculateXOffset(bb, dx);
            bb = bb.offset(dx, 0, 0);
            for (auto& b : boxes) dz = b.calculateZOffset(bb, dz);
            bb = bb.offset(0, 0, dz);
            t.x = (bb.minX + bb.maxX) / 2.0;
            t.y = bb.minY + 0.49;
            t.z = (bb.minZ + bb.maxZ) / 2.0;
            t.onGround = oy != dy && oy < 0.0;
            if (ox != dx) t.mx = 0.0;
            if (oy != dy) t.my = 0.0;
            if (oz != dz) t.mz = 0.0;
        }
        t.mx *= double(0.98f);
        t.my *= double(0.98f);
        t.mz *= double(0.98f);
        if (t.onGround) {
            t.mx *= double(0.7f);
            t.mz *= double(0.7f);
            t.my *= -0.5;
        }
        if (t.fuse-- <= 0) {
            tnts.erase(tnts.begin() + long(i));
            --i;
            explodeAt(w, t.x, t.y, t.z, 4.0f, editWorld, false, true, nullptr, p, items, false);
            continue;
        }
        if (gParticles) gParticles->spawn(Particles::SMOKE, t.x, t.y + 0.5, t.z, 0, 0, 0);
        tnts[i] = t;
    }
}

// ================================================================ the player's projectiles

int Mobs::mobOnPath(double x, double y, double z, double dx, double dy, double dz, double maxT, double grow, double& tHit) {
    int victim = -1;
    tHit = maxT;
    for (size_t i = 0; i < mobs.size(); ++i) {
        const Mob& m = mobs[i];
        if (m.dead || m.health <= 0) continue;
        AABB box = m.box.expand(grow, grow, grow);
        double t0 = 0.0, t1 = tHit;
        const double o[3] = {x, y, z}, d[3] = {dx, dy, dz};
        const double lo[3] = {box.minX, box.minY, box.minZ}, hi[3] = {box.maxX, box.maxY, box.maxZ};
        bool ok = true;
        for (int a = 0; a < 3 && ok; ++a) {
            if (std::fabs(d[a]) < 1e-9) { if (o[a] < lo[a] || o[a] > hi[a]) ok = false; continue; }
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) ok = false;
        }
        if (ok && t0 <= tHit) { tHit = t0; victim = int(i); }
    }
    return victim;
}

void Mobs::throwFrom(const Player& p, int what) {
    // EntityArrow / EntitySnowball / EntityEgg (world, thrower): from the eyes, then setArrowHeading(1.5, 1.0)
    float yaw = p.rotationYaw, pitch = p.rotationPitch;
    double x = p.posX - double(MathHelper::cos(yaw / 180.0f * PI_F) * 0.16f);
    double y = p.posY + double(0.12f) - double(0.1f);
    double z = p.posZ - double(MathHelper::sin(yaw / 180.0f * PI_F) * 0.16f);
    double hx = double(-MathHelper::sin(yaw / 180.0f * PI_F) * MathHelper::cos(pitch / 180.0f * PI_F));
    double hz = double(MathHelper::cos(yaw / 180.0f * PI_F) * MathHelper::cos(pitch / 180.0f * PI_F));
    double hy = double(-MathHelper::sin(pitch / 180.0f * PI_F));
    float len = MathHelper::sqrt_double(hx * hx + hy * hy + hz * hz);
    hx /= double(len);
    hy /= double(len);
    hz /= double(len);
    hx += jr.nextGaussian() * double(0.0075f);
    hy += jr.nextGaussian() * double(0.0075f);
    hz += jr.nextGaussian() * double(0.0075f);
    hx *= 1.5;
    hy *= 1.5;
    hz *= 1.5;
    if (what == T_ARROW) {
        Arrow a{};
        a.x = a.px = x;
        a.y = a.py = y;
        a.z = a.pz = z;
        a.mx = hx;
        a.my = hy;
        a.mz = hz;
        float hz2 = MathHelper::sqrt_double(hx * hx + hz * hz);
        a.yaw = float(std::atan2(hx, hz) * 180.0 / double(PI_F));
        a.pitch = float(std::atan2(hy, double(hz2)) * 180.0 / double(PI_F));
        a.fromPlayer = true;
        arrows.push_back(a);
    } else {
        Thrown t{};
        t.kind = what;
        t.x = t.px = x;
        t.y = t.py = y;
        t.z = t.pz = z;
        t.mx = hx;
        t.my = hy;
        t.mz = hz;
        thrown.push_back(t);
    }
}

void Mobs::tickThrown(World& w, Player& p, ItemEntities& items) {
    (void)p;
    std::vector<Mob> chicks;
    for (Thrown& t : thrown) {
        t.px = t.x;
        t.py = t.y;
        t.pz = t.z;
        float len = std::sqrt(float(t.mx * t.mx + t.my * t.my + t.mz * t.mz));
        bool hit = false;
        if (len > 0.0f) {
            RayHit h = w.rayTrace(float(t.x), float(t.y), float(t.z), float(t.mx) / len, float(t.my) / len, float(t.mz) / len, len);
            double reach = len;
            if (h.hit) {
                double cx = h.x + 0.5 - t.x, cy = h.y + 0.5 - t.y, cz = h.z + 0.5 - t.z;
                reach = std::max(0.0, std::sqrt(cx * cx + cy * cy + cz * cz) - 0.5);
            }
            double vt;
            int v = mobOnPath(t.x, t.y, t.z, t.mx / len, t.my / len, t.mz / len, reach, 0.3, vt);
            if (v >= 0) {
                hurt(mobs[v], 0, true, t.x, t.z, &items, &w); // knockback only
                t.x += t.mx / len * vt;
                t.y += t.my / len * vt;
                t.z += t.mz / len * vt;
                hit = true;
            } else if (h.hit) {
                hit = true;
            }
        }
        if (hit) {
            if (t.kind == T_EGG && jr.nextInt(8) == 0) { // a chick hatches (four, once in a while)
                int n = jr.nextInt(32) == 0 ? 4 : 1;
                for (int i = 0; i < n; ++i) {
                    Mob m;
                    m.kind = CHICKEN;
                    m.x = m.px = t.x;
                    m.y = m.py = t.y;
                    m.z = m.pz = t.z;
                    m.yaw = m.prevYaw = m.renderYaw = m.prevRenderYaw = 0.0f;
                    initMob(m);
                    m.health = m.prevHealth = STATS[CHICKEN].health;
                    chicks.push_back(std::move(m));
                }
            }
            if (gParticles)
                for (int i = 0; i < 8; ++i) gParticles->spawn(Particles::SNOWPOOF, t.x, t.y, t.z, 0, 0, 0);
            t.dead = true;
            continue;
        }
        t.x += t.mx;
        t.y += t.my;
        t.z += t.mz;
        float drag = 0.99f;
        if (BP[w.getBlockId(fl(t.x), fl(t.y), fl(t.z))].mat == Mat::water) {
            if (gParticles)
                for (int i = 0; i < 4; ++i)
                    gParticles->spawn(Particles::BUBBLE, t.x - t.mx * 0.25, t.y - t.my * 0.25, t.z - t.mz * 0.25, t.mx, t.my, t.mz);
            drag = 0.8f;
        }
        t.mx *= double(drag);
        t.my *= double(drag);
        t.mz *= double(drag);
        t.my -= double(0.03f);
        if (t.y < -64.0) t.dead = true;
    }
    thrown.erase(std::remove_if(thrown.begin(), thrown.end(), [](const Thrown& t) { return t.dead; }), thrown.end());
    for (Mob& m : chicks) mobs.push_back(std::move(m));
}

bool Mobs::anyIn(const AABB& b) const {
    for (const Mob& m : mobs)
        if (!m.dead && m.health > 0 && m.box.intersects(b)) return true;
    return false;
}

// ================================================================ falling sand and gravel

void Mobs::fallingBlock(int x, int y, int z, int id) {
    Falling f{x + 0.5, y + 0.5, z + 0.5, x + 0.5, y + 0.5, z + 0.5, 0.0, id, 0, false};
    falling.push_back(f);
}

void Mobs::tickFalling(World& w, ItemEntities& items) { // EntityFallingSand.onUpdate
    std::vector<AABB> boxes;
    for (size_t i = 0; i < falling.size(); ++i) {
        Falling f = falling[i];
        if (f.dead) continue;
        f.px = f.x;
        f.py = f.y;
        f.pz = f.z;
        ++f.fallTime;
        f.my -= double(0.04f);
        AABB bb{f.x - 0.49, f.y - 0.49, f.z - 0.49, f.x + 0.49, f.y + 0.49, f.z + 0.49};
        double dy = f.my, oy = dy;
        boxes.clear();
        Physics::collidingBoxes(w, bb.addCoord(0, dy, 0), boxes);
        for (auto& b : boxes) dy = b.calculateYOffset(bb, dy);
        f.y += dy;
        bool onGround = oy != dy && oy < 0.0;
        if (oy != dy) f.my = 0.0;
        f.my *= double(0.98f);
        int bx = fl(f.x), by = fl(f.y), bz = fl(f.z);
        if (w.getBlockId(bx, by, bz) == f.id) w.setBlockWithNotify(bx, by, bz, 0);
        if (onGround) {
            f.dead = true;
            int cur = w.getBlockId(bx, by, bz);
            bool replaceable = cur == 0 || cur == 51 || cur == 78 || matLiquid(BP[cur].mat);
            if (!(replaceable && w.setBlockWithNotify(bx, by, bz, f.id))) {
                ItemStack st;
                st.id = uint16_t(f.id);
                st.count = 1;
                items.spawn(f.x, f.y, f.z, st, 10);
            }
        } else if (f.fallTime > 100) {
            ItemStack st;
            st.id = uint16_t(f.id);
            st.count = 1;
            items.spawn(f.x, f.y, f.z, st, 10);
            f.dead = true;
        }
        falling[i] = f;
    }
    falling.erase(std::remove_if(falling.begin(), falling.end(), [](const Falling& f) { return f.dead; }), falling.end());
}

// ================================================================ riding (EntityPig) and interacting

Mobs::Mob* Mobs::byUid(int uid) {
    if (uid == 0) return nullptr;
    for (Mob& m : mobs)
        if (m.uid == uid) return &m;
    return nullptr;
}

bool Mobs::interact(int index, Player& p, Inventory& inv, bool, bool, std::string&) {
    if (index < 0 || index >= int(mobs.size())) return false;
    Mob& m = mobs[size_t(index)];
    if (m.dead) return false;
    int heldId = inv.held().empty() ? 0 : inv.held().id;
    if (m.kind == COW && heldId == 325) { // EntityCow.interact: a bucket of milk
        inv.held().id = 335;
        return true;
    }
    if (m.kind == PIG && m.saddle) { // EntityPig.interact: on it goes
        if (m.uid == 0) m.uid = nextUid++;
        if (riddenUid == m.uid) dismount(p);
        else riddenUid = m.uid;
        return true;
    }
    return false;
}

bool Mobs::riderSeat(double& x, double& y, double& z) const {
    if (!riddenUid) return false;
    for (const Mob& m : mobs)
        if (m.uid == riddenUid && !m.dead) {
            x = m.x;
            z = m.z;
            // getMountedYOffset (Entity: 0.75 of the height) + the player's getYOffset
            y = m.y + double(m.height) * 0.75 + double(1.62f - 0.5f);
            return true;
        }
    return false;
}

void Mobs::dismount(Player& p) {
    Mob* m = byUid(riddenUid);
    riddenUid = 0;
    if (!m) return;
    // Entity.mountEntity(null): stand on its back, then step off
    p.setPosition(m->x, m->box.minY + double(m->height) + 1.62, m->z);
    p.motionX = p.motionY = p.motionZ = 0;
}

void Mobs::spawnAt(int kind, double x, double y, double z, float yaw) {
    Mob m;
    m.kind = Kind(kind);
    m.x = m.px = x;
    m.y = m.py = y;
    m.z = m.pz = z;
    m.yaw = m.prevYaw = m.renderYaw = m.prevRenderYaw = yaw;
    m.pitch = 0;
    if (kind == SLIME) m.slimeSize = 1;
    initMob(m);
    m.health = m.prevHealth = kind == SLIME ? m.slimeSize * m.slimeSize : STATS[kind].health;
    mobs.push_back(std::move(m));
}

