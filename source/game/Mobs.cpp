#include "Mobs.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <map>
#include "model/BlockModels.h"
#include "render/Renderer.h"
#include "world/World.h"
#include "world/BlockTable.h"
#include "world/Chunk.h"

namespace {

struct KindInfo {
    const char* id;      // EntityList save name
    const char* model;   // /axeolon/mob/<model>.bbmodel
    const char* texture; // romfs path of the entity texture
    float scale;         // preRenderCallback scale (RenderAxeolonMob, giant, ghast at rest)
    float height;        // setSize height (getEntityBrightness)
    float deathAngle;    // RenderLiving.func_172_a
    bool axeolonCreature; // EntityAxeolonCreature: scripts its own animations
};

// Same order as Mobs::Kind
const KindInfo KINDS[] = {
    {"Pig", "pig", "romfs:/textures/mob/pig.rgba", 1, 0.9f, 90, false},
    {"Sheep", "sheep", "romfs:/textures/mob/sheep.rgba", 1, 1.3f, 90, false},
    {"Cow", "cow", "romfs:/textures/mob/cow.rgba", 1, 1.3f, 90, false},
    {"Chicken", "chicken", "romfs:/textures/mob/chicken.rgba", 1, 0.4f, 90, false},
    {"Creeper", "creeper", "romfs:/textures/mob/creeper.rgba", 1, 1.8f, 90, false},
    {"Skeleton", "skeleton", "romfs:/textures/mob/skeleton.rgba", 1, 1.8f, 90, false},
    {"Zombie", "zombie", "romfs:/textures/mob/zombie.rgba", 1, 1.8f, 90, false},
    {"PigZombie", "pigzombie", "romfs:/textures/mob/pigzombie.rgba", 1, 1.8f, 90, false},
    {"Spider", "spider", "romfs:/textures/mob/spider.rgba", 1, 0.9f, 180, false},
    {"Slime", "slime", "romfs:/textures/mob/slime.rgba", 1, 0.6f, 90, false},
    {"Ghast", "ghast", "romfs:/textures/mob/ghast.rgba", 4.5f, 4.0f, 90, false},
    {"Squid", "squid", "romfs:/textures/mob/squid.rgba", 1, 0.95f, 90, false},
    {"Giant", "zombie", "romfs:/textures/mob/zombie.rgba", 6, 10.8f, 90, false},
    {"Ghoul", "ghoul", "romfs:/axeolon/mob/ghoul.rgba", 1, 1.8f, 90, false},
    {"Raccoon", "raccoon", "romfs:/axeolon/mob/raccoon.rgba", 1, 0.6f, 90, true},
    {"Owl", "owl", "romfs:/axeolon/mob/owl.rgba", 1, 0.9f, 90, true},
    {"Goat", "goat", "romfs:/axeolon/mob/goat.rgba", 1, 1.3f, 90, true},
    {"DireWolf", "dire_wolf", "romfs:/axeolon/mob/dire_wolf.rgba", 1.1f, 1.6f, 90, true},
    {"Tabish", "tabish", "romfs:/axeolon/mob/tabish.rgba", 2, 3.2f, 90, true},
};

// EntitySheep.field_21075_a
const float FLEECE[16][3] = {{1.0f, 1.0f, 1.0f}, {0.95f, 0.7f, 0.2f}, {0.9f, 0.5f, 0.85f}, {0.6f, 0.7f, 0.95f},
                             {0.9f, 0.9f, 0.2f}, {0.5f, 0.8f, 0.1f},  {0.95f, 0.7f, 0.8f}, {0.3f, 0.3f, 0.3f},
                             {0.6f, 0.6f, 0.6f}, {0.3f, 0.6f, 0.7f},  {0.7f, 0.4f, 0.9f},  {0.2f, 0.4f, 0.8f},
                             {0.5f, 0.4f, 0.3f}, {0.4f, 0.5f, 0.2f},  {0.8f, 0.3f, 0.3f},  {0.1f, 0.1f, 0.1f}};

const char* const FIDGET_PIG[] = {"look_around", "sniff", "ear_flick"};
const char* const FIDGET_COW[] = {"look_around", "shake_head", "ear_flick"};
const char* const FIDGET_SHEEP[] = {"look_around", "graze", "ear_flick"};
const char* const FIDGET_CHICKEN[] = {"peck", "look_around", "ruffle"};
const char* const FIDGET_CREEPER[] = {"look_around", "shiver"};
const char* const FIDGET_SPIDER[] = {"twitch", "look_around"};
const char* const FIDGET_ZOMBIE[] = {"look_around", "groan"};
const char* const FIDGET_SKELETON[] = {"look_around", "rattle"};
const char* const FIDGET_SLIME[] = {"wobble"};

// ---- a little matrix stack (row-major, column vectors) for RenderLiving's GL calls ----
struct M4 {
    float m[16];
    M4() { std::memset(m, 0, sizeof m); m[0] = m[5] = m[10] = m[15] = 1; }
    void mul(const float* b) {
        float r[16];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r[i * 4 + j] = m[i * 4] * b[j] + m[i * 4 + 1] * b[4 + j] + m[i * 4 + 2] * b[8 + j] + m[i * 4 + 3] * b[12 + j];
        std::memcpy(m, r, sizeof r);
    }
    void translate(float x, float y, float z) { float t[16] = {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1}; mul(t); }
    void scale(float x, float y, float z) { float t[16] = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1}; mul(t); }
    void rotate(float deg, int axis) {
        float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
        float r[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (axis == 0) { r[5] = c; r[6] = -s; r[9] = s; r[10] = c; }
        else if (axis == 1) { r[0] = c; r[2] = s; r[8] = -s; r[10] = c; }
        else { r[0] = c; r[1] = -s; r[4] = s; r[5] = c; }
        mul(r);
    }
};

std::map<std::string, std::unique_ptr<axm::Model>> modelCache;
std::unique_ptr<axm::Model> wolfArmour;

struct WolfBones { bool saddle, left, right; };
bool wolfBoneVisible(const char* name, void* user) {
    const WolfBones& w = *static_cast<WolfBones*>(user);
    if (!std::strcmp(name, "saddle")) return w.saddle;
    if (!std::strcmp(name, "crate_right")) return w.right;
    if (!std::strcmp(name, "crate_left")) return w.left;
    return true;
}

} // namespace

