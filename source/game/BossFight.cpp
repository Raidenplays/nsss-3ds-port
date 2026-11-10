// Boss fights: AxeolonBossFight (wake-up, intro, music, enrage, arena wall, the end),
// EntityAxeolonBoss (arena, gate) and EntityTabish (the Bear Queen: swipe and ground slam),
// AxeolonCinematic (the flying intro camera) and EntityFallingIcicle (shaken loose by her slam).
#include "Mobs.h"
#include <cmath>
#include "ItemEntities.h"
#include "Player.h"
#include "Progress.h"
#include "audio/Sound.h"
#include "render/Particles.h"
#include "world/BlockProps.h"
#include "world/Blocks.h"
#include "world/Bosses.h"
#include "world/MathHelper.h"
#include "world/World.h"

namespace {
constexpr float BPI = 3.1415927f;
constexpr int SLAM_LEN = 25, SLAM_HIT = 11, SWIPE_HIT = 3, SHAKE_LEN = 30, ROAR_LEN = 45, ATTACK_LEN = 12;
constexpr int ICICLE = 144;

// EntityTabish.getIntroScene: camera (x side, y up, z ahead of her), look-at point, shake, fov
struct Key { int tick; double x, y, z, lx, ly, lz; float shake, fov; };
const Key KEYS[] = {
    {0, -16.0, 11.0, -14.0, 0.0, 1.5, 0.0, 0.0f, 70.0f},
    {35, -11.0, 7.0, -7.0, 0.0, 1.5, 0.5, 0.0f, 70.0f},
    {60, -8.0, 4.5, 3.0, 0.0, 1.4, 0.8, 0.0f, 70.0f},
    {85, -1.0, 3.6, 9.0, 0.0, 1.4, 1.2, 0.0f, 70.0f},
    {110, 7.5, 4.0, 5.0, 0.0, 1.4, 0.8, 0.0f, 70.0f},
    {135, 8.0, 3.2, -4.0, 0.0, 1.3, 0.0, 0.0f, 70.0f},
    {160, 2.0, 2.0, 7.0, 0.0, 1.8, 3.0, 0.0f, 48.0f},
    {185, 1.4, 1.9, 6.4, 0.0, 1.8, 3.1, 0.0f, 42.0f},
    {205, -6.5, 3.6, 8.5, 0.0, 2.3, 1.2, 0.0f, 66.0f},
    {235, -6.0, 3.4, 9.0, 0.0, 2.4, 1.2, 0.4f, 66.0f},
    {255, 0.3, 1.0, 9.0, 0.0, 3.7, 3.0, 0.0f, 62.0f},
    {270, 0.2, 0.9, 8.3, 0.0, 4.0, 3.0, 2.2f, 55.0f},
    {295, 0.1, 1.0, 8.6, 0.0, 3.8, 3.0, 1.0f, 58.0f},
    {320, 0.0, 2.5, 10.0, 0.0, 2.5, 2.0, 0.0f, 70.0f},
};
constexpr int NKEYS = int(sizeof(KEYS) / sizeof(KEYS[0]));
constexpr int SCENE_LEN = 320, BLEND_IN = 25, BLEND_OUT = 30, TITLE_TICK = 262;

const Key& key(int i) { return KEYS[std::max(0, std::min(NKEYS - 1, i))]; }
double cr(double p0, double p1, double p2, double p3, double t) {
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * (2.0 * p1 + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}
void sample(double t, double o[8]) {
    int i = 0;
    while (i < NKEYS - 2 && double(key(i + 1).tick) <= t) ++i;
    const Key &a = key(i - 1), &b = key(i), &c = key(i + 1), &d = key(i + 2);
    double span = double(std::max(1, c.tick - b.tick));
    double u = std::max(0.0, std::min(1.0, (t - double(b.tick)) / span));
    o[0] = cr(a.x, b.x, c.x, d.x, u);
    o[1] = cr(a.y, b.y, c.y, d.y, u);
    o[2] = cr(a.z, b.z, c.z, d.z, u);
    o[3] = cr(a.lx, b.lx, c.lx, d.lx, u);
    o[4] = cr(a.ly, b.ly, c.ly, d.ly, u);
    o[5] = cr(a.lz, b.lz, c.lz, d.lz, u);
    o[6] = double(b.shake) + double(c.shake - b.shake) * u;
    o[7] = double(b.fov) + double(c.fov - b.fov) * u;
}
float smooth(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}
float lerpAngle(float a, float b, float t) {
    float d = b - a;
    while (d < -180.0f) d += 360.0f;
    while (d >= 180.0f) d -= 360.0f;
    return a + d * t;
}
int fl(double v) { return int(std::floor(v)); }
} // namespace

Mobs::Mob* Mobs::bossMob() {
    if (bossUid < 0) return nullptr;
    for (Mob& m : mobs)
        if (m.uid == bossUid) return &m;
    return nullptr;
}
const Mobs::Mob* Mobs::bossMob() const {
    if (bossUid < 0) return nullptr;
    for (const Mob& m : mobs)
        if (m.uid == bossUid) return &m;
    return nullptr;
}

bool Mobs::inArena(const Mob& b, double x, double z) const {
    double dx = x - (double(b.arenaX) + 0.5), dz = z - (double(b.arenaZ) + 0.5);
    return dx * dx + dz * dz <= double(b.arenaR * b.arenaR);
}

bool Mobs::arenaLocked(int x, int z) const {
    const Mob* b = bossMob();
    return b && inArena(*b, x + 0.5, z + 0.5);
}

Mobs::BossInfo Mobs::bossInfo() const {
    BossInfo i;
    const Mob* b = bossMob();
    if (!b) return i;
    i.active = true;
    i.cutscene = cutscene > 0;
    i.enraged = b->enraged;
    i.health = std::max(0.0f, float(b->health) / 200.0f);
    i.elapsed = cutsceneLength - cutscene;
    i.left = cutscene;
    i.titleTick = TITLE_TICK;
    i.name = "Tabish";
    i.title = "The Bear Queen";
    return i;
}

void Mobs::closeGate(World& w, Mob& b) {
    if (!b.hasGate) return;
    for (int x = b.gate[0]; x <= b.gate[3]; ++x)
        for (int y = b.gate[1]; y <= b.gate[4]; ++y)
            for (int z = b.gate[2]; z <= b.gate[5]; ++z)
                if (w.getBlockId(x, y, z) == 0) w.setBlockAndMetadataWithNotify(x, y, z, b.gateBlock, b.gateMeta);
    sound::play("random.explode", (b.gate[0] + b.gate[3]) / 2.0, b.gate[1] + 1.0, (b.gate[2] + b.gate[5]) / 2.0, 1.2f, 0.6f);
    shake(1.0f);
}

void Mobs::openGate(World& w, Mob& b) {
    if (!b.hasGate) return;
    for (int x = b.gate[0]; x <= b.gate[3]; ++x)
        for (int y = b.gate[1]; y <= b.gate[4]; ++y)
            for (int z = b.gate[2]; z <= b.gate[5]; ++z)
                if (w.getBlockId(x, y, z) == b.gateBlock) w.setBlockWithNotify(x, y, z, 0);
}

void Mobs::once(Mob& m, const char* anim, int ticks, int fade) {
    m.anim.restart(anim, fade);
    m.busyTicks = ticks;
}

void Mobs::puff(const Mob& m, int n) { // snow off her coat
    if (!gParticles) return;
    for (int i = 0; i < n; ++i)
        gParticles->spawn(Particles::SNOWPOOF, m.x + (jr.nextDouble() - 0.5) * m.width, m.box.minY + jr.nextDouble() * m.height,
                          m.z + (jr.nextDouble() - 0.5) * m.width, 0, 0, 0);
}

void Mobs::startFight(World& w, Mob& b) { // EntityAxeolonBoss.startFight + EntityTabish.onFightStart
    b.awake = true;
    closeGate(w, b);
    b.slamCooldown = 160;
}

void Mobs::resetFight(World& w, Mob& b) { // the player died or left: heal up and go back to sleep
    b.awake = b.enraged = b.inCutscene = false;
    b.target = false;
    b.health = b.prevHealth = 200;
    b.path.clear();
    openGate(w, b);
    b.slamTicks = b.swipeTicks = 0;
    b.moveSpeed = 0.55f;
    b.standing = false;
    b.busyTicks = 0;
}

void Mobs::endFight(bool won) {
    (void)won;
    if (bossUid >= 0) sound::stopMusic();
    if (Mob* b = bossMob()) b->inCutscene = false;
    cineActive = false;
    bossUid = -1;
    cutscene = 0;
    playingEnraged = false;
}

void Mobs::tabishEnrage(Mob& m) { // EntityTabish.onEnrage
    m.enraged = true;
    m.moveSpeed = 0.8f;
    m.slamCooldown = std::min(m.slamCooldown, 60);
    once(m, "roar", ROAR_LEN, 4);
    sound::play("axeolon.mob.tabish.roar", m.x, m.y, m.z, 4.0f, 1.05f);
    shake(1.5f);
}

// AxeolonBossFight.tick
void Mobs::bossTick(World& w, Player& p, bool creative) {
    if (p.dead()) {
        if (Mob* b = bossMob()) {
            resetFight(w, *b);
            endFight(false);
        }
        pendingWake = -1;
        return;
    }
    if (bossUid < 0) {
        Mob* found = nullptr;
        if (pendingWake >= 0) {
            for (Mob& m : mobs)
                if (m.uid == pendingWake) found = &m;
            pendingWake = -1;
        }
        if (!found && !creative) { // findSleepingBoss
            for (Mob& m : mobs) {
                if (m.kind != TABISH || m.dead || m.health <= 0 || m.awake) continue;
                if (!m.arenaSet) { m.arenaX = fl(m.x); m.arenaY = fl(m.y); m.arenaZ = fl(m.z); m.arenaSet = true; }
                double dx = p.posX - m.x, dy = p.posY - m.y, dz = p.posZ - m.z;
                if (inArena(m, p.posX, p.posZ) && std::sqrt(dx * dx + dy * dy + dz * dz) <= 11.0 && canSee(w, m, p)) {
                    found = &m;
                    break;
                }
            }
        }
        if (found && !found->dead && found->health > 0) { // start
            Mob& b = *found;
            bossUid = b.uid;
            cutsceneLength = cutscene = SCENE_LEN;
            b.inCutscene = true;
            // AxeolonCinematic.play: anchored at her feet, facing the way her body faces
            cineActive = true;
            cineTick = 0;
            kick = 0;
            cax = b.x;
            cay = b.box.minY;
            caz = b.z;
            float yaw = b.renderYaw * BPI / 180.0f;
            cfx = double(-MathHelper::sin(yaw));
            cfz = double(MathHelper::cos(yaw));
            playingEnraged = false;
            sound::playMusic("mechanolith");
            earned.push_back(progress::WAKE);
        }
        return;
    }
    Mob* bp = bossMob();
    if (!bp) { endFight(false); return; } // her chunk went away
    Mob& b = *bp;
    if (b.dead || b.health <= 0) {
        bosses::markDefeated("tabish");
        earned.push_back(progress::KING);
        openGate(w, b);
        endFight(true);
        return;
    }
    double dx = p.posX - b.x, dy = p.posY - b.y, dz = p.posZ - b.z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) > double(b.arenaR + 48)) {
        resetFight(w, b);
        endFight(false);
        return;
    }
    if (cutscene > 0) { // the intro
        --cutscene;
        b.inCutscene = cutscene > 0;
        tabishCutscene(w, b, cutsceneLength - cutscene);
        if (cineActive) {
            ++cineTick;
            kick *= 0.9f;
            if (cineTick >= SCENE_LEN) cineActive = false;
        }
        // lookAt: the player's own view turns toward her eyes
        double ldx = b.x - p.posX, ldz = b.z - p.posZ;
        double ldy = (b.y + double(b.height) * 0.75) - p.posY;
        float yaw = float(std::atan2(ldz, ldx) * 180.0 / 3.141592653589793) - 90.0f;
        float pitch = float(-(std::atan2(ldy, std::sqrt(ldx * ldx + ldz * ldz)) * 180.0 / 3.141592653589793));
        float speed = cineActive ? 0.04f : 0.18f;
        float dyaw = yaw - p.rotationYaw;
        while (dyaw < -180.0f) dyaw += 360.0f;
        while (dyaw >= 180.0f) dyaw -= 360.0f;
        p.rotationYaw += dyaw * speed;
        p.rotationPitch += (pitch - p.rotationPitch) * speed;
        if (cutscene == 0) {
            cineActive = false;
            startFight(w, b);
        }
    } else {
        kick *= 0.9f;
    }
    if (!b.enraged && float(b.health) / 200.0f <= 0.5f) tabishEnrage(b);
    if (b.enraged && !playingEnraged) {
        playingEnraged = true;
        sound::playMusic("volatile_reaction");
    }
    if (!creative && !inArena(b, p.posX, p.posZ)) { // the arena wall
        double cx = double(b.arenaX) + 0.5, cz = double(b.arenaZ) + 0.5;
        double ax = p.posX - cx, az = p.posZ - cz;
        double len = std::sqrt(ax * ax + az * az);
        if (len > 0.001 && len < double(b.arenaR + 8)) {
            double r = double(b.arenaR) - 0.4;
            p.setPosition(cx + ax / len * r, p.posY, cz + az / len * r);
            p.motionX = -ax / len * 0.2;
            p.motionZ = -az / len * 0.2;
        }
    }
}

