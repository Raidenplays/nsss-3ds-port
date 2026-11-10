#include "Mobs.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <memory>
#include "render/AlphaModels.h"
#include "render/Renderer.h"
#include "world/World.h"
#include "world/BlockTable.h"
#include "world/Chunk.h"
#include "world/MathHelper.h"

namespace {

struct KindInfo {
    const char* id;      // EntityList save name
    const char* texture; // getEntityTexture
    float deathAngle;    // RenderLiving.func_172_a
};

// Same order as Mobs::Kind
const KindInfo KINDS[] = {
    {"Pig", "romfs:/textures/mob/pig.rgba", 90},
    {"Sheep", "romfs:/textures/mob/sheep.rgba", 90},
    {"Cow", "romfs:/textures/mob/cow.rgba", 90},
    {"Chicken", "romfs:/textures/mob/chicken.rgba", 90},
    {"Creeper", "romfs:/textures/mob/creeper.rgba", 90},
    {"Skeleton", "romfs:/textures/mob/skeleton.rgba", 90},
    {"Zombie", "romfs:/textures/mob/zombie.rgba", 90},
    {"Spider", "romfs:/textures/mob/spider.rgba", 180},
    {"Slime", "romfs:/textures/mob/slime.rgba", 90},
    {"Giant", "romfs:/textures/mob/zombie.rgba", 90},
};

// RenderManager's renderers and their models
struct Models {
    alphamodel::Pig pig, saddle{0.5f};
    alphamodel::Sheep2 sheep;
    alphamodel::Sheep1 wool;
    alphamodel::Cow cow;
    alphamodel::Chicken chicken;
    alphamodel::Creeper creeper;
    alphamodel::Skeleton skeleton;
    alphamodel::Zombie zombie, giant;
    alphamodel::Spider spider, spiderEyes;
    alphamodel::Slime slime{16}, slimeOuter{0};
};
std::unique_ptr<Models> gModels;

} // namespace

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
    m.slimeSize = e->getInt("Size") + 1;
    initMob(m);
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
        } else if (m.kind == PIG) {
            t->setBool("Saddle", m.saddle);
        } else if (m.kind == SLIME) {
            t->setInt("Size", m.slimeSize - 1);
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

#include "world/MathHelper.h"
namespace { constexpr float BOLT_PI = 3.1415927f; }
// RenderLiving.func_171_a and the renderers RenderManager gives each mob
void Mobs::render(const World& w, Renderer& r, float partial, float camX, float camY, float camZ, float maxDist) {
    if (!gModels) gModels.reset(new Models);
    Models& md = *gModels;
    const float f7 = 0.0625f;
    for (Mob& m : mobs) {
        float ix = float(m.px + (m.x - m.px) * partial), iy = float(m.py + (m.y - m.py) * partial),
              iz = float(m.pz + (m.z - m.pz) * partial);
        float dx = ix - camX, dy = iy - camY, dz = iz - camZ;
        if (dx * dx + dy * dy + dz * dz > maxDist * maxDist) continue;
        const KindInfo& k = KINDS[m.kind];
        C3D_Tex* tex = r.texture(k.texture);
        if (!tex) continue;
        alphamodel::Model* main = nullptr;
        alphamodel::Model* pass = nullptr; // field_202_e, for the second passes
        switch (m.kind) {
        case PIG: main = &md.pig; pass = &md.saddle; break;
        case SHEEP: main = &md.sheep; pass = &md.wool; break;
        case COW: main = &md.cow; break;
        case CHICKEN: main = &md.chicken; break;
        case CREEPER: main = &md.creeper; break;
        case SKELETON: main = &md.skeleton; break;
        case ZOMBIE: main = &md.zombie; break;
        case SPIDER: main = &md.spider; pass = &md.spiderEyes; break;
        case SLIME: main = &md.slime; pass = &md.slimeOuter; break;
        case GIANT: main = &md.giant; break;
        default: continue;
        }
        // Entity.getEntityBrightness: the light at the eyes (posY - yOffset + height * 0.66)
        float bright = w.getBrightness(int(std::floor(ix)), int(std::floor(iy + m.height * 0.66f)), int(std::floor(iz)));
        float f2 = m.prevRenderYaw + (m.renderYaw - m.prevRenderYaw) * partial;
        float f3 = m.prevYaw + (m.yaw - m.prevYaw) * partial;
        float f4 = m.prevPitch + (m.pitch - m.prevPitch) * partial;
        float f5 = float(m.ticksExisted) + partial; // func_170_d
        if (m.kind == CHICKEN) { // RenderChicken: the wings beat
            float wing = m.prevWing + (m.wing - m.prevWing) * partial;
            float speed = m.prevFlapSpeed + (m.flapSpeed - m.prevFlapSpeed) * partial;
            f5 = (MathHelper::sin(wing) + 1.0f) * speed;
        }
        main->onGround = 0.0f; // getSwingProgress: mobs don't swing
        main->riding = false;
        for (const Mob& o : mobs)
            if (o.vehicle && o.vehicle == m.uid && !o.dead) main->riding = true; // entityBeingRidden
        if (pass) { pass->riding = main->riding; pass->onGround = main->onGround; }

        alphamodel::M4 t;
        t.translate(ix, iy, iz);
        t.rotate(180.0f - f2, 0, 1, 0);
        if (m.deathTime > 0) {
            float d = MathHelper::sqrt_float((float(m.deathTime) + partial - 1.0f) / 20.0f * 1.6f);
            if (d > 1.0f) d = 1.0f;
            t.rotate(d * k.deathAngle, 0, 0, 1);
        }
        t.scale(-1.0f, -1.0f, 1.0f);
        float flash = 0.0f;
        if (m.kind == CREEPER) { // RenderCreeper.func_184_a: it swells before it goes off
            float f1 = (float(m.prevFuse) + float(m.fuse - m.prevFuse) * partial) / 28.0f; // setCreeperFlashTime
            flash = f1;
            float wob = 1.0f + MathHelper::sin(f1 * 100.0f) * f1 * 0.01f;
            if (f1 < 0.0f) f1 = 0.0f;
            if (f1 > 1.0f) f1 = 1.0f;
            f1 *= f1;
            f1 *= f1;
            float sxz = (1.0f + f1 * 0.4f) * wob, sy = (1.0f + f1 * 0.1f) / wob;
            t.scale(sxz, sy, sxz);
        } else if (m.kind == SLIME) { // RenderSlime.func_178_a: squashed as it lands, stretched as it jumps
            float sq = (m.prevSquish + (m.squish - m.prevSquish) * partial) / (float(m.slimeSize) * 0.5f + 1.0f);
            float g = 1.0f / (sq + 1.0f), sz = float(m.slimeSize);
            t.scale(g * sz, 1.0f / g * sz, g * sz);
        } else if (m.kind == GIANT) { // RenderZombieSimple
            t.scale(6.0f, 6.0f, 6.0f);
        }
        t.translate(0.0f, -24.0f * f7 - 0.0078125f, 0.0f);
        float f8 = m.prevLimbAmount + (m.limbAmount - m.prevLimbAmount) * partial;
        t.translate(0.0f, -24.0f * f7 + 1.4f, 0.0f); // setBobbing (no bob for mobs)
        float f9 = m.limbSwing - m.limbAmount * (1.0f - partial);
        if (f8 > 1.0f) f8 = 1.0f;

        scratch.clear();
        main->setRotationAnglesAndRender(scratch, t, f9, f8, f5, f3 - f2, f4, f7, bright);
        r.drawTriangles(scratch, tex);
        // func_166_a: the second pass (pass 0 only)
        std::vector<QuadVertex> second;
        if (m.kind == PIG && m.saddle) {
            pass->setRotationAnglesAndRender(second, t, f9, f8, f5, f3 - f2, f4, f7, bright);
            r.drawTriangles(second, r.texture("romfs:/textures/mob/saddle.rgba"));
        } else if (m.kind == SHEEP && !m.sheared) {
            pass->setRotationAnglesAndRender(second, t, f9, f8, f5, f3 - f2, f4, f7, bright);
            r.drawTriangles(second, r.texture("romfs:/textures/mob/sheep_fur.rgba"));
        } else if (m.kind == SPIDER) { // the eyes, brighter in the dark
            pass->setRotationAnglesAndRender(second, t, f9, f8, f5, f3 - f2, f4, f7, 1.0f);
            float tint[4] = {1, 1, 1, (1.0f - bright) * 0.5f};
            r.drawTriangles(second, r.texture("romfs:/textures/mob/spider_eyes.rgba"), tint, 1);
        } else if (m.kind == SLIME) { // the see-through outside
            pass->setRotationAnglesAndRender(second, t, f9, f8, f5, f3 - f2, f4, f7, bright);
            r.drawTriangles(second, tex, nullptr, 1);
        }
        // func_173_a (the creeper's white flash) and the red hurt flash, over the same shapes
        int j = 0;
        if (m.kind == CREEPER && int(flash * 10.0f) % 2 != 0) {
            int a = int(flash * 0.2f * 255.0f);
            j = std::max(0, std::min(255, a)) << 24 | 0xFFFFFF;
        }
        if ((j >> 24 & 0xFF) > 0 || m.hurtTime > 0 || m.deathTime > 0) {
            // the shapes again, lit but untextured, in the flash's colour (glColor4f with the lights on)
            std::vector<QuadVertex> both;
            main->setRotationAnglesAndRender(both, t, f9, f8, f5, f3 - f2, f4, f7, 1.0f);
            if (!second.empty() && m.kind != SPIDER) pass->setRotationAnglesAndRender(both, t, f9, f8, f5, f3 - f2, f4, f7, 1.0f);
            if (m.hurtTime > 0 || m.deathTime > 0) {
                float tint[4] = {bright, 0.0f, 0.0f, 0.4f};
                r.drawTriangles(both, nullptr, tint, 1, true, true);
            }
            if ((j >> 24 & 0xFF) > 0) {
                float tint[4] = {float(j >> 16 & 0xFF) / 255.0f, float(j >> 8 & 0xFF) / 255.0f, float(j & 0xFF) / 255.0f,
                                 float(j >> 24 & 0xFF) / 255.0f};
                r.drawTriangles(both, nullptr, tint, 1, true, true);
            }
        }
    }
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
    std::vector<QuadVertex> tris;
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
        float shade = w.getBrightness(int(std::floor(x)), int(std::floor(y)), int(std::floor(z)));
        for (int f = 0; f < 6; ++f) {
            int tile = TILE[f];
            float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) / 16.0f;
            float u1 = u0 + 1 / 16.0f, v1 = v0 + 1 / 16.0f;
            const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
            QuadVertex q[4];
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
    std::vector<QuadVertex> q;
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
            return QuadVertex{x + (rx * a + ux * c) * s, y + (uy * c) * s, z + (rz * a + uz * c) * s, 1.0f, u, v};
        };
        QuadVertex v[4] = {at(-1, -1, u0, v1), at(1, -1, u1, v1), at(1, 1, u1, v0), at(-1, 1, u0, v0)};
        static const int T[6] = {0, 1, 2, 0, 2, 3};
        for (int i : T) q.push_back(v[i]);
    }
    r.drawTriangles(q, r.texture("romfs:/textures/gui/items.rgba"));
}

