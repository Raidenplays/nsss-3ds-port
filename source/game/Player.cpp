#include "Player.h"
#include <algorithm>
#include <cmath>
#include "world/BlockTable.h"
#include "world/World.h"
#include "world/MathHelper.h"
#include "world/BlockProps.h"
#include "game/Inventory.h"
#include "game/ItemTable.h"
#include "game/Survival.h"
#include "game/Inventory.h"
#include "game/Survival.h"

namespace {

inline float mhSin(float v) { return MathHelper::sin(v); }
inline float mhCos(float v) { return MathHelper::cos(v); }
inline int floorD(double v) { return int(std::floor(v)); }

constexpr int LADDER = 65, SOUL_SAND = 88, CACTUS = 81, FIRE = 51, ICICLE = 144;
constexpr int CHARM_GLIDE = 1007, CHARM_SWIFT = 1008;
constexpr float GLIDE_FALL_SPEED = -0.15f;
constexpr int CHARM_BEAR = 1045, ARMOUR_FIRST = 298, ARMOUR_LAST = 317;

// AxeolonArmor: points by material (leather, chain, iron, diamond, gold) and slot (helmet .. boots)
constexpr int ARMOUR_POINTS[5][4] = {{1, 3, 2, 1}, {2, 5, 4, 1}, {2, 6, 5, 2}, {3, 8, 6, 3}, {2, 5, 3, 1}};
constexpr float ARMOUR_TOUGH[5] = {0.0f, 0.0f, 0.0f, 2.0f, 0.0f};

bool isArmour(int id) { return id >= ARMOUR_FIRST && id <= ARMOUR_LAST; }

} // namespace

int Player::healthCap() const {
    if (inv)
        for (const ItemStack& c : inv->charms)
            if (!c.empty() && c.id == CHARM_BEAR) return 24; // AxeolonCharms.BEAR_HEALTH
    return 20;
}

ItemStack* Player::findCharm(int id) {
    if (!inv) return nullptr;
    for (ItemStack& c : inv->charms)
        if (!c.empty() && c.id == id) return &c;
    return nullptr;
}

bool Player::immovable() const {
    return const_cast<Player*>(this)->findCharm(CHARM_BEAR) != nullptr;
}

void Player::wearCharm(ItemStack* c, int amount) {
    if (!c || invulnerable) return; // not in creative
    c->damage += amount;           // ItemStack.damageItem
    if (c->damage > survival::maxDamage(c->id)) {
        const ItemInfo* info = itemInfo(c->id, 0);
        messages.push_back(std::string("Your ") + (info ? info->name : "charm") + " crumbled to dust.");
        c->clear();
        sounds.push_back({"random.break", 0.8f, 0.8f + rand.nextFloat() * 0.4f});
    }
}

int Player::armourValue() const {
    int p = 0;
    if (inv)
        for (const ItemStack& a : inv->armour)
            if (!a.empty() && isArmour(a.id)) p += ARMOUR_POINTS[(a.id - ARMOUR_FIRST) / 4][(a.id - ARMOUR_FIRST) % 4];
    return p;
}

void Player::heal(int n) { // EntityLiving.heal
    if (health > 0) {
        health += n;
        if (health > healthCap()) health = healthCap();
        hurtResistantTime = 10; // field_9366_o / 2
    }
}

// EntityPlayer.damageEntity: modern armour (AxeolonArmor.reduce + wear), then EntityLiving's
void Player::damageEntity(int n, bool bypass) {
    if (bypass || n <= 0 || !inv) {
        health -= n;
        return;
    }
    float points = float(armourValue()), tough = 0.0f;
    for (const ItemStack& a : inv->armour)
        if (!a.empty() && isArmour(a.id)) tough += ARMOUR_TOUGH[(a.id - ARMOUR_FIRST) / 4];
    float dmg = float(n);
    float eff = std::min(20.0f, std::max(points / 5.0f, points - dmg / (2.0f + tough / 4.0f)));
    float left = dmg * (1.0f - eff / 25.0f);
    int v = int(std::floor(left * 25.0f + 0.5f)) + damageRemainder; // Math.round(float)
    // AxeolonArmor.wear: InventoryPlayer.damageArmor(max(1, damage / 4))
    int wear = std::max(1, n / 4);
    for (ItemStack& a : inv->armour)
        if (!a.empty() && isArmour(a.id)) {
            a.damage = int16_t(a.damage + wear);
            if (a.damage > survival::maxDamage(a.id)) a.clear(); // TODO(port): "random.break"
        }
    damageRemainder = v % 25;
    health -= v / 25;
}