void Mobs::tabishCutscene(World& w, Mob& m, int t) { // EntityTabish.cutsceneTick
    (void)w;
    if (t < 170 && t % 40 == 0) sound::play("axeolon.mob.tabish.snore", m.x, m.y, m.z, 0.9f, 1.0f);
    if (t == 170) {
        once(m, "wake", 20, 8);
        sound::play("axeolon.mob.tabish.growl", m.x, m.y, m.z, 1.5f, 0.9f);
    }
    if (t == 190) {
        m.standing = true;
        once(m, "stand_up", 22, 6);
    }
    if (t == 212) once(m, "shake", SHAKE_LEN, 4);
    if (t == 212 || t == 220 || t == 228)
        sound::play("axeolon.mob.tabish.shake", m.x, m.y, m.z, 1.2f, 0.9f + jr.nextFloat() * 0.2f);
    if (t >= 214 && t <= 238 && t % 3 == 0) puff(m, 6);
    if (t == 258) {
        once(m, "roar", ROAR_LEN, 4);
        sound::play("axeolon.mob.tabish.roar", m.x, m.y, m.z, 4.0f, 1.0f);
        shake(3.0f);
    }
    if (t >= 260 && t <= 295 && t % 2 == 0 && gParticles) { // roarBreath
        float yaw = m.renderYaw * BPI / 180.0f;
        double fx = double(-MathHelper::sin(yaw)), fz = double(MathHelper::cos(yaw));
        double mx = m.x + fx * 3.6, my = m.box.minY + 3.4, mz = m.z + fz * 3.6;
        for (int i = 0; i < 4; ++i)
            gParticles->spawn(Particles::SMOKE, mx, my, mz, fx * 0.4 + jr.nextGaussian() * 0.05, 0.05, fz * 0.4 + jr.nextGaussian() * 0.05);
        puff(m, 3);
    }
}