axm::Model* Mobs::model(const std::string& name) {
    auto it = modelCache.find(name);
    if (it != modelCache.end()) return it->second.get();
    std::unique_ptr<axm::Model> m(new axm::Model);
    if (!m->load(("romfs:/axeolon/mob/" + name + ".axm").c_str())) m.reset();
    axm::Model* r = m.get();
    modelCache[name] = std::move(m);
    return r;
}

bool Mobs::fromTag(std::shared_ptr<nbt::Tag> tag, Mob& m) {
    nbt::Tag* e = tag.get();
    if (!e || e->type != nbt::Compound) return false;
    std::string id = e->getString("id");
    int kind = -1;
    for (int k = 0; k < KIND_COUNT; k++)
        if (id == KINDS[k].id) kind = k;
    nbt::Tag* pos = kind >= 0 ? e->getList("Pos") : nullptr;
    if (!pos || pos->items.size() < 3) return false;
    m.kind = Kind(kind);
    m.tag = tag;
    m.x = m.px = pos->items[0]->d;
    m.y = m.py = pos->items[1]->d;
    m.z = m.pz = pos->items[2]->d;
    if (nbt::Tag* mo = e->getList("Motion"); mo && mo->items.size() >= 3) {
        m.mx = mo->items[0]->d;
        m.my = mo->items[1]->d;
        m.mz = mo->items[2]->d;
    }
    nbt::Tag* rot = e->getList("Rotation");
    m.yaw = m.prevYaw = m.renderYaw = m.prevRenderYaw = rot && rot->items.size() >= 2 ? float(rot->items[0]->d) : 0.0f;
    m.pitch = m.prevPitch = rot && rot->items.size() >= 2 ? float(rot->items[1]->d) : 0.0f;
    m.health = m.prevHealth = e->has("Health") ? e->getShort("Health") : 10;
    m.hurtTime = e->getShort("HurtTime");
    m.deathTime = e->getShort("DeathTime");
    m.attackTime = e->getShort("AttackTime");
    m.fallDistance = e->getFloat("FallDistance");
    m.fire = e->getShort("Fire");
    m.air = e->has("Air") ? e->getShort("Air") : 300;
    m.onGround = e->getBool("OnGround");
    m.saddle = e->getBool("Saddle");
    m.sheared = e->getBool("Sheared");
    m.fleece = e->getByte("Color") & 15;
    m.slimeSize = e->getInt("Size") + 1;
    m.armour = e->has("Armour") ? (e->getByte("Armour") & 3) : 0;
    m.crateLeft = e->has("CrateLeft");
    m.crateRight = e->has("CrateRight");
    m.sitting = e->getBool("AxSitting");
    if (m.kind == DIREWOLF) m.saddle = e->getBool("AxSaddled");
    if (m.kind == RACCOON) // EntityRaccoon.readEntityFromNBT: still has its food? it sits down to eat it
        if (nbt::Tag* c = e->getCompound("Carried")) {
            m.carried = nbt::clone(*c);
            m.rState = 4;
            m.rTimer = 140;
        }
    m.tamed = !e->getString("AxOwner").empty();
    initMob(m);
    if (m.kind == TABISH) { // EntityTabish: starts asleep; EntityAxeolonBoss.readEntityFromNBT
        m.anim.play("sleep", 1);
        if (e->has("ArenaR")) {
            m.arenaX = e->getInt("ArenaX");
            m.arenaY = e->getInt("ArenaY");
            m.arenaZ = e->getInt("ArenaZ");
            m.arenaR = e->getInt("ArenaR");
            m.arenaSet = true;
        }
        if (e->has("GateBlock")) {
            int g[6];
            for (int i = 0; i < 6; ++i) g[i] = e->getInt(("Gate" + std::to_string(i)).c_str());
            m.gate[0] = std::min(g[0], g[3]); m.gate[3] = std::max(g[0], g[3]);
            m.gate[1] = std::min(g[1], g[4]); m.gate[4] = std::max(g[1], g[4]);
            m.gate[2] = std::min(g[2], g[5]); m.gate[5] = std::max(g[2], g[5]);
            m.gateBlock = e->getInt("GateBlock");
            m.gateMeta = e->getInt("GateMeta");
            m.hasGate = true;
        }
        m.health = m.prevHealth = std::max(m.health, 1);
    }
    return true;
}