// EntityLiving.attackEntityFrom (the player isn't on a server, so no multiplayer branch)
int Player::shieldBlock(int damage, double fromX, double fromZ) {
    if (!blocking || !inv || damage <= 0) return damage;
    ItemStack* off = inv->offhand();
    if (!off || off->empty() || !((off->id >= 1035 && off->id <= 1038) || off->id == 1042)) return damage;
    // in front? within 60 degrees either side of where the player faces
    double dx = fromX - posX, dz = fromZ - posZ;
    double yaw = double(rotationYaw) * 3.141592653589793 / 180.0;
    double fx = -std::sin(yaw), fz = std::cos(yaw);
    double len = std::sqrt(dx * dx + dz * dz);
    if (len < 0.01 || (dx * fx + dz * fz) / len < 0.5) return damage;
    sounds.push_back({"step.wood", 1.0f, 0.6f + rand.nextFloat() * 0.2f});
    int through = off->id == 1042 && len < 3.0 ? damage / 2 : 0; // the light pelt only halves a close hit
    if (!invulnerable) {
        off->damage = int16_t(off->damage + (damage - through));
        if (off->damage > survival::maxDamage(off->id)) {
            off->clear();
            sounds.push_back({"random.break", 0.8f, 0.8f + rand.nextFloat() * 0.4f});
        }
    }
    return through;
}

bool Player::attack(int dmg, bool bypass, double fromX, double fromZ, bool hasSource, bool fromMonster) {
    if (invulnerable) return false;
    if (hasSource) { // EntityPlayer.attackEntityFrom: AxeolonBattle.block first
        dmg = shieldBlock(dmg, fromX, fromZ);
        if (dmg <= 0 && blocking) return false;
    }
    if (health <= 0) return false;
    if (fromMonster) { // the difficulty
        if (difficulty == 0) dmg = 0;
        if (difficulty == 1) dmg = dmg / 3 + 1;
        if (difficulty == 3) dmg = dmg * 3 / 2;
        if (dmg == 0) return false;
    }
    bool fresh = true;
    if (float(hurtResistantTime) > 20.0f / 2.0f) {
        if (dmg <= lastDamage) return false;
        damageEntity(dmg - lastDamage, bypass);
        lastDamage = dmg;
        fresh = false;
    } else {
        lastDamage = dmg;
        prevHealth = health;
        hurtResistantTime = 20;
        damageEntity(dmg, bypass);
        hurtTime = maxHurtTime = 10;
    }
    attackedAtYaw = 0.0f;
    if (fresh) {
        if (hasSource) {
            double dx = fromX - posX, dz = fromZ - posZ;
            while (dx * dx + dz * dz < 1.0e-4) {
                dx = (rand.nextDouble() - rand.nextDouble()) * 0.01;
                dz = (rand.nextDouble() - rand.nextDouble()) * 0.01;
            }
            attackedAtYaw = float(std::atan2(dz, dx) * 180.0 / double(3.1415927f)) - rotationYaw;
            // knockBack (EntityPlayer: not with the Bear Charm)
            if (!immovable()) {
            float len = float(std::sqrt(dx * dx + dz * dz));
            motionX /= 2.0; motionY /= 2.0; motionZ /= 2.0;
            motionX -= dx / double(len) * double(0.4f);
            motionY += double(0.4f);
            motionZ -= dz / double(len) * double(0.4f);
            if (motionY > double(0.4f)) motionY = double(0.4f);
            }
        } else {
            attackedAtYaw = float(int(rand.nextDouble() * 2.0) * 180);
        }
    }
    if (health <= 0) {
        if (fresh) sounds.push_back({"random.hurt", 1.0f, (rand.nextFloat() - rand.nextFloat()) * 0.2f + 1.0f});
        // EntityPlayer.onDeath: a small hop, no sideways push without an attacker
        motionY = double(0.1f);
        if (hasSource) {
            motionX = double(-MathHelper::cos((attackedAtYaw + rotationYaw) * 3.1415927f / 180.0f) * 0.1f);
            motionZ = double(-MathHelper::sin((attackedAtYaw + rotationYaw) * 3.1415927f / 180.0f) * 0.1f);
        } else {
            motionX = motionZ = 0.0;
        }
    } else if (fresh) {
        sounds.push_back({"random.hurt", 1.0f, (rand.nextFloat() - rand.nextFloat()) * 0.2f + 1.0f});
    }
    return true;
}