void Mobs::tabishAttack(Mob& m, float dist, const Player& p) { // EntityTabish.attackEntity
    if (m.slamTicks > 0 || m.swipeTicks > 0) return;
    if (m.attackTime <= 0 && double(dist) < 3.8 && p.boundingBox.maxY > m.box.minY && p.boundingBox.minY < m.box.maxY) {
        m.attackTime = m.enraged ? 16 : 24;
        once(m, "attack", ATTACK_LEN, 2);
        m.swipeTicks = SWIPE_HIT;
        sound::play("axeolon.mob.tabish.swipe", m.x, m.y, m.z, 1.5f, 0.9f + jr.nextFloat() * 0.2f);
    }
}

// EntityTabish.onUpdate (her fight) + the snoring
void Mobs::tabishUpdate(World& w, Mob& m, Player& p, ItemEntities& items) {
    (void)items;
    if (!m.arenaSet) { m.arenaX = fl(m.x); m.arenaY = fl(m.y); m.arenaZ = fl(m.z); m.arenaSet = true; }
    if (!m.awake && !m.inCutscene && m.ticksExisted % 60 == 0)
        sound::play("axeolon.mob.tabish.snore", m.x, m.y, m.z, 0.7f, 1.0f);
    if (!m.awake || m.inCutscene || m.health <= 0) return;
    if (m.swipeTicks > 0 && --m.swipeTicks == 0 && !p.dead()) { // swipeLands: a wide arc in front of her
        float yaw = m.renderYaw * BPI / 180.0f;
        double fx = double(-MathHelper::sin(yaw)), fz = double(MathHelper::cos(yaw));
        double dx = p.posX - m.x, dz = p.posZ - m.z;
        double d = std::sqrt(dx * dx + dz * dz);
        bool inBox = p.boundingBox.intersects(m.box.expand(3.5, 1.0, 3.5));
        if (inBox && d <= 4.4 && !(d > 0.5 && (dx * fx + dz * fz) / d < 0.2)) {
            if (p.attack(m.enraged ? 10 : 8, false, m.x, m.z, true) && !p.immovable()) {
                double k = std::max(d, 0.5);
                p.motionX += dx / k * 0.7;
                p.motionY += 0.3;
                p.motionZ += dz / k * 0.7;
            }
        }
    }
    if (m.slamTicks > 0) {
        if (--m.slamTicks == SLAM_LEN - SLAM_HIT) { // slamLands
            int bx = fl(m.x), by = fl(m.box.minY), bz = fl(m.z);
            sound::play("axeolon.mob.tabish.slam", m.x, m.y, m.z, 3.0f, 0.9f + jr.nextFloat() * 0.2f);
            if (gParticles)
                for (int i = 0; i < 48; ++i) { // a ring of snow and dust
                    double a = double(i) / 48.0 * 3.141592653589793 * 2.0;
                    double r = 1.5 + jr.nextDouble() * 2.5;
                    gParticles->spawn(i % 3 == 0 ? Particles::EXPLODE : Particles::SNOWPOOF, m.x + std::cos(a) * r, m.box.minY + 0.2,
                                      m.z + std::sin(a) * r, std::cos(a) * 0.3, 0.15, std::sin(a) * 0.3);
                }
            // BlockIcicle.shakeCeiling
            float chance = m.enraged ? 0.55f : 0.3f;
            int rad = m.arenaR, cy = by + 2;
            for (int x = bx - rad; x <= bx + rad; ++x)
                for (int z = bz - rad; z <= bz + rad; ++z) {
                    if ((x - bx) * (x - bx) + (z - bz) * (z - bz) > rad * rad) continue;
                    for (int y = cy; y < cy + 16; ++y)
                        if (w.getBlockId(x, y, z) == ICICLE && (w.getBlockMetadata(x, y, z) & 1) == 0) {
                            if (jr.nextFloat() < chance) knockIcicleLoose(w, x, y, z);
                            break;
                        }
                }
            if (!p.dead() && p.boundingBox.intersects(m.box.expand(7.0, 2.0, 7.0))) {
                double dx = p.posX - m.x, dz = p.posZ - m.z;
                double d = std::sqrt(dx * dx + dz * dz);
                if (d <= 7.0 && !p.invulnerable) {
                    if (d < 0.01) { dx = 1.0; d = 1.0; }
                    if (d < 6.0) shake(1.2f);
                    if (p.onGround) { // jumped it otherwise
                        double push = 1.4 * (1.0 - d / 9.0);
                        if (!p.immovable()) { // AxeolonCharms.immovable
                            p.motionX += dx / d * push;
                            p.motionY += 0.55;
                            p.motionZ += dz / d * push;
                        }
                        p.attack(m.enraged ? 5 : 4, false); // no attacker: shields don't help
                    }
                }
            }
        }
        return;
    }
    if (m.slamCooldown > 0) {
        --m.slamCooldown;
    } else if (m.target && m.onGround && m.busyTicks <= 0 && m.swipeTicks <= 0 && !p.dead()) {
        double dx = p.posX - m.x, dy = p.posY - m.y, dz = p.posZ - m.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) < 10.0) {
            m.slamTicks = SLAM_LEN;
            m.slamCooldown = (m.enraged ? 110 : 200) + jr.nextInt(40);
            once(m, "slam", SLAM_LEN, 3);
            sound::play("axeolon.mob.tabish.growl", m.x, m.y, m.z, 3.0f, 1.1f);
        }
    }
}