void Mobs::chunkLoaded(Chunk& c) {
    if (!c.level) return;
    nbt::Tag* list = c.level->getList("Entities");
    if (!list) return;
    auto& v = list->items;
    for (size_t i = 0; i < v.size();) {
        Mob m;
        std::shared_ptr<nbt::Tag> tag(v[i].release());
        if (!fromTag(tag, m)) {
            v[i].reset(nbt::clone(*tag).release()); // not ours: put it back as it was
            ++i;
            continue;
        }
        v.erase(v.begin() + i);
        c.modified = true; // the mob now lives here, not in the saved list
        mobs.push_back(std::move(m));
    }
}

void Mobs::adopt(nbt::TagPtr tag) {
    Mob m;
    if (fromTag(std::shared_ptr<nbt::Tag>(tag.release()), m)) mobs.push_back(std::move(m));
}

void Mobs::writeToChunk(Chunk& c) const {
    if (!c.level) c.level = nbt::makeCompound();
    nbt::Tag* list = c.level->getList("Entities");
    if (!list) {
        c.level->set("Entities", nbt::makeList(nbt::Compound));
        list = c.level->getList("Entities");
    }
    auto& v = list->items;
    // every mob came out of the list when the chunk loaded, so any mob in it now is one we put back
    // at the last save: take those out and write the mobs as they are now
    auto isMob = [](const nbt::TagPtr& t) {
        std::string id = t->getString("id");
        for (int k = 0; k < KIND_COUNT; k++)
            if (id == KINDS[k].id) return true;
        return false;
    };
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(), isMob), v.end());
    bool any = v.size() != before;
    for (const Mob& m : mobs) {
        if (m.dead || (int(std::floor(m.x)) >> 4) != c.xPosition || (int(std::floor(m.z)) >> 4) != c.zPosition) continue;
        // Entity.writeToNBT + EntityLiving.writeEntityToNBT over the tags Java saved
        nbt::TagPtr t = m.tag ? nbt::clone(*m.tag) : nbt::makeCompound();
        t->setString("id", KINDS[m.kind].id);
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(m.x));
        pos->items.push_back(nbt::makeDouble(m.y));
        pos->items.push_back(nbt::makeDouble(m.z));
        t->set("Pos", std::move(pos));
        nbt::TagPtr mo = nbt::makeList(nbt::Double);
        mo->items.push_back(nbt::makeDouble(m.mx));
        mo->items.push_back(nbt::makeDouble(m.my));
        mo->items.push_back(nbt::makeDouble(m.mz));
        t->set("Motion", std::move(mo));
        nbt::TagPtr ro = nbt::makeList(nbt::Float);
        ro->items.push_back(nbt::makeFloat(m.yaw));
        ro->items.push_back(nbt::makeFloat(m.pitch));
        t->set("Rotation", std::move(ro));
        t->setFloat("FallDistance", m.fallDistance);
        t->setShort("Fire", int16_t(m.fire));
        t->setShort("Air", int16_t(m.air));
        t->setBool("OnGround", m.onGround);
        t->setShort("Health", int16_t(m.health));
        t->setShort("HurtTime", int16_t(m.hurtTime));
        t->setShort("DeathTime", int16_t(m.deathTime));
        t->setShort("AttackTime", int16_t(m.attackTime));
        if (m.kind == SHEEP) {
            t->setBool("Sheared", m.sheared);
            t->setByte("Color", int8_t(m.fleece));
        } else if (m.kind == PIG) {
            t->setBool("Saddle", m.saddle);
        } else if (m.kind == SLIME) {
            t->setInt("Size", m.slimeSize - 1);
        }
        if (m.kind == RACCOON) {
            auto& en = t->entries; // the food it carries, or none
            en.erase(std::remove_if(en.begin(), en.end(), [](const std::pair<std::string, nbt::TagPtr>& kv) { return kv.first == "Carried"; }),
                     en.end());
            if (m.carried) t->set("Carried", nbt::clone(*m.carried));
        }
        if (KINDS[m.kind].axeolonCreature) { // EntityAxeolonCreature.writeEntityToNBT
            if (m.tamed && t->getString("AxOwner").empty()) t->setString("AxOwner", "Player");
            if (!m.tamed) t->setString("AxOwner", "");
            t->setBool("AxSitting", m.sitting);
            t->setBool("AxSaddled", m.kind == DIREWOLF && m.saddle);
            t->setBool("AxAngry", m.angry);
        }
        v.push_back(std::move(t));
        any = true;
    }
    if (any) c.modified = true;
}

void Mobs::chunkUnloaded(int cx, int cz) {
    mobs.erase(std::remove_if(mobs.begin(), mobs.end(),
                              [&](const Mob& m) { return (int(std::floor(m.x)) >> 4) == cx && (int(std::floor(m.z)) >> 4) == cz; }),
               mobs.end());
}

// AxeolonJukeboxes: which jukeboxes are playing (a record lasts about 190 s)
void Mobs::jukebox(int x, int y, int z, bool playing) {
    jukeboxes.erase(std::remove_if(jukeboxes.begin(), jukeboxes.end(),
                                   [&](const Jukebox& j) { return j.x == x && j.y == y && j.z == z; }),
                    jukeboxes.end());
    if (playing) jukeboxes.push_back({x, y, z, animTicks});
}

