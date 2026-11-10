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
namespace {

inline float mhSin(float v) { return MathHelper::sin(v); }
inline float mhCos(float v) { return MathHelper::cos(v); }
inline int floorD(double v) { return int(std::floor(v)); }

constexpr int LADDER = 65, CACTUS = 81, FIRE = 51;

} // namespace

// InventoryPlayer.getTotalArmorValue: ItemArmor.protection by article (helmet 3, plate 8, legs 6,
// boots 3), for the armour below id 370 (the quiver and sponge armour count for nothing)
int Player::armourValue() const {
    int p = 0;
    if (inv)
        for (const ItemStack& a : inv->armour)
            if (!a.empty() && a.id >= 298 && a.id <= 317) {
                static const int PROTECTION[4] = {3, 8, 6, 3};
                p += PROTECTION[(a.id - 298) % 4];
            }
    return p;
}

// InventoryPlayer.damageArmor: ItemStack.damageItem on each piece below 370; a worn-out piece goes
void Player::damageArmour(int n) {
    if (!inv) return;
    for (ItemStack& a : inv->armour) {
        if (a.empty() || !((a.id >= 298 && a.id <= 317) || (a.id >= 370 && a.id <= 374))) continue;
        if (a.id < 370) {
            a.damage = int16_t(a.damage + n);
            if (a.damage > survival::maxDamage(a.id)) {
                if (--a.count < 0) a.count = 0;
                a.damage = 0;
            }
        }
        if (a.count == 0) a.clear();
    }
}

void Player::heal(int n) { // EntityLiving.heal
    if (health > 0) {
        health += n;
        if (health > 20) health = 20;
        hurtResistantTime = 10; // heartsLife = maxHealth / 2
    }
}

// EntityPlayer.attackEntityFrom (difficulty, armour) then EntityLiving.attackEntityFrom
bool Player::attack(int dmg, Source src, double fromX, double fromZ) {
    if (health <= 0) return false;
    if (float(hurtResistantTime) > 20.0f / 2.0f) return false;
    if (src == MOB || src == MOB_ALWAYS) { // EntityMobs or EntityArrow
        if (difficulty == 0) dmg = 0;
        if (difficulty == 1) dmg = dmg / 3 + 1;
        if (difficulty == 3) dmg = dmg * 3 / 2;
    }
    int k = dmg * (25 - armourValue()) + damageRemainder;
    damageArmour(dmg);
    dmg = k / 25;
    damageRemainder = k % 25;
    if (dmg == 0 && src != MOB_ALWAYS) return false; // arrows, spiders, zombies and skeletons always land
    // EntityLiving.attackEntityFrom
    attackedAtYaw = 0.0f;
    if (src != ENV) {
        double dx = fromX - posX, dz = fromZ - posZ;
        while (dx * dx + dz * dz < 1.0e-4) {
            dx = (rand.nextDouble() - rand.nextDouble()) * 0.01;
            dz = (rand.nextDouble() - rand.nextDouble()) * 0.01;
        }
        attackedAtYaw = float(std::atan2(dz, dx) * 180.0 / double(3.1415927f)) - rotationYaw;
        float len = MathHelper::sqrt_double(dx * dx + dz * dz); // applyKnockback
        motionX /= 2.0; motionY /= 2.0; motionZ /= 2.0;
        motionX -= dx / double(len) * double(0.4f);
        motionY += double(0.4f);
        motionZ -= dz / double(len) * double(0.4f);
        if (motionY > double(0.4f)) motionY = double(0.4f);
    } else {
        attackedAtYaw = float(int(rand.nextDouble() * 2.0) * 180);
    }
    if (invulnerable) return false; // isCreative
    if (float(hurtResistantTime) > 20.0f / 2.0f) {
        if (prevHealth - dmg >= health) return false;
        health = prevHealth - dmg;
    } else {
        prevHealth = health;
        hurtResistantTime = 20;
        health -= dmg;
        hurtTime = maxHurtTime = 10;
    }
    sounds.push_back({"random.hurt", 1.0f, (rand.nextFloat() - rand.nextFloat()) * 0.2f + 1.0f});
    if (health <= 0) { // EntityPlayer.onDeath
        motionY = 0.1;
        if (src != ENV) {
            motionX = double(-MathHelper::cos((attackedAtYaw + rotationYaw) * 3.141593f / 180.0f) * 0.1f);
            motionZ = double(-MathHelper::sin((attackedAtYaw + rotationYaw) * 3.141593f / 180.0f) * 0.1f);
        } else {
            motionX = motionZ = 0.0;
        }
    }
    return true;
}

// EntityLiving.fall (goldArmor is always 0)
void Player::fall(float d) {
    const World& w = *world;
    int dmg = int(std::ceil(double(d - 3.0f)));
    if (dmg > 0) {
        attack(dmg, ENV);
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
                    if (id == CACTUS) attack(1, ENV); // BlockCactus.onEntityCollidedWithBlock
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
        attack(1, ENV); // dealFireDamage
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
        float ground = 0.1f; // EntityPlayer.getGroundAcceleration
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
        if (fire % 20 == 0) attack(1, ENV);
        --fire;
    }
    if (handleLavaMovement(w)) { // setOnFireFromLava
        if (!invulnerable) {
            attack(4, ENV);
            fire = 600;
        }
    }
    if (posY < -64.0) attack(4, ENV); // kill(): the void
    // EntityLiving.onEntityUpdate
    if (health > 0) { // suffocating in a solid block
        int x = floorD(posX), y = floorD(posY), z = floorD(posZ);
        int id = (y >= 0 && y < 128) ? w.getBlockId(x, y, z) : 0;
        if (id && BP[id].opaqueCube) attack(1, ENV);
    }
    if (health > 0 && isInsideOfWater(w)) {
        --air;
        if (air == -20) {
            air = 0;
            attack(2, ENV); // drowning
        }
        fire = 0;
    } else {
        air = MAX_AIR;
    }
    if (attackTime > 0) --attackTime;
    if (hurtTime > 0) --hurtTime;
    if (hurtResistantTime > 0) --hurtResistantTime;
    if (health <= 0) ++deathTime;

    // EntityPlayer.onLivingUpdate: peaceful heals a point every 4 seconds
    if (peaceful && health < 20 && ticksExisted % 20 * 4 == 0) heal(1);

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