// RenderFallingSand: the block, as a cube, where the entity is
void Mobs::renderFalling(const World& w, Renderer& r, float partial) {
    if (falling.empty()) return;
    static const float F[6][4][3] = {
        {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}}, {{-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}},
        {{1, 1, -1}, {-1, 1, -1}, {-1, -1, -1}, {1, -1, -1}}, {{-1, 1, 1}, {1, 1, 1}, {1, -1, 1}, {-1, -1, 1}},
        {{-1, 1, -1}, {-1, 1, 1}, {-1, -1, 1}, {-1, -1, -1}}, {{1, 1, 1}, {1, 1, -1}, {1, -1, -1}, {1, -1, 1}}};
    static const float SH[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    std::vector<QuadVertex> tris;
    for (const Falling& f : falling) {
        float x = float(f.px + (f.x - f.px) * partial), y = float(f.py + (f.y - f.py) * partial), z = float(f.pz + (f.z - f.pz) * partial);
        float shade = w.getBrightness(int(std::floor(x)), int(std::floor(y)), int(std::floor(z)));
        for (int s = 0; s < 6; ++s) {
            int tile = BLOCKS[f.id].tex[0][s];
            float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) / 16.0f;
            float u1 = u0 + 1 / 16.0f, v1 = v0 + 1 / 16.0f;
            const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
            QuadVertex q[4];
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