bool Mobs::dancing(Mob& m) { // checked once a second: a jukebox playing within 5 blocks?
    if (m.ticksExisted % 20 == 0) {
        bool near = false;
        for (size_t i = jukeboxes.size(); i-- > 0;) {
            const Jukebox& j = jukeboxes[i];
            if (animTicks - j.since > 190 * 20 || !animWorld || animWorld->getBlockId(j.x, j.y, j.z) != 84) {
                jukeboxes.erase(jukeboxes.begin() + long(i));
                continue;
            }
            double dx = j.x + 0.5 - m.x, dy = j.y + 0.5 - m.y, dz = j.z + 0.5 - m.z;
            if (dx * dx + dy * dy + dz * dz <= 25.0) near = true;
        }
        m.anim.dancing = near;
    }
    return m.anim.dancing;
}

void Mobs::updateAnims(Mob& m) {
    // AxeolonMobAnims.update (and the Axeolon creatures' own onUpdate scripts), standing still
    Animator& a = m.anim;
    if (m.hurtTime > a.lastHurtTime && m.health > 0) a.overlay("hurt", 1);
    a.lastHurtTime = m.hurtTime;
    const float speed = m.limbAmount; // field_704_R

    if (KINDS[m.kind].axeolonCreature) {
        switch (m.kind) {
        case OWL:
            a.play(m.owlFlying ? "fly" : "idle", 4);
            a.walkWeight = m.owlFlying ? 0.0f : 1.0f;
            if (--a.fidgetCooldown <= 0) {
                a.fidgetCooldown = 90 + rand(200);
                if (!m.owlFlying && !a.hasOverlay()) a.overlay(rnd() ? "look_around" : "preen", 4);
            }
            break;
        case GOAT: {
            bool ramming = m.goatWindup > 0 || m.goatCharge > 0;
            a.play(ramming ? "ram" : "idle", 4);
            a.walkWeight = m.goatWindup > 0 ? 0.0f : 1.0f;
            a.lookWeight = ramming ? 0.0f : 1.0f;
            if (--a.fidgetCooldown <= 0) {
                a.fidgetCooldown = 100 + rand(220);
                if (!ramming && speed < 0.08f && !a.hasOverlay()) {
                    int r = rand(4);
                    a.overlay(r == 0 ? "graze" : r == 1 ? "look_around" : r == 2 ? "ear_flick" : "shake", 6);
                }
            }
            break;
        }
        case RACCOON: {
            const char* main = "idle";
            if (m.rState == 2) main = m.raid < 6 ? "idle" : m.raid < 18 ? "pry" : m.raid < 78 ? "rummage" : "pull";
            else if (m.rState == 4) main = "eat";
            a.play(main, 3);
            bool busy = m.rState == 2 || m.rState == 4;
            a.walkWeight = busy ? 0.0f : 1.0f;
            a.lookWeight = busy ? 0.0f : 1.0f;
            if (--a.fidgetCooldown <= 0) {
                a.fidgetCooldown = 100 + rand(200);
                if (m.rState == 0 && speed < 0.08f && !a.hasOverlay()) a.overlay(rnd() ? "look_around" : "scratch", 5);
            }
            break;
        }
        case DIREWOLF:
            a.play(m.sitting ? "sit" : "idle", 8);
            a.walkWeight = m.sitting ? 0.0f : 1.0f;
            a.lookWeight = 1.0f;
            if (--a.fidgetCooldown <= 0) {
                a.fidgetCooldown = 120 + rand(240);
                if (speed < 0.08f && !a.hasOverlay()) {
                    int r = rand(4);
                    // (howl needs night - not tracked yet, so it looks around instead)
                    if (r == 1 && !m.sitting) a.overlay("sniff", 6);
                    else if (r == 2 && m.tamed) a.overlay("wag", 4);
                    else a.overlay("look_around", 6);
                }
            }
            break;
        case TABISH: { // EntityTabish.onUpdate
            bool up = m.standing || (m.awake && !m.inCutscene);
            if (m.busyTicks > 0) --m.busyTicks;
            else if (!up) a.play("sleep", 20);
            else if (!m.inCutscene) a.play("idle", 8);
            a.walkWeight = up && !m.inCutscene ? 1.0f : 0.0f;
            a.lookWeight = up && !m.inCutscene && m.busyTicks <= 0 ? 1.0f : 0.0f;
            break;
        }
        default:
            break;
        }
        return;
    }

    if (m.health <= 0 || m.deathTime > 0) {
        a.play("death", 3);
        a.walkWeight = 0.0f;
        a.lookWeight = 0.0f;
        return;
    }
    a.walkWeight = 1.0f;
    a.lookWeight = 1.0f;
    const char* base = "idle";
    bool still = speed < 0.08f;
    if (m.kind == CHICKEN && !m.onGround && m.my < 0.0) base = "flap";
    else if (m.kind == SQUID) base = m.inWater ? "swim" : "flop";
    else if (m.kind == CREEPER && m.fuse > 0) { base = "fuse"; a.lookWeight = 0.4f; }
    else if (m.kind == GHAST && m.attackCounter > 0) base = "charge";
    else if (m.kind == SPIDER && !m.onGround && m.my > 0.15) { base = "leap"; a.walkWeight = 0.0f; }
    else if (m.kind == SLIME && !m.onGround) base = "jump";
    else if ((m.kind == PIG || m.kind == SHEEP || m.kind == COW || m.kind == CHICKEN) && dancing(m)) {
        base = "dance"; // passive mobs dance next to a playing jukebox
        a.lookWeight = 0.3f;
    }
    a.play(base, !std::strcmp(base, "idle") ? 8 : 4);

    if (m.attackTime > a.lastAttackTime) a.overlay("attack", 2);
    a.lastAttackTime = m.attackTime;
    if (m.kind == GHAST) { // it fired
        if (m.attackCounter < a.lastCounter && a.lastCounter >= 10) a.overlay("attack", 2);
        a.lastCounter = m.attackCounter;
    }

    if (--a.fidgetCooldown <= 0) {
        a.fidgetCooldown = 80 + rand(220);
        const char* const* f = nullptr;
        int n = 0;
        switch (m.kind) {
        case PIG: f = FIDGET_PIG; n = 3; break;
        case COW: f = FIDGET_COW; n = 3; break;
        case SHEEP: f = FIDGET_SHEEP; n = 3; break;
        case CHICKEN: f = FIDGET_CHICKEN; n = 3; break;
        case CREEPER: f = FIDGET_CREEPER; n = 2; break;
        case SPIDER: f = FIDGET_SPIDER; n = 2; break;
        case SKELETON: f = FIDGET_SKELETON; n = 2; break;
        case ZOMBIE: case PIGZOMBIE: case GIANT: case GHOUL: f = FIDGET_ZOMBIE; n = 2; break;
        case SLIME: f = FIDGET_SLIME; n = 1; break;
        default: break;
        }
        if (n > 0 && still && !std::strcmp(base, "idle") && !a.hasOverlay()) a.overlay(f[rand(n)], 6);
    }
}

