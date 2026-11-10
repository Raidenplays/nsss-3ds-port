#pragma once
#include <functional>
// Mobs: Beta's EntityLiving / EntityCreature / EntityMobs / EntityAnimals and the mobs themselves,
// drawn from Axeolon's Blockbench models with their animations (AxeolonAnimator + AxeolonMobAnims).
//  - physics: Entity.moveEntity + EntityLiving.moveEntityWithHeading (MobAI.cpp)
//  - AI: looking around, wandering by Pathfinder paths, hunting the player (zombies, spiders,
//    creepers, skeletons, ghouls), burning in daylight, despawning far from the player
//  - SpawnerAnimals: animals on grass in daylight, monsters in the dark
//  - saved back into their chunk's "Entities" list with every tag Java wrote kept as it was
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include "model/Animator.h"
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
    // World.onSpawnEntity: a mob made by world generation (Tabish in her lair)
    void adopt(nbt::TagPtr tag);
    void clear() { mobs.clear(); arrows.clear(); bolts.clear(); tnts.clear(); thrown.clear(); falling.clear(); icicles.clear(); }

    // 20 per second. editWorld: explosions may change blocks (not while a chunk is half made)
    void tick(World& w, Player& p, ItemEntities& items, bool editWorld, bool peaceful);
    // SpawnerAnimals.performSpawning (once a tick)
    void spawn(World& w, const Player& p, bool monsters, bool animals);
    void render(const World& w, Renderer& r, float partial, float camX, float camY, float camZ, float maxDist);

    // The mob the crosshair is on within reach (EntityRenderer.getMouseOver), or -1. dist: how far.
    int pick(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const;
    // EntityPlayer.attackTargetEntityWithCurrentItem
    void attack(int index, Player& p, int damage, bool mace, ItemEntities& items);

    size_t count() const { return mobs.size(); }
    bool anyIn(const AABB& box) const; // for pressure plates
    void launch(const AABB& box, double mx, double my, double mz); // a piston throws them

    // Spell projectiles (EntityMagicMissile, EntitySpellFireball, EntityFrostBolt), cast by the player
    enum Bolt { MISSILE = 0, FIREBALL = 1, FROST = 3 };
    void castBolt(int spellId, const Player& p);
    void renderBolts(Renderer& r, float partial, float yaw, float pitch);
    // EntityTNTPrimed: lit by the player (80 ticks) or by another explosion (10-29 ticks)
    void primeTnt(int bx, int by, int bz, bool byExplosion);
    // ItemBow / ItemSnowball / ItemEgg: the player's projectiles (EntityArrow, EntitySnowball, EntityEgg)
    enum Throw { T_ARROW, T_SNOWBALL, T_EGG };
    void throwFrom(const Player& p, int what);
    // BlockDispenser: an arrow, snowball or egg from (x, y, z) along (dx, dy, dz), or a magic missile
    void shootFrom(double x, double y, double z, double dx, double dy, double dz, float speed, float spread, int what);
    void castBoltAt(int spell, double x, double y, double z, double dx, double dy, double dz);
    void renderThrown(Renderer& r, float partial, float yaw, float pitch);
    void renderFireballs(Renderer& r, float partial, float yaw, float pitch);
    void jukebox(int x, int y, int z, bool playing); // AxeolonJukeboxes.record: animals dance near one
    // EntityMinecart.applyEntityCollision: every live mob's box, motion and what it rides (a cart: -id)
    template <class F> void forEachBody(F f) {
        for (Mob& m : mobs)
            if (!m.dead && m.health > 0) f(m.box, m.x, m.z, m.mx, m.mz, m.vehicle);
    }
    // where a cart's passenger sits (feet), or false if the cart is gone; set by the game
    std::function<bool(int cartId, double& x, double& y, double& z)> cartSeat;
    // AxeolonDynamicLights (Fancy): burning mobs as moving lights {key, x, y, z}
    template <class F> void forEachBurning(F f) const {
        for (const Mob& m : mobs)
            if (m.fire > 0 && !m.dead) f(static_cast<const void*>(&m), m.x, m.y + double(m.height) * 0.5, m.z);
    }
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
    // the player's punch: a fireball on the line within reach is knocked back along the look
    bool punchFireball(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist);
    // ---- boss fights (AxeolonBossFight, EntityAxeolonBoss, EntityTabish, AxeolonCinematic) ----
    struct BossInfo {
        bool active = false, cutscene = false, enraged = false;
        float health = 0;          // 0..1
        int elapsed = 0, left = 0; // intro ticks
        int titleTick = 0;
        const char* name = ""; const char* title = "";
    };
    BossInfo bossInfo() const;
    void bossTick(World& w, Player& p, bool creative); // once a game tick, before Mobs::tick
    bool arenaLocked(int x, int z) const;              // no building or digging in the arena mid-fight
    bool introPlaying() const { return bossUid >= 0 && cutscene > 0; }
    // AxeolonCinematic.applyCamera: the intro camera, eased from and back to the player's view.
    // Takes the player's eye and look (degrees); gives the camera's. False when no scene plays.
    bool cinematic(float partial, double px, double py, double pz, float pyaw, float ppitch, double& cx, double& cy,
                   double& cz, float& yaw, float& pitch, float& roll, float& fov);
    float cinemaBars() const; // 0..1: how far the black bars have slid in
    bool cinematicActive() const { return cineActive; }
    void shake(float amount) { kick = std::max(kick, amount); }
    void renderIcicles(Renderer& r, float partial);
    // ---- Axeolon creatures: taming, sitting, the saddle, riding (EntityAxeolonCreature / EntityDireWolf)
    // Right-click on a mob. Fills chat with what to tell the player. True if the click was used.
    bool interact(int index, Player& p, Inventory& inv, bool sneaking, bool creative, std::string& chat);
    // Pocket Edition's interact buttons (above the hotbar on touch screens): what a mob offers, and doing it
    enum Action { ACT_USE, ACT_SIT, ACT_RIDE };
    struct Offer { const char* label; Action act; };
    std::vector<Offer> offers(int index, const Inventory& inv) const;
    bool act(int index, Action a, Player& p, Inventory& inv, bool creative, std::string& chat);
    bool riding() const { return riddenUid != 0; }
    struct RiderInput { float forward = 0, strafe = 0, yaw = 0; bool jump = false; };
    void setRiderInput(const RiderInput& in) { riderIn = in; }
    // Where the rider sits now (posY at the eyes, like the player); false if not riding
    bool riderSeat(double& x, double& y, double& z) const;
    void dismount(Player& p);
    // EntityAxeolonCreature.onPlayerHurt: tamed mobs go after whoever hurt their owner
    void ownerHurtBy(int attackerUid, const Player& p);
    bool pickedUpSomething = false;
    std::vector<std::string> earned; // achievements the mobs gave this tick (AxeolonProgress.grant) // an arrow went back into the inventory this tick
    void renderTnt(const World& w, Renderer& r, float partial);
    // EntityFallingSand
    void fallingBlock(int x, int y, int z, int id);
    void renderFalling(const World& w, Renderer& r, float partial);

    // Every model a mob can use, loaded once (romfs:/axeolon/mob/<name>.axm)
    static axm::Model* model(const std::string& name);

    enum Kind {
        PIG, SHEEP, COW, CHICKEN, CREEPER, SKELETON, ZOMBIE, PIGZOMBIE, SPIDER, SLIME, GHAST, SQUID, GIANT,
        GHOUL, RACCOON, OWL, GOAT, DIREWOLF, TABISH, KIND_COUNT
    };

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
        float wingFlap = 0, wingSpeed = 1;
        int slimeJumpDelay = 0;
        int frozenTicks = 0;          // Frost Bolt: moves at a third of its speed
        int ticksExisted = 0;
        int uid = 0;
        int targetMob = 0;            // the mob it's after (uid), for wolves hunting skeletons
        bool angry = false, warned = false;
        // EntityRaccoon: WANDER 0, GO 1, RUMMAGE 2, FLEE 3, EAT 4
        int rState = 0, rTimer = 0, rCooldown = 100, rcx = 0, rcy = 0, rcz = 0, rfdx = 0, rfdz = 0, raid = 0;
        double rsx = 0, rsz = 0;
        bool raider = false;
        nbt::TagPtr carried; // the stack of food in its mouth (as saved)
        // EntityOwl / EntityGoat
        bool owlFlying = false;
        // EntitySquid
        float sqPitch = 0, sqPrevPitch = 0, sqRoll = 0, sqPrevRoll = 0, sqAnim = 0, sqPrevAnim = 0, sqAnimSpeed = 0;
        float sqTentacle = 0, sqPrevTentacle = 0, sqSpeed = 0, sqRotSpeed = 0, sqVx = 0, sqVy = 0, sqVz = 0;
        // EntityGhast
        double wpX = 0, wpY = 0, wpZ = 0;
        int courseCooldown = 0, aggroCooldown = 0, attackCounter = 0, prevAttackCounter = 0;
        bool ghastTarget = false;
        int vehicle = 0; // the mob it rides (uid): a skeleton on a spider
        double otx = 0, oty = 0, otz = 0;
        int flyTime = 0, goatWindup = 0, goatCharge = 0;
        // EntityAxeolonBoss / EntityTabish
        bool awake = false, enraged = false, inCutscene = false, standing = false;
        int busyTicks = 0, slamTicks = 0, slamCooldown = 200, swipeTicks = 0;
        int arenaX = 0, arenaY = 0, arenaZ = 0, arenaR = 22;
        bool arenaSet = false, hasGate = false;
        int gate[6] = {0, 0, 0, 0, 0, 0}, gateBlock = 0, gateMeta = 0;
        bool saddle = false, sheared = false, sitting = false, tamed = false;
        int fleece = 0, slimeSize = 1, armour = 0;
        bool crateLeft = false, crateRight = false;
        Animator anim;
        int idleCooldown = 0;
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
    RiderInput riderIn;
    Mob* byUid(int uid);
    bool wolfAI(World& w, Mob& m, Player& p, ItemEntities& items);
    bool raccoonAI(World& w, Mob& m, Player& p);
    void owlAI(World& w, Mob& m, Player& p);
    void owlTakeOff(World& w, Mob& m, double fromX, double fromY, double fromZ, bool from);
    bool goatAI(World& w, Mob& m, Player& p);
    void raccoonAfterMove(World& w, Mob& m);
    void raccoonFlee(World& w, Mob& m, double fx, double fz);
    void pathAwayFrom(World& w, Mob& m, double fx, double fz, float dist);
    void walkPath(World& w, Mob& m);
    void alertPack(Mob& m, int attackerUid);
    // boss fight state
    int bossUid = -1, pendingWake = -1, nextUid = 1;
    int cutscene = 0, cutsceneLength = 0;
    bool playingEnraged = false;
    // AxeolonCinematic playback
    bool cineActive = false;
    int cineTick = 0;
    double cax = 0, cay = 0, caz = 0, cfx = 0, cfz = 1;
    float kick = 0;
    Mob* bossMob();
    const Mob* bossMob() const;
    bool inArena(const Mob& b, double x, double z) const;
    void startFight(World& w, Mob& b);
    void endFight(bool won);
    void resetFight(World& w, Mob& b);
    void closeGate(World& w, Mob& b);
    void openGate(World& w, Mob& b);
    void tabishUpdate(World& w, Mob& m, Player& p, ItemEntities& items);
    void tabishCutscene(World& w, Mob& m, int t);
    void tabishAttack(Mob& m, float dist, const Player& p);
    void tabishEnrage(Mob& m);
    void once(Mob& m, const char* anim, int ticks, int fade);
    void puff(const Mob& m, int n);
    struct Icicle { double x, y, z, py, my, startY; int age; bool tip, dead; };
    std::vector<Icicle> icicles;
    void tickIcicles(World& w, Player& p, ItemEntities& items);
    void knockIcicleLoose(World& w, int x, int y, int z);
    struct Thrown { int kind; double x, y, z, px, py, pz, mx, my, mz; bool dead = false; };
    // EntityFireball: a ghast's, speeding up as it goes; hit it back and it's yours
    struct Fireball { double x, y, z, px, py, pz, mx, my, mz, ax, ay, az; int age = 0, owner = 0; bool dead = false; };
    std::vector<Fireball> fireballs;
    void tickFireballs(World& w, Player& p, ItemEntities& items, bool editWorld);
    void ghastAI(World& w, Mob& m, Player& p);
    std::vector<Thrown> thrown;
    void tickThrown(World& w, Player& p, ItemEntities& items);
    // the first living mob a segment passes through (boxes grown by `grow`); -1 if none before maxT
    int mobOnPath(double x, double y, double z, double dx, double dy, double dz, double maxT, double grow, double& t);
    std::vector<Arrow> arrows;
    struct SpellBolt { int spell; double x, y, z, px, py, pz, mx, my, mz; int age = 0, target = -1; bool dead = false; };
    std::vector<SpellBolt> bolts;
    struct Falling { double x, y, z, px, py, pz, my; int id, fallTime; bool dead; };
    std::vector<Falling> falling;
    void tickFalling(World& w, ItemEntities& items);
    struct PrimedTnt { double x, y, z, px, py, pz, mx, my, mz; int fuse; bool onGround = false; };
    std::vector<PrimedTnt> tnts;
    void tickTnt(World& w, Player& p, ItemEntities& items, bool editWorld);
    void tickBolts(World& w, Player& p, ItemEntities& items, bool editWorld);
    std::mt19937 rng{12345};
    JavaRandom jr{0xA11CE};       // Entity.rand stand-in (one for all mobs)
    std::vector<axm::ModelVertex> scratch;
    std::vector<AABB> boxes;

    void updateAnims(Mob& m);
    bool dancing(Mob& m);
    struct Jukebox { int x, y, z; int64_t since; };
    std::vector<Jukebox> jukeboxes;
    const World* animWorld = nullptr;
    int64_t animTicks = 0;
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
