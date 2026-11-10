#pragma once
#include <functional>
// Mobs: EntityLiving / EntityCreature / EntityMobs / EntityAnimals and NSSS's mobs (pig, sheep, cow,
// chicken, creeper, skeleton, zombie, spider, slime, giant), drawn with the jar's own models
// (render/AlphaModels: ModelPig, ModelCow, ModelBiped...).
//  - physics: Entity.moveEntity + EntityLiving.moveEntityWithHeading (MobAI.cpp)
//  - AI: looking around, wandering by Pathfinder paths, hunting the player (zombies, spiders,
//    creepers, skeletons), burning in daylight, despawning far from the player
//  - SpawnerAnimals / SpawnerMonsters: PlayerControllerSP's two, every tick
//  - saved back into their chunk's "Entities" list with every tag Java wrote kept as it was
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include "model/AxmModel.h"
#include "world/JavaRandom.h"
#include "world/Nbt.h"
#include "world/Physics.h"

class World;
class Renderer;
class Player;
class Inventory;
class ItemEntities;
struct Chunk;

class Mobs {
public:
    // Chunk streaming (World.onChunkLoaded / beforeSave / onUnload): mobs come out of the chunk's
    // saved list while it's loaded and go back in when it's saved.
    void chunkLoaded(Chunk& c);
    void writeToChunk(Chunk& c) const;
    void chunkUnloaded(int cx, int cz);
    // World.onSpawnEntity / the /summon command: a mob from its saved form
    void adopt(nbt::TagPtr tag);
    void clear() { mobs.clear(); arrows.clear(); tnts.clear(); thrown.clear(); falling.clear(); }

    // 20 per second. editWorld: explosions may change blocks (not while a chunk is half made)
    void tick(World& w, Player& p, ItemEntities& items, bool editWorld, bool peaceful);
    // PlayerControllerSP.updateController: SpawnerMonsters and SpawnerAnimals (once a tick)
    void spawn(World& w, const Player& p, bool monsters, bool animals);
    void render(const World& w, Renderer& r, float partial, float camX, float camY, float camZ, float maxDist);