void Mobs::render(const World& w, Renderer& r, float partial, float camX, float camY, float camZ, float maxDist) {
    for (Mob& m : mobs) {
        // between the last two ticks
        float ix = float(m.px + (m.x - m.px) * partial), iy = float(m.py + (m.y - m.py) * partial),
              iz = float(m.pz + (m.z - m.pz) * partial);
        float dx = ix - camX, dy = iy - camY, dz = iz - camZ;
        if (dx * dx + dy * dy + dz * dz > maxDist * maxDist) continue;
        const KindInfo& k = KINDS[m.kind];
        axm::Model* model = Mobs::model(k.model);
        if (!model) continue;
        // RenderGhast: the mouth glows while it's about to fire
        C3D_Tex* tex = r.texture(m.kind == GHAST && m.attackCounter > 10 ? "romfs:/textures/mob/ghast_fire.rgba" : k.texture);
        if (!tex) continue;

        float bright = BlockModels::lightBrightness(w, int(std::floor(ix)), int(std::floor(iy + k.height * 0.66f)),
                                                    int(std::floor(iz)));
        float bodyYaw = m.prevRenderYaw + (m.renderYaw - m.prevRenderYaw) * partial;
        float headYaw = (m.prevYaw + (m.yaw - m.prevYaw) * partial) - bodyYaw;
        float limbAmount = m.prevLimbAmount + (m.limbAmount - m.prevLimbAmount) * partial;
        if (limbAmount > 1.0f) limbAmount = 1.0f;
        float limbSwing = m.limbSwing - m.limbAmount * (1.0f - partial);
        float sizeScale = m.kind == SLIME ? float(m.slimeSize) : k.scale;

        // RenderLiving.doRenderLiving
        M4 t;
        t.translate(ix, iy, iz);
        if (m.kind == SQUID) { // RenderSquid.func_21007_a: nose up or down as it swims, rolling
            float sp = m.sqPrevPitch + (m.sqPitch - m.sqPrevPitch) * partial;
            float sr = m.sqPrevRoll + (m.sqRoll - m.sqPrevRoll) * partial;
            t.translate(0, 0.5f, 0);
            t.rotate(180.0f - bodyYaw, 1);
            t.rotate(sp, 0);
            t.rotate(sr, 1);
            t.translate(0, -1.2f, 0);
        } else {
            t.rotate(180.0f - bodyYaw, 1);
            if (m.deathTime > 0) {
                float d = std::sqrt((float(m.deathTime) + partial - 1.0f) / 20.0f * 1.6f);
                t.rotate(std::fmin(d, 1.0f) * k.deathAngle, 2);
            }
        }
        t.scale(-1, -1, 1);
        if (m.kind == GHAST) { // RenderGhast.preRenderCallback: it puffs up as it charges
            float f = (float(m.prevAttackCounter) + float(m.attackCounter - m.prevAttackCounter) * partial) / 20.0f;
            if (f < 0.0f) f = 0.0f;
            f = 1.0f / (f * f * f * f * f * 2.0f + 1.0f);
            float sy = (8.0f + f) / 2.0f, sxz = (8.0f + 1.0f / f) / 2.0f;
            t.scale(sxz, sy, sxz);
        } else {
            t.scale(sizeScale, sizeScale, sizeScale);
        }
        float swell = 0.0f; // RenderCreeper.preRenderCallback: it swells before it goes off
        if (m.kind == CREEPER && (m.fuse > 0 || m.prevFuse > 0)) {
            float f = (float(m.prevFuse) + float(m.fuse - m.prevFuse) * partial) / 28.0f;
            float wob = 1.0f + std::sin(f * 100.0f) * f * 0.01f;
            if (f < 0.0f) f = 0.0f;
            if (f > 1.0f) f = 1.0f;
            swell = f;
            f *= f;
            f *= f;
            float sxz = (1.0f + f * 0.4f) * wob, sy = (1.0f + f * 0.1f) / wob;
            t.scale(sxz, sy, sxz);
        }
        if (m.kind == GHOUL) t.translate(0, std::sin((m.ticksExisted + partial) * 0.1f) * 0.08f, 0); // bob
        t.translate(0, -24.0f / 16.0f - 0.0078125f, 0);

        // pose: the animator, then the head looking (head yaw relative to the body is 0 when still)
        auto pose = [&](axm::Model& md) {
            md.resetPose();
            m.anim.applyTo(md, limbSwing, limbAmount, partial);
            md.lookAt(headYaw, m.pitch, m.anim.lookWeight);
        };
        pose(*model);
        WolfBones wb{m.saddle, m.crateLeft, m.crateRight};
        scratch.clear();
        if (m.kind == DIREWOLF) model->emitMob(scratch, t.m, 1.0f / 16.0f, bright, wolfBoneVisible, &wb);
        else model->emitMob(scratch, t.m, 1.0f / 16.0f, bright);

        if (m.kind == GHOUL) { // see-through and full bright
            float tint[4] = {1, 1, 1, 0.55f};
            std::vector<axm::ModelVertex> lit = scratch;
            for (auto& v : lit) v.shade = 1.0f;
            r.drawTriangles(lit, tex, tint, 1);
        } else {
            r.drawTriangles(scratch, tex);
        }
        const std::vector<axm::ModelVertex> body = scratch;

        // second passes (shouldRenderPass)
        if (m.kind == PIG && m.saddle) {
            if (axm::Model* s = Mobs::model("pig_saddle")) {
                pose(*s);
                scratch.clear();
                s->emitMob(scratch, t.m, 1.0f / 16.0f, bright);
                r.drawTriangles(scratch, r.texture("romfs:/textures/mob/saddle.rgba"));
            }
        } else if (m.kind == SHEEP && !m.sheared) {
            if (axm::Model* s = Mobs::model("sheep_fur")) {
                pose(*s);
                scratch.clear();
                s->emitMob(scratch, t.m, 1.0f / 16.0f, 1.0f);
                const float* c = FLEECE[m.fleece];
                float tint[4] = {bright * c[0], bright * c[1], bright * c[2], 1};
                r.drawTriangles(scratch, r.texture("romfs:/textures/mob/sheep_fur.rgba"), tint);
            }
        } else if (m.kind == SPIDER) {
            float tint[4] = {1, 1, 1, (1.0f - bright) * 0.5f};
            std::vector<axm::ModelVertex> eyes = body;
            r.drawTriangles(eyes, r.texture("romfs:/textures/mob/spider_eyes.rgba"), tint, 1);
        } else if (m.kind == SLIME) {
            if (axm::Model* s = Mobs::model("slime_outer")) {
                pose(*s);
                scratch.clear();
                s->emitMob(scratch, t.m, 1.0f / 16.0f, bright);
                r.drawTriangles(scratch, tex, nullptr, 1);
            }
        } else if (m.kind == GHOUL) {
            std::vector<axm::ModelVertex> eyes = body;
            for (auto& v : eyes) v.shade = 1.0f;
            float tint[4] = {1, 1, 1, 1};
            r.drawTriangles(eyes, r.texture("romfs:/axeolon/mob/ghoul_eyes.rgba"), tint, 2);
        } else if (m.kind == DIREWOLF && m.armour > 0) {
            if (!wolfArmour) {
                wolfArmour.reset(new axm::Model);
                if (!wolfArmour->load("romfs:/axeolon/mob/dire_wolf.axm")) wolfArmour.reset();
                else wolfArmour->makeArmour(0.35f, {"torso", "mane", "head", "leg_front_right", "leg_front_left",
                                                     "leg_back_right", "leg_back_left"});
            }
            if (wolfArmour) {
                static const char* const KEYS[4] = {"", "iron", "gold", "diamond"}; // ItemWolfArmour.KEYS
                pose(*wolfArmour);
                scratch.clear();
                wolfArmour->emitMob(scratch, t.m, 1.0f / 16.0f, bright, wolfBoneVisible, &wb);
                r.drawTriangles(scratch, r.texture(std::string("romfs:/axeolon/mob/wolf_armour_") + KEYS[m.armour] + ".rgba"));
            }
        }

        // the creeper's white flash (getColorMultiplier)
        if (swell > 0.0f && int(swell * 10.0f) % 2 != 0) {
            float tint[4] = {1, 1, 1, swell * 0.2f};
            r.drawTriangles(body, nullptr, tint, 1, true, true);
        }
        // hurt / dying: the red flash over the body
        if (m.hurtTime > 0 || m.deathTime > 0) {
            float tint[4] = {1, 0, 0, 0.4f}; // the body is already lit by brightness
            r.drawTriangles(body, nullptr, tint, 1, true, true);
        }
    }
}

