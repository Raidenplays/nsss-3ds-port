#pragma once
// The player's body: Beta's Entity / EntityLiving / EntityPlayer / EntityPlayerSP movement,
// ported line by line (tools/java/SimPlayer + hosttest/physics.cpp check it against the mod).
// Runs at 20 ticks a second; the camera is drawn between ticks (prevPos -> pos).
#include <string>
#include <vector>
#include "world/Physics.h"
#include "world/JavaRandom.h"

class World;
class Inventory;
struct ItemStack;

struct PlayerInput {
    float strafe = 0, forward = 0; // -1..1 (Circle Pad); sneaking scales them by 0.3
    bool jump = false, sneak = false;
};

class Player {
public:
    // Entity
    double posX = 0, posY = 0, posZ = 0;       // posY at eye level, like the save
    double prevPosX = 0, prevPosY = 0, prevPosZ = 0;
    double motionX = 0, motionY = 0, motionZ = 0;
    float rotationYaw = 0, rotationPitch = 0;   // degrees, Java convention
    AABB boundingBox{};
    bool onGround = false, isCollidedHorizontally = false, isCollidedVertically = false, inWater = false;
    float fallDistance = 0, ySize = 0, distanceWalkedModified = 0;
    int ticksExisted = 0;
    float speedMultiplier = 1.0f; // AxeolonCharms.speedMultiplier (Swift charm)
    float lastFall = 0;           // fall distance at the last landing (for damage)

    // EntityLiving / EntityPlayer health (Axeolon armour, mana and the Bear Charm included)
    int health = 20, prevHealth = 20;
    int hurtResistantTime = 0;    // field_9306_bj (heartsLife): 20 after a hit, half of it ignores smaller hits
    int lastDamage = 0;           // field_9346_af
    int hurtTime = 0, maxHurtTime = 0, deathTime = 0, attackTime = 0;
    float attackedAtYaw = 0;
    int air = 300;
    static constexpr int MAX_AIR = 300;
    int fire = 0;                 // ticks left burning (negative: fire resistance after leaving it)
    int fireResistance = 20;      // EntityPlayer
    int damageRemainder = 0;      // armour's fraction of a point, in 25ths
    int mana = 20, manaRegenTimer = 0;
    double exhaustion = 0.0; // NSSS's EntityPlayer.getExhaustion (0..1), greys the hearts
    bool exhausted = false;   // getIsExhausted
    static constexpr int MAX_MANA = 20, MANA_REGEN_TICKS = 80;
    bool invulnerable = false;    // creative: nothing hurts
    bool peaceful = false;        // difficulty 0 heals a point every 12 s
    Inventory* inv = nullptr;     // armour and charms
    // Things the game should play/show this tick ("random.hurt", step sounds on a hard landing...)
    struct SoundEvent { const char* name; float volume, pitch; };
    std::vector<SoundEvent> sounds;    // World.playSoundAtEntity(this, ...)

    // EntityLiving.swingItem / the first-person bob (EntityPlayer field_774_f / field_9328_R)
    int swingProgressInt = 0;
    bool isSwinging = false;
    float swingProgress = 0, prevSwingProgress = 0;
    float prevDistanceWalked = 0;
    float cameraYaw = 0, prevCameraYaw = 0, cameraPitch = 0, prevCameraPitch = 0;
    void swingItem() {
        if (!isSwinging || swingProgressInt >= 4 || swingProgressInt < 0) { // EntityPlayer: restart halfway
            swingProgressInt = -1;
            isSwinging = true;
        }
    }
    float swing(float partial) const { // getSwingProgress
        float d = swingProgress - prevSwingProgress;
        if (d < 0.0f) d += 1.0f;
        return prevSwingProgress + d * partial;
    }

    int healthCap() const;        // 20, +4 with the Bear Charm
    // AxeolonCharms
    ItemStack* findCharm(int id); // the first worn charm of this kind
    bool immovable() const;       // the Bear Charm: no knockback
    void wearCharm(ItemStack* c, int amount); // breaks it (and says so) when it runs out
    std::vector<std::string> messages; // chat lines for the game to show (a charm crumbled)
    int armourValue() const;      // AxeolonArmor.totalPoints
    bool dead() const { return health <= 0; }
    // fromMonster: EntityMobs or an arrow, scaled by the difficulty (peaceful: nothing)
    bool attack(int damage, bool bypassArmour, double fromX = 0, double fromZ = 0, bool hasSource = false, bool fromMonster = false);
    int difficulty = 2;
    // Axeolon battle mode: holding a shield up (AxeolonBattleInput sets it every tick)
    bool blocking = false;
    // EntityPlayerSP: standing in a Nether portal
    bool inPortal = false;
    float timeInPortal = 0, prevTimeInPortal = 0;
    int portalCooldown = 0;       // field_9373_b: just came through
    bool travelNow = false;       // Minecraft.usePortal is due (the game switches dimension)
    void setInPortal() { if (portalCooldown > 0) portalCooldown = 10; else inPortal = true; }
    int shieldBlock(int damage, double fromX, double fromZ); // AxeolonBattle.block: the damage left
    void heal(int n);
    bool isInsideOfWater(const World& w) const { return isEyeIn(w, false); } // isInsideOfMaterial(water)
    bool isEyeIn(const World& w, bool lava) const;
    // EntityPlayer.onDeath + Minecraft.respawn
    void resetAfterRespawn();

    static constexpr float YOFFSET = 1.62f, WIDTH = 0.6f, HEIGHT = 1.8f, STEP = 0.5f;

    void setPosition(double x, double y, double z);
    // setPositionAndRotation: places the player without moving it
    void place(double x, double y, double z, float yaw, float pitch);

    // One game tick (EntityLiving.onUpdate with EntityPlayerSP's input)
    void tick(const World& w, const PlayerInput& in);

    bool isSneaking() const { return sneaking; }

private:
    bool sneaking = false;
    float moveStrafing = 0, moveForward = 0;
    bool isJumping = false;
    int glideTicks = 0, swiftTicks = 0; // AxeolonCharms: wear a point every 20 / 40 ticks of use
    std::vector<AABB> boxes;
    JavaRandom rand{0x5EEDULL};
    int nextStepDistance = 1;
    const World* world = nullptr;

    void fall(float distance);
    void damageEntity(int n, bool bypassArmour);

    bool handleWaterMovement(const World& w);
    bool handleLavaMovement(const World& w) const;
    bool isOnLadder(const World& w) const;
    void moveFlying(float strafe, float forward, float accel);
    void moveEntity(const World& w, double dx, double dy, double dz);
    void moveEntityWithHeading(const World& w, float strafe, float forward);
    bool isOffsetPositionInLiquid(const World& w, double x, double y, double z);
};