    // The mob the crosshair is on within reach (EntityRenderer.getMouseOver), or -1. dist: how far.
    int pick(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const;
    // EntityPlayer.attackTargetEntityWithCurrentItem
    void attack(int index, Player& p, int damage, ItemEntities& items);

    size_t count() const { return mobs.size(); }
    bool anyIn(const AABB& box) const; // for pressure plates

    // EntityTNTPrimed: lit by the player (80 ticks) or by another explosion (10-29 ticks)
    void primeTnt(int bx, int by, int bz, bool byExplosion);
    // ItemBow / ItemSnowball / ItemEgg: the player's projectiles (EntityArrow, EntitySnowball, EntityEgg)
    enum Throw { T_ARROW, T_SNOWBALL, T_EGG };
    void throwFrom(const Player& p, int what);
    void renderThrown(Renderer& r, float partial, float yaw, float pitch);
    // EntityMinecart.applyEntityCollision: every live mob's box, motion and what it rides (a cart: -id)
    template <class F> void forEachBody(F f) {
        for (Mob& m : mobs)
            if (!m.dead && m.health > 0) f(m.box, m.x, m.z, m.mx, m.mz, m.vehicle);
    }
    // where a cart's passenger sits (feet), or false if the cart is gone; set by the game
    std::function<bool(int cartId, double& x, double& y, double& z)> cartSeat;
    // EntityFish: the fishing rod's bobber (only ever the player's)
    struct Bobber {
        bool active = false;
        double x = 0, y = 0, z = 0, px = 0, py = 0, pz = 0, mx = 0, my = 0, mz = 0;
        float yaw = 0, pitch = 0;
        bool inGround = false;
        int tx = -1, ty = -1, tz = -1, inTile = 0, groundTicks = 0, airTicks = 0, shake = 0, bite = 0;
        int hooked = 0; // the mob on the hook (uid)
    };
    Bobber bobber;
    void castBobber(const Player& p);                  // ItemFishingRod.onItemRightClick: out it goes
    int reelIn(const Player& p, ItemEntities& items);  // func_4043_i: what the rod wears (0..3)
    void tickBobber(World& w, const Player& p, bool holdingRod);
    void renderBobber(Renderer& r, const Player& p, float partial, float camYaw, float camPitch);
    // EntityPig (riding) and EntityCow (milking): right-click on a mob
    // Right-click on a mob. Fills chat with what to tell the player. True if the click was used.
    bool interact(int index, Player& p, Inventory& inv, bool sneaking, bool creative, std::string& chat);
    bool riding() const { return riddenUid != 0; }
    // Where the rider sits now (posY at the eyes, like the player); false if not riding
    bool riderSeat(double& x, double& y, double& z) const;
    void dismount(Player& p);
    bool pickedUpSomething = false; // an arrow went back into the inventory this tick
    void renderTnt(const World& w, Renderer& r, float partial);
    // EntityFallingSand
    void fallingBlock(int x, int y, int z, int id);
    void renderFalling(const World& w, Renderer& r, float partial);

    enum Kind { PIG, SHEEP, COW, CHICKEN, CREEPER, SKELETON, ZOMBIE, SPIDER, SLIME, GIANT, KIND_COUNT };

private:
    struct PathPoint { int x, y, z; };
    struct Mob {
        Kind kind;
        std::shared_ptr<nbt::Tag> tag; // what Java saved (unknown tags go back untouched)
        // Entity
        double x, y, z, px = 0, py = 0, pz = 0, mx = 0, my = 0, mz = 0;
        float yaw, pitch, prevYaw = 0, prevPitch = 0, headYaw = 0;
        float width = 0.6f, height = 1.8f;
        AABB box{};
        bool onGround = true, collidedH = false, collidedV = false, inWater = false, dead = false;
        float fallDistance = 0, ySize = 0;
        int fire = 0, air = 300;
        // EntityLiving
        int health = 10, prevHealth = 10, hurtTime = 0, maxHurtTime = 0, deathTime = 0, attackTime = 0;
        int hurtResist = 0, lastDamage = 0, age = 0, soundTimer = 0;
        float attackedAtYaw = 0, moveStrafing = 0, moveForward = 0, randomYawVelocity = 0, moveSpeed = 0.7f;
        float renderYaw = 0, prevRenderYaw = 0, limbSwing = 0, limbAmount = 0, prevLimbAmount = 0;
        bool jumping = false;
        int lookTicks = 0; bool lookAtPlayer = false;
        // EntityCreature
        std::vector<PathPoint> path; size_t pathIndex = 0;
        bool target = false, hasAttacked = false;
        // per mob
        int fuse = 0, prevFuse = 0;   // creeper
        int eggTimer = 6000;          // chicken
        float wing = 0, prevWing = 0, flapSpeed = 0, prevFlapSpeed = 0, flapAccel = 1; // EntityChicken
        int slimeJumpDelay = 0;
        float squish = 0, prevSquish = 0; // EntitySlime.field_768_a
        int ticksExisted = 0;
        int uid = 0;
        int vehicle = 0; // the mob it rides (uid): a skeleton on a spider
        bool saddle = false, sheared = false;
        int slimeSize = 1;
    };
    struct Arrow { // EntityArrow from a skeleton
        double x, y, z, px, py, pz, mx, my, mz;
        float yaw, pitch;
        bool inGround = false;
        int ticksInGround = 0, ticksInAir = 0, inTile = 0;
        int tileX = 0, tileY = 0, tileZ = 0;
        bool dead = false;
        bool fromPlayer = false;  // hits mobs, and can be picked up again once it lands
        bool fromDispenser = false; // hits mobs and the player
    };
    std::vector<Mob> mobs;
    int riddenUid = 0;
    ItemEntities* curItems = nullptr;
    Mob* byUid(int uid);
    int nextUid = 1;
    struct Thrown { int kind; double x, y, z, px, py, pz, mx, my, mz; bool dead = false; };
    std::vector<Thrown> thrown;
    void tickThrown(World& w, Player& p, ItemEntities& items);
    // the first living mob a segment passes through (boxes grown by `grow`); -1 if none before maxT
    int mobOnPath(double x, double y, double z, double dx, double dy, double dz, double maxT, double grow, double& t);
    std::vector<Arrow> arrows;
    struct Falling { double x, y, z, px, py, pz, my; int id, fallTime; bool dead; };
    std::vector<Falling> falling;
    void tickFalling(World& w, ItemEntities& items);
    struct PrimedTnt { double x, y, z, px, py, pz, mx, my, mz; int fuse; bool onGround = false; };
    std::vector<PrimedTnt> tnts;
    void tickTnt(World& w, Player& p, ItemEntities& items, bool editWorld);
    std::mt19937 rng{12345};
    JavaRandom jr{0xA11CE};       // Entity.rand stand-in (one for all mobs)
    std::vector<axm::ModelVertex> scratch;
    std::vector<AABB> boxes;

    bool fromTag(std::shared_ptr<nbt::Tag> tag, Mob& m);
    int rand(int n) { return int(rng() % uint32_t(n)); }
    bool rnd() { return rng() & 1; }

    // MobAI.cpp
    void initMob(Mob& m); // size, speed, and the like for its kind
    void setSize(Mob& m, float w, float h);
    void setPosition(Mob& m, double x, double y, double z);
    void moveEntity(const World& w, Mob& m, double dx, double dy, double dz);
    void moveWithHeading(const World& w, Mob& m, float s, float f);
    bool waterMovement(const World& w, Mob& m);
    bool lavaMovement(const World& w, const Mob& m) const;
    bool hurt(Mob& m, int damage, bool fromPlayer, double fx, double fz, ItemEntities* items, const World* w);
    void die(Mob& m, ItemEntities* items, const World* w);
    void livingUpdate(World& w, Mob& m, Player& p, ItemEntities& items, bool editWorld);
    void actionState(World& w, Mob& m, Player& p);
    void creatureAI(World& w, Mob& m, Player& p);
    bool findPath(const World& w, Mob& m, double tx, double ty, double tz, float range);
    float pathWeight(const World& w, const Mob& m, int x, int y, int z) const;
    bool canSee(const World& w, const Mob& m, const Player& p) const;
    void faceTowards(Mob& m, double tx, double ty, double tz, float maxTurn);
    void explode(World& w, Mob& m, Player& p, ItemEntities& items, bool editWorld);
    void explodeAt(World& w, double x, double y, double z, float size, bool editWorld, bool fire, bool breakBlocks,
                   const Mob* source, Player& p, ItemEntities& items, bool byPlayer);
    void tickArrows(World& w, Player& p);
    void playSound(const Mob& m, const char* name, float volume = 1.0f);
};