#include "world/MathHelper.h"
namespace { constexpr float BOLT_PI = 3.1415927f; }
void Mobs::renderBolts(Renderer& r, float partial, float yaw, float pitch) {
    if (bolts.empty()) return;
    // RenderSpellProjectile: the spell's icon, turned to face the camera
    std::vector<axm::ModelVertex> q;
    float cy = MathHelper::cos(yaw * BOLT_PI / 180.0f), sy = MathHelper::sin(yaw * BOLT_PI / 180.0f);
    float sp = MathHelper::sin(pitch * BOLT_PI / 180.0f), cp = MathHelper::cos(pitch * BOLT_PI / 180.0f);
    // right = (cos yaw, 0, sin yaw); up = (-sin yaw * sin pitch, cos pitch, cos yaw * sin pitch)
    float rx = cy, rz = sy, ux = -sy * sp, uy = cp, uz = cy * sp;
    for (const SpellBolt& b : bolts) {
        float x = float(b.px + (b.x - b.px) * partial), y = float(b.py + (b.y - b.py) * partial),
              z = float(b.pz + (b.z - b.pz) * partial);
        float s = b.spell == FIREBALL ? 1.0f : 0.5f;
        int icon = 224 + b.spell;
        float u0 = float(icon % 16 * 16) / 256.0f, u1 = float(icon % 16 * 16 + 16) / 256.0f;
        float v0 = float(icon / 16 * 16) / 256.0f, v1 = float(icon / 16 * 16 + 16) / 256.0f;
        auto at = [&](float a, float c, float u, float v) {
            return axm::ModelVertex{x + (rx * a + ux * c) * s, y + (uy * c) * s, z + (rz * a + uz * c) * s, 1.0f, u, v};
        };
        axm::ModelVertex v[4] = {at(-0.5f, -0.25f, u0, v1), at(0.5f, -0.25f, u1, v1), at(0.5f, 0.75f, u1, v0), at(-0.5f, 0.75f, u0, v0)};
        static const int T[6] = {0, 1, 2, 0, 2, 3};
        for (int i : T) q.push_back(v[i]);
    }
    r.drawTriangles(q, r.texture("romfs:/textures/gui/items.rgba"));
}