// EntityLiving.fall (Axeolon: a rising icicle's point hurts twice as much, from 2 blocks)
void Player::fall(float d) {
    const World& w = *world;
    int dmg = int(std::ceil(double(d - 3.0f)));
    int ix = floorD(posX), iy = floorD(boundingBox.minY - 0.2), iz = floorD(posZ);
    if (w.getBlockId(ix, iy, iz) == ICICLE && (w.getBlockMetadata(ix, iy, iz) & 1) == 1) {
        int up = w.getBlockId(ix, iy + 1, iz); // shape == TIP: the stack doesn't go on (or meet another)
        if (up != ICICLE) dmg = int(std::ceil(double((d - 2.0f) * 2.0f)));
    }
    if (dmg > 0) {
        attack(dmg, true);
        int below = w.getBlockId(floorD(posX), floorD(posY - double(0.2f) - double(YOFFSET)), floorD(posZ));
        if (below > 0) sounds.push_back({BS[below].step, BS[below].volume * 0.5f, BS[below].pitch * 0.75f});
    }
}

bool Player::isEyeIn(const World& w, bool lava) const {
    double eye = posY; // posY is already at the eyes (getEyeHeight is 0 for the SP player)
    int x = floorD(posX), y = floorD(eye), z = floorD(posZ);
    if (y < 0 || y >= 128) return false;
    int id = w.getBlockId(x, y, z);
    if (lava ? (id != 10 && id != 11) : (id != 8 && id != 9)) return false;
    int m = w.getBlockMetadata(x, y, z);
    if (m >= 8) m = 0;
    float h = float(m + 1) / 9.0f - 1.0f / 9.0f;
    return eye < double(float(y + 1) - h);
}

void Player::resetAfterRespawn() {
    health = prevHealth = 20;
    hurtResistantTime = lastDamage = hurtTime = maxHurtTime = deathTime = attackTime = 0;
    air = MAX_AIR;
    fire = 0;
    fallDistance = 0;
    lastFall = 0;
    motionX = motionY = motionZ = 0;
    mana = MAX_MANA;
    manaRegenTimer = 0;
    damageRemainder = 0;
}

void Player::setPosition(double x, double y, double z) {
    posX = x;
    posY = y;
    posZ = z;
    float hw = WIDTH / 2.0f;
    boundingBox = {x - hw, y - YOFFSET + ySize, z - hw, x + hw, y - YOFFSET + ySize + HEIGHT, z + hw};
}

void Player::place(double x, double y, double z, float yaw, float pitch) {
    prevPosX = posX = x;
    prevPosY = posY = y;
    prevPosZ = posZ = z;
    rotationYaw = yaw;
    rotationPitch = pitch;
    ySize = 0.0f;
    setPosition(x, y, z);
}

bool Player::handleWaterMovement(const World& w) {
    return Physics::waterAcceleration(w, boundingBox.expand(0.0, double(-0.4f), 0.0), motionX, motionY, motionZ);
}

bool Player::handleLavaMovement(const World& w) const {
    return Physics::isMaterialInBB(w, boundingBox.expand(double(-0.1f), double(-0.4f), double(-0.1f)), true);
}