// ---------------------------------------------------------------- the intro camera

bool Mobs::cinematic(float partial, double px, double py, double pz, float pyaw, float ppitch, double& cx, double& cy,
                     double& cz, float& yaw, float& pitch, float& roll, float& fov) {
    if (!cineActive) return false;
    double t = double(cineTick) + double(partial);
    double s[8];
    sample(t, s);
    auto wx = [&](double x, double z) { return cax + cfz * x + cfx * z; };
    auto wz = [&](double x, double z) { return caz - cfx * x + cfz * z; };
    double x = wx(s[0], s[2]), y = cay + s[1], z = wz(s[0], s[2]);
    double lx = wx(s[3], s[5]), ly = cay + s[4], lz = wz(s[3], s[5]);
    double dx = lx - x, dy = ly - y, dz = lz - z;
    float cyaw = float(std::atan2(dz, dx) * 180.0 / 3.141592653589793) - 90.0f;
    float cpitch = float(-(std::atan2(dy, std::sqrt(dx * dx + dz * dz)) * 180.0 / 3.141592653589793));
    float f = float(s[7]);
    float wgt = 1.0f;
    if (t < double(BLEND_IN)) wgt = smooth(float(t / double(BLEND_IN)));
    else if (t > double(SCENE_LEN - BLEND_OUT)) wgt = smooth(float((double(SCENE_LEN) - t) / double(BLEND_OUT)));
    cx = px + (x - px) * double(wgt);
    cy = py + (y - py) * double(wgt);
    cz = pz + (z - pz) * double(wgt);
    yaw = lerpAngle(pyaw, cyaw, wgt);
    pitch = ppitch + (cpitch - ppitch) * wgt;
    fov = 70.0f + (f - 70.0f) * wgt;
    roll = 0.0f;
    float sh = (float(s[6]) + kick) * wgt;
    if (sh > 0.001f) {
        JavaRandom r(int64_t(t * 3.0));
        cx += (r.nextDouble() - 0.5) * double(sh) * 0.3;
        cy += (r.nextDouble() - 0.5) * double(sh) * 0.3;
        cz += (r.nextDouble() - 0.5) * double(sh) * 0.3;
        roll = (r.nextFloat() - 0.5f) * sh * 4.0f;
        yaw += (r.nextFloat() - 0.5f) * sh * 2.0f;
        pitch += (r.nextFloat() - 0.5f) * sh * 2.0f;
    }
    return true;
}