// RenderTNTPrimed: swells in its last half second and flashes white every quarter second
void Mobs::renderTnt(const World& w, Renderer& r, float partial) {
    if (tnts.empty() && falling.empty()) return;
    static const float F[6][4][3] = {
        {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}}, {{-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}},
        {{1, 1, -1}, {-1, 1, -1}, {-1, -1, -1}, {1, -1, -1}}, {{-1, 1, 1}, {1, 1, 1}, {1, -1, 1}, {-1, -1, 1}},
        {{-1, 1, -1}, {-1, 1, 1}, {-1, -1, 1}, {-1, -1, -1}}, {{1, 1, 1}, {1, 1, -1}, {1, -1, -1}, {1, -1, 1}}};
    static const float SH[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    static const int TILE[6] = {10, 9, 8, 8, 8, 8}; // TNT: bottom, top, sides
    std::vector<axm::ModelVertex> tris;
    for (const PrimedTnt& t : tnts) {
        tris.clear();
        float x = float(t.px + (t.x - t.px) * partial), y = float(t.py + (t.y - t.py) * partial),
              z = float(t.pz + (t.z - t.pz) * partial);
        float left = float(t.fuse) - partial + 1.0f;
        float sc = 1.0f;
        if (left < 10.0f) {
            float f = 1.0f - left / 10.0f;
            f = std::max(0.0f, std::min(1.0f, f));
            f *= f;
            f *= f;
            sc = 1.0f + f * 0.3f;
        }
        float h = 0.5f * sc;
        float shade = BlockModels::lightBrightness(w, int(std::floor(x)), int(std::floor(y)), int(std::floor(z)));
        for (int f = 0; f < 6; ++f) {
            int tile = TILE[f];
            float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) * 16.0f / 1024.0f;
            float u1 = u0 + 1 / 16.0f, v1 = v0 + 16.0f / 1024.0f;
            const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
            axm::ModelVertex q[4];
            for (int k = 0; k < 4; ++k) {
                q[k].x = x + F[f][k][0] * h;
                q[k].y = y + F[f][k][1] * h;
                q[k].z = z + F[f][k][2] * h;
                q[k].shade = shade * SH[f];
                q[k].u = uv[k][0];
                q[k].v = uv[k][1];
            }
            tris.insert(tris.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        }
        r.drawTriangles(tris, r.terrainTexture());
        if (t.fuse / 5 % 2 == 0) {
            float a = (1.0f - left / 100.0f) * 0.8f;
            const float tint[4] = {1.0f, 1.0f, 1.0f, std::max(0.0f, a)};
            r.drawTriangles(tris, r.terrainTexture(), tint, 2, true, true);
        }
    }
}

// RenderSnowball: the item's icon facing the camera
void Mobs::renderThrown(Renderer& r, float partial, float yaw, float pitch) {
    if (thrown.empty()) return;
    std::vector<axm::ModelVertex> q;
    float cy = MathHelper::cos(yaw * BOLT_PI / 180.0f), sy = MathHelper::sin(yaw * BOLT_PI / 180.0f);
    float sp = MathHelper::sin(pitch * BOLT_PI / 180.0f), cp = MathHelper::cos(pitch * BOLT_PI / 180.0f);
    float rx = cy, rz = sy, ux = -sy * sp, uy = cp, uz = cy * sp;
    for (const Thrown& t : thrown) {
        float x = float(t.px + (t.x - t.px) * partial), y = float(t.py + (t.y - t.py) * partial),
              z = float(t.pz + (t.z - t.pz) * partial);
        int icon = t.kind == T_EGG ? 12 : 14; // Item.egg (12, 0), Item.snowball (14, 0)
        float u0 = float(icon % 16 * 16) / 256.0f, u1 = u0 + 1.0f / 16.0f;
        float v0 = float(icon / 16 * 16) / 256.0f, v1 = v0 + 1.0f / 16.0f;
        const float s = 0.25f;
        auto at = [&](float a, float c, float u, float v) {
            return axm::ModelVertex{x + (rx * a + ux * c) * s, y + (uy * c) * s, z + (rz * a + uz * c) * s, 1.0f, u, v};
        };
        axm::ModelVertex v[4] = {at(-1, -1, u0, v1), at(1, -1, u1, v1), at(1, 1, u1, v0), at(-1, 1, u0, v0)};
        static const int T[6] = {0, 1, 2, 0, 2, 3};
        for (int i : T) q.push_back(v[i]);
    }
    r.drawTriangles(q, r.texture("romfs:/textures/gui/items.rgba"));
}