bool Player::isOnLadder(const World& w) const {
    int x = floorD(posX), y = floorD(boundingBox.minY), z = floorD(posZ);
    return w.getBlockId(x, y, z) == LADDER || w.getBlockId(x, y + 1, z) == LADDER;
}

bool Player::isOffsetPositionInLiquid(const World& w, double x, double y, double z) {
    AABB b = boundingBox.offset(x, y, z);
    Physics::collidingBoxes(w, b, boxes);
    return boxes.empty() ? !Physics::isAnyLiquid(w, b) : false;
}

void Player::moveFlying(float s, float f, float accel) {
    float l = float(std::sqrt(double(s * s + f * f)));
    if (l >= 0.01f) {
        if (l < 1.0f) l = 1.0f;
        l = accel / l;
        s *= l;
        f *= l;
        float sn = mhSin(rotationYaw * 3.1415927f / 180.0f);
        float cs = mhCos(rotationYaw * 3.1415927f / 180.0f);
        motionX += double(s * cs - f * sn);
        motionZ += double(f * cs + s * sn);
    }
}

void Player::moveEntity(const World& w, double dx, double dy, double dz) {
    double x0 = posX, z0 = posZ;
    double odx = dx, ody = dy, odz = dz;
    AABB start = boundingBox;
    bool sneakEdge = onGround && isSneaking();
    if (sneakEdge) { // sneaking doesn't walk off edges
        const double s = 0.05;
        for (; dx != 0.0; odx = dx) {
            Physics::collidingBoxes(w, boundingBox.offset(dx, -1.0, 0.0), boxes);
            if (!boxes.empty()) break;
            if (dx < s && dx >= -s) dx = 0.0;
            else if (dx > 0.0) dx -= s;
            else dx += s;
        }
        for (; dz != 0.0; odz = dz) {
            Physics::collidingBoxes(w, boundingBox.offset(0.0, -1.0, dz), boxes);
            if (!boxes.empty()) break;
            if (dz < s && dz >= -s) dz = 0.0;
            else if (dz > 0.0) dz -= s;
            else dz += s;
        }
    }

    Physics::collidingBoxes(w, boundingBox.addCoord(dx, dy, dz), boxes);
    for (const AABB& b : boxes) dy = b.calculateYOffset(boundingBox, dy);
    boundingBox.move(0.0, dy, 0.0);
    bool stepOk = onGround || (ody != dy && ody < 0.0);
    for (const AABB& b : boxes) dx = b.calculateXOffset(boundingBox, dx);
    boundingBox.move(dx, 0.0, 0.0);
    for (const AABB& b : boxes) dz = b.calculateZOffset(boundingBox, dz);
    boundingBox.move(0.0, 0.0, dz);

    if (STEP > 0.0f && stepOk && ySize < 0.05f && (odx != dx || odz != dz)) {
        double sx = dx, sy = dy, sz = dz;
        dx = odx;
        dy = double(STEP);
        dz = odz;
        AABB moved = boundingBox;
        boundingBox = start;
        Physics::collidingBoxes(w, boundingBox.addCoord(odx, dy, odz), boxes);
        for (const AABB& b : boxes) dy = b.calculateYOffset(boundingBox, dy);
        boundingBox.move(0.0, dy, 0.0);
        for (const AABB& b : boxes) dx = b.calculateXOffset(boundingBox, dx);
        boundingBox.move(dx, 0.0, 0.0);
        for (const AABB& b : boxes) dz = b.calculateZOffset(boundingBox, dz);
        boundingBox.move(0.0, 0.0, dz);
        if (sx * sx + sz * sz >= dx * dx + dz * dz) {
            dx = sx;
            dy = sy;
            dz = sz;
            boundingBox = moved;
        } else {
            ySize = float(double(ySize) + 0.5);
        }
    }

    posX = (boundingBox.minX + boundingBox.maxX) / 2.0;
    posY = boundingBox.minY + double(YOFFSET) - double(ySize);
    posZ = (boundingBox.minZ + boundingBox.maxZ) / 2.0;
    isCollidedHorizontally = odx != dx || odz != dz;
    isCollidedVertically = ody != dy;
    onGround = ody != dy && ody < 0.0;
    // updateFallState
    if (onGround) {
        if (fallDistance > 0.0f) {
            lastFall = fallDistance;
            fall(fallDistance);
            fallDistance = 0.0f;
        }
    } else if (dy < 0.0) {
        fallDistance = float(double(fallDistance) - dy);
    }
    if (odx != dx) motionX = 0.0;
    if (ody != dy) motionY = 0.0;
    if (odz != dz) motionZ = 0.0;

    double wx = posX - x0, wz = posZ - z0;
    if (!sneakEdge) {
        distanceWalkedModified = float(double(distanceWalkedModified) + double(float(std::sqrt(wx * wx + wz * wz))) * 0.6);
        int sx = floorD(posX), sy = floorD(posY - double(0.2f) - double(YOFFSET)), sz = floorD(posZ);
        int id = (sy >= 0 && sy < 128) ? w.getBlockId(sx, sy, sz) : 0;
        if (distanceWalkedModified > float(nextStepDistance) && id > 0) { // footsteps
            ++nextStepDistance;
            if (w.getBlockId(sx, sy + 1, sz) == 78) // a snow layer on top
                sounds.push_back({BS[78].step, BS[78].volume * 0.15f, BS[78].pitch});
            else if (!matLiquid(BP[id].mat))
                sounds.push_back({BS[id].step, BS[id].volume * 0.15f, BS[id].pitch});
        }
    }

    // onEntityCollidedWithBlock (soul sand slows you down)
    int bx0 = floorD(boundingBox.minX), by0 = floorD(boundingBox.minY), bz0 = floorD(boundingBox.minZ);
    int bx1 = floorD(boundingBox.maxX), by1 = floorD(boundingBox.maxY), bz1 = floorD(boundingBox.maxZ);
    bool loaded = by1 >= 0 && by0 < 256;
    for (int cx = bx0 >> 4; loaded && cx <= bx1 >> 4; ++cx)
        for (int cz = bz0 >> 4; cz <= bz1 >> 4; ++cz) loaded &= w.isChunkLoaded(cx, cz);
    if (loaded)
        for (int x = bx0; x <= bx1; ++x)
            for (int y = by0; y <= by1; ++y)
                for (int z = bz0; z <= bz1; ++z)
                {
                    int id = w.getBlockId(x, y, z);
                    if (id == SOUL_SAND) {
                        motionX *= 0.4;
                        motionZ *= 0.4;
                    } else if (id == CACTUS) {
                        attack(1, false);
                    } else if (id == 90) {
                        setInPortal(); // BlockPortal.onEntityCollidedWithBlock
                    }
                }

    ySize *= 0.4f;
    bool wet = handleWaterMovement(w);
    // World.isBoundingBoxBurning: fire or lava anywhere in the box
    bool burning = false;
    {
        int x0 = floorD(boundingBox.minX), x1 = floorD(boundingBox.maxX + 1.0);
        int y0 = floorD(boundingBox.minY), y1 = floorD(boundingBox.maxY + 1.0);
        int z0 = floorD(boundingBox.minZ), z1 = floorD(boundingBox.maxZ + 1.0);
        bool have = y1 > 0 && y0 < 256;
        for (int cx = x0 >> 4; have && cx <= (x1 - 1) >> 4; ++cx)
            for (int cz = z0 >> 4; cz <= (z1 - 1) >> 4; ++cz) have &= w.isChunkLoaded(cx, cz);
        for (int x = x0; have && !burning && x < x1; ++x)
            for (int y = y0; !burning && y < y1; ++y)
                for (int z = z0; z < z1; ++z) {
                    int id = w.getBlockId(x, y, z);
                    if (id == FIRE || id == 10 || id == 11) { burning = true; break; }
                }
    }
    if (burning) {
        attack(1, false); // dealFireDamage
        if (!wet) {
            ++fire;
            if (fire == 0) fire = 300;
        }
    } else if (fire <= 0) {
        fire = -fireResistance;
    }
    if (wet && fire > 0) {
        sounds.push_back({"random.fizz", 0.7f, 1.6f + (rand.nextFloat() - rand.nextFloat()) * 0.4f});
        fire = -fireResistance;
    }
}