float Mobs::cinemaBars() const {
    if (!cineActive) return 0.0f;
    float t = float(cineTick);
    return std::min(smooth(t / 15.0f), smooth((float(SCENE_LEN) - t) / 15.0f));
}

// ---------------------------------------------------------------- falling icicles

void Mobs::knockIcicleLoose(World& w, int x, int y, int z) { // BlockIcicle.knockLoose + fall
    while (w.getBlockId(x, y + 1, z) == ICICLE && (w.getBlockMetadata(x, y + 1, z) & 1) == 0) ++y; // find the base
    if (w.getBlockId(x, y, z) != ICICLE || (w.getBlockMetadata(x, y, z) & 1) != 0) return;
    sound::play("random.glass", x + 0.5, y + 0.5, z + 0.5, 0.6f, 1.6f);
    // the whole hanging icicle comes down; the lowest piece is the tip
    int bottom = y;
    while (w.getBlockId(x, bottom - 1, z) == ICICLE && (w.getBlockMetadata(x, bottom - 1, z) & 1) == 0) --bottom;
    for (int yy = y; yy >= bottom; --yy) {
        w.setBlockWithNotify(x, yy, z, 0);
        Icicle ic{x + 0.5, double(yy), z + 0.5, double(yy), 0.0, double(yy), 0, yy == bottom, false};
        icicles.push_back(ic);
    }
}