// RenderFireball: the snowball icon, twice the size, facing the camera
void Mobs::renderFireballs(Renderer& r, float partial, float yaw, float pitch) {
    if (fireballs.empty()) return;
    std::vector<axm::ModelVertex> q;
    float cy = MathHelper::cos(yaw * BOLT_PI / 180.0f), sy = MathHelper::sin(yaw * BOLT_PI / 180.0f);
    float sp = MathHelper::sin(pitch * BOLT_PI / 180.0f), cp = MathHelper::cos(pitch * BOLT_PI / 180.0f);
    float rx = cy, rz = sy, ux = -sy * sp, uy = cp, uz = cy * sp;
    for (const Fireball& f : fireballs) {
        float x = float(f.px + (f.x - f.px) * partial), y = float(f.py + (f.y - f.py) * partial),
              z = float(f.pz + (f.z - f.pz) * partial);
        const int icon = 14; // Item.snowball
        float u0 = float(icon % 16 * 16) / 256.0f, u1 = u0 + 1.0f / 16.0f;
        float v0 = float(icon / 16 * 16) / 256.0f, v1 = v0 + 1.0f / 16.0f;
        const float s = 2.0f;
        auto at = [&](float a, float c, float u, float v) {
            return axm::ModelVertex{x + (rx * a + ux * c) * s, y + (uy * c) * s, z + (rz * a + uz * c) * s, 1.0f, u, v};
        };
        axm::ModelVertex v[4] = {at(-0.5f, -0.25f, u0, v1), at(0.5f, -0.25f, u1, v1), at(0.5f, 0.75f, u1, v0), at(-0.5f, 0.75f, u0, v0)};
        static const int T[6] = {0, 1, 2, 0, 2, 3};
        for (int i : T) q.push_back(v[i]);
    }
    r.drawTriangles(q, r.texture("romfs:/textures/gui/items.rgba"));
}

// EntityFallingIcicle's renderer: the icicle's crossed sprites (body, or the tip for the lowest piece)
void Mobs::renderIcicles(Renderer& r, float partial) {
    if (icicles.empty()) return;
    std::vector<axm::ModelVertex> q;
    for (const Icicle& ic : icicles) {
        float x = float(ic.x), y = float(ic.py + (ic.y - ic.py) * partial), z = float(ic.z);
        int tile = ic.tip ? 253 : 175; // left half of each tile
        float u0 = (tile & 15) / 16.0f, u1 = u0 + 0.5f / 16.0f;
        float v0 = (tile >> 4) * 16.0f / 1024.0f, v1 = v0 + 16.0f / 1024.0f;
        const float a = 0.35f;
        const float d[2][2] = {{a, a}, {a, -a}};
        for (int k = 0; k < 2; ++k) {
            float dx = d[k][0], dz = d[k][1];
            axm::ModelVertex v[4] = {{x - dx, y, z - dz, 1.0f, u0, v1}, {x + dx, y, z + dz, 1.0f, u1, v1},
                                     {x + dx, y + 1.0f, z + dz, 1.0f, u1, v0}, {x - dx, y + 1.0f, z - dz, 1.0f, u0, v0}};
            static const int T[12] = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2}; // both sides
            for (int i : T) q.push_back(v[i]);
        }
    }
    r.drawTriangles(q, r.terrainTexture());
}

// RenderFallingSand: the block, as a cube, where the entity is
void Mobs::renderFalling(const World& w, Renderer& r, float partial) {
    if (falling.empty()) return;
    static const float F[6][4][3] = {
        {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}}, {{-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}},
        {{1, 1, -1}, {-1, 1, -1}, {-1, -1, -1}, {1, -1, -1}}, {{-1, 1, 1}, {1, 1, 1}, {1, -1, 1}, {-1, -1, 1}},
        {{-1, 1, -1}, {-1, 1, 1}, {-1, -1, 1}, {-1, -1, -1}}, {{1, 1, 1}, {1, 1, -1}, {1, -1, -1}, {1, -1, 1}}};
    static const float SH[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    std::vector<axm::ModelVertex> tris;
    for (const Falling& f : falling) {
        float x = float(f.px + (f.x - f.px) * partial), y = float(f.py + (f.y - f.py) * partial), z = float(f.pz + (f.z - f.pz) * partial);
        float shade = BlockModels::lightBrightness(w, int(std::floor(x)), int(std::floor(y)), int(std::floor(z)));
        for (int s = 0; s < 6; ++s) {
            int tile = BLOCKS[f.id].tex[0][s];
            float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) * 16.0f / 1024.0f;
            float u1 = u0 + 1 / 16.0f, v1 = v0 + 16.0f / 1024.0f;
            const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
            axm::ModelVertex q[4];
            for (int k = 0; k < 4; ++k) {
                q[k].x = x + F[s][k][0] * 0.5f;
                q[k].y = y + F[s][k][1] * 0.5f;
                q[k].z = z + F[s][k][2] * 0.5f;
                q[k].shade = shade * SH[s];
                q[k].u = uv[k][0];
                q[k].v = uv[k][1];
            }
            tris.insert(tris.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        }
    }
    r.drawTriangles(tris, r.terrainTexture());
}