void Player::moveEntityWithHeading(const World& w, float s, float f) {
    if (handleWaterMovement(w)) {
        double y0 = posY;
        moveFlying(s, f, 0.02f);
        moveEntity(w, motionX, motionY, motionZ);
        motionX *= double(0.8f);
        motionY *= double(0.8f);
        motionZ *= double(0.8f);
        motionY -= 0.02;
        if (isCollidedHorizontally && isOffsetPositionInLiquid(w, motionX, motionY + double(0.6f) - posY + y0, motionZ))
            motionY = double(0.3f);
    } else if (handleLavaMovement(w)) {
        double y0 = posY;
        moveFlying(s, f, 0.02f);
        moveEntity(w, motionX, motionY, motionZ);
        motionX *= 0.5;
        motionY *= 0.5;
        motionZ *= 0.5;
        motionY -= 0.02;
        if (isCollidedHorizontally && isOffsetPositionInLiquid(w, motionX, motionY + double(0.6f) - posY + y0, motionZ))
            motionY = double(0.3f);
    } else {
        auto friction = [&]() {
            float fr = 0.91f;
            if (onGround) {
                fr = 546.0f * 0.1f * 0.1f * 0.1f;
                int id = w.getBlockId(floorD(posX), floorD(boundingBox.minY) - 1, floorD(posZ));
                if (id > 0) fr = Physics::slipperiness(id) * 0.91f;
            }
            return fr;
        };
        float fr = friction();
        float k = 0.16277136f / (fr * fr * fr);
        float ground = 0.1f * speedMultiplier * (blocking ? 0.35f : 1.0f); // EntityPlayer.getGroundAcceleration
        moveFlying(s, f, onGround ? ground * k : 0.02f);
        fr = friction();
        if (isOnLadder(w)) {
            fallDistance = 0.0f;
            if (motionY < -0.15) motionY = -0.15;
        }
        moveEntity(w, motionX, motionY, motionZ);
        if (isCollidedHorizontally && isOnLadder(w)) motionY = 0.2;
        motionY -= 0.08;
        motionY *= double(0.98f);
        motionX *= double(fr);
        motionZ *= double(fr);
    }
}