void Mobs::tickIcicles(World& w, Player& p, ItemEntities& items) { // EntityFallingIcicle.onUpdate
    std::vector<AABB> boxes;
    for (Icicle& ic : icicles) {
        ic.py = ic.y;
        ++ic.age;
        ic.my -= 0.05;
        ic.my *= 0.98;
        AABB bb{ic.x - 0.2, ic.y, ic.z - 0.2, ic.x + 0.2, ic.y + 0.9, ic.z + 0.2};
        double dy = ic.my, oy = dy;
        boxes.clear();
        Physics::collidingBoxes(w, bb.addCoord(0, dy, 0), boxes);
        for (auto& b : boxes) dy = b.calculateYOffset(bb, dy);
        ic.y += dy;
        bool onGround = oy != dy && oy < 0.0;
        if (oy != dy) ic.my = 0.0;
        float fell = float(ic.startY - ic.y);
        int dmg = std::min(18, std::max(2, int(std::ceil(double(fell * 1.5f))) + (ic.tip ? 2 : 0)));
        AABB hit{ic.x - 0.3, ic.y, ic.z - 0.3, ic.x + 0.3, ic.y + 0.9, ic.z + 0.3};
        bool struck = false;
        if (fell > 0.5f) {
            if (!p.dead() && p.boundingBox.intersects(hit)) {
                p.attack(dmg, false, ic.x, ic.z, true);
                struck = true;
            }
            for (Mob& m : mobs)
                if (!m.dead && m.health > 0 && m.box.intersects(hit)) {
                    hurt(m, dmg, false, ic.x, ic.z, &items, &w);
                    struck = true;
                }
        }
        if (struck || onGround || ic.age > 200) { // shatter
            sound::play("random.glass", ic.x, ic.y, ic.z, 0.8f, 1.3f + jr.nextFloat() * 0.3f);
            if (gParticles)
                for (int i = 0; i < 8; ++i)
                    gParticles->spawn(Particles::SNOWPOOF, ic.x + (jr.nextDouble() - 0.5) * 0.5, ic.y + jr.nextDouble() * 0.6,
                                      ic.z + (jr.nextDouble() - 0.5) * 0.5, 0, 0, 0);
            if (ic.tip && jr.nextInt(3) == 0) {
                ItemStack st;
                st.id = ICICLE;
                st.count = 1;
                items.spawn(ic.x, ic.y, ic.z, st, 10);
            }
            ic.dead = true;
        }
    }
    icicles.erase(std::remove_if(icicles.begin(), icicles.end(), [](const Icicle& i) { return i.dead; }), icicles.end());
}