void Player::tick(const World& w, const PlayerInput& in) {
    world = &w;
    // EntityLiving.onEntityUpdate: the arm swing
    prevSwingProgress = swingProgress;
    if (isSwinging) {
        if (++swingProgressInt >= 8) {
            swingProgressInt = 0;
            isSwinging = false;
        }
    } else {
        swingProgressInt = 0;
    }
    swingProgress = float(swingProgressInt) / 8.0f;
    // EntityPlayerSP.onLivingUpdate: the portal's pull
    prevTimeInPortal = timeInPortal;
    if (inPortal) {
        if (timeInPortal == 0.0f) sounds.push_back({"portal.trigger", 1.0f, rand.nextFloat() * 0.4f + 0.8f});
        timeInPortal += 0.0125f;
        if (timeInPortal >= 1.0f) {
            timeInPortal = 1.0f;
            portalCooldown = 10;
            sounds.push_back({"portal.travel", 1.0f, rand.nextFloat() * 0.4f + 0.8f});
            travelNow = true;
        }
        inPortal = false;
    } else {
        if (timeInPortal > 0.0f) timeInPortal -= 0.05f;
        if (timeInPortal < 0.0f) timeInPortal = 0.0f;
    }
    if (portalCooldown > 0) --portalCooldown;
    prevDistanceWalked = distanceWalkedModified;
    prevCameraYaw = cameraYaw;
    prevCameraPitch = cameraPitch;
    // Entity.onEntityUpdate
    ++ticksExisted;
    prevPosX = posX;
    prevPosY = posY;
    prevPosZ = posZ;
    if (handleWaterMovement(w)) {
        fallDistance = 0.0f;
        inWater = true;
        fire = 0;
    } else {
        inWater = false;
    }
    if (fire > 0) {
        if (fire % 20 == 0) attack(1, true); // burning ignores armour
        --fire;
    }
    if (handleLavaMovement(w)) { // setOnFireFromLava
        if (!invulnerable) {
            attack(4, false);
            fire = 600;
        }
    }
    if (posY < -64.0) attack(4, true); // kill(): the void
    // EntityLiving.onEntityUpdate
    if (health > 0) { // suffocating in a solid block
        int x = floorD(posX), y = floorD(posY), z = floorD(posZ);
        int id = (y >= 0 && y < 128) ? w.getBlockId(x, y, z) : 0;
        if (id && BP[id].opaqueCube) attack(1, true);
    }
    if (health > 0 && isInsideOfWater(w)) {
        --air;
        if (air == -20) {
            air = 0;
            attack(2, true); // drowning
        }
        fire = 0;
    } else {
        air = MAX_AIR;
    }
    if (attackTime > 0) --attackTime;
    if (hurtTime > 0) --hurtTime;
    if (hurtResistantTime > 0) --hurtResistantTime;
    if (health <= 0) ++deathTime;

    // EntityPlayer.onLivingUpdate: charms, mana, peaceful healing
    if (health > healthCap()) health = healthCap(); // took the Bear Charm off
    { // AxeolonCharms.tick: Gliding (slow fall, no fall damage; sneak to drop) and Swiftness
        ItemStack* glide = findCharm(CHARM_GLIDE);
        if (glide && !onGround && !inWater && motionY < double(GLIDE_FALL_SPEED) && !sneaking) {
            motionY = double(GLIDE_FALL_SPEED);
            fallDistance = 0.0f;
            if (++glideTicks >= 20) {
                glideTicks = 0;
                wearCharm(glide, 1);
            }
        }
        ItemStack* swift = findCharm(CHARM_SWIFT);
        if (swift && onGround && motionX * motionX + motionZ * motionZ > 0.001) {
            if (++swiftTicks >= 40) {
                swiftTicks = 0;
                wearCharm(swift, 1);
            }
        }
        speedMultiplier = findCharm(CHARM_SWIFT) ? 1.3f : 1.0f; // SWIFT_BONUS
    }
    if (health > 0 && mana < MAX_MANA) {
        if (++manaRegenTimer >= MANA_REGEN_TICKS) {
            manaRegenTimer = 0;
            ++mana;
        }
    } else {
        manaRegenTimer = 0;
    }
    if (peaceful && health < healthCap() && ticksExisted % 20 * 12 == 0) heal(1);

    // EntityPlayerSP.onLivingUpdate: input, crouch
    bool blocked = health <= 0; // isMovementBlocked
    sneaking = !blocked && in.sneak;
    if (sneaking && ySize < 0.2f) ySize = 0.2f;
    // EntityLiving.onLivingUpdate
    moveStrafing = blocked ? 0.0f : sneaking ? float(double(in.strafe) * 0.3) : in.strafe;
    moveForward = blocked ? 0.0f : sneaking ? float(double(in.forward) * 0.3) : in.forward;
    isJumping = !blocked && in.jump;
    bool water = handleWaterMovement(w);
    bool lava = handleLavaMovement(w);
    if (isJumping) {
        if (water || lava) motionY += double(0.04f);
        else if (onGround) motionY = double(0.42f);
    }
    moveStrafing *= 0.98f;
    moveForward *= 0.98f;
    moveEntityWithHeading(w, moveStrafing, moveForward);
    // EntityPlayer.onLivingUpdate: how much the view bobs
    float sp = MathHelper::sqrt_double(motionX * motionX + motionZ * motionZ);
    float tilt = float(std::atan(-motionY * double(0.2f))) * 15.0f;
    if (sp > 0.1f) sp = 0.1f;
    if (!onGround || health <= 0) sp = 0.0f;
    if (onGround || health <= 0) tilt = 0.0f;
    cameraYaw += (sp - cameraYaw) * 0.4f;
    cameraPitch += (tilt - cameraPitch) * 0.8f;
}
