#include "ItemEntities.h"
#include <algorithm>
#include <cmath>
#include "ItemTable.h"
#include "Survival.h"
#include "render/Renderer.h"
#include "world/BlockProps.h"
#include "world/BlockTable.h"
#include "world/Chunk.h"
#include "world/World.h"
#include "model/BlockModels.h"

namespace {
const double HALF = 0.125; // setSize(0.25, 0.25)
inline int fl(double v) { int i = int(v); return v < i ? i - 1 : i; }
AABB box(const ItemEntity& e) { return {e.x - HALF, e.y, e.z - HALF, e.x + HALF, e.y + 0.25, e.z + HALF}; }
double rnd() { return double(std::rand()) / RAND_MAX; } // Math.random
} // namespace

void ItemEntities::spawn(double x, double y, double z, const ItemStack& s, int delay) {
    ItemEntity e{};
    // EntityItem(World, x, y, z, stack): yOffset = height / 2, so posY is the middle
    e.x = e.px = x;
    e.y = e.py = y - 0.125;
    e.z = e.pz = z;
    e.yaw = float(rnd() * 360.0);
    e.mx = double(float(rnd() * double(0.2f) - double(0.1f)));
    e.my = double(0.2f);
    e.mz = double(float(rnd() * double(0.2f) - double(0.1f)));
    e.spin = float(rnd() * 3.141592653589793 * 2.0);
    e.pickupDelay = delay;
    e.item = s;
    items.push_back(e);
}

void ItemEntities::spawnMoving(double x, double y, double z, const ItemStack& s, double mx, double my, double mz,
                               int delay) {
    spawn(x, y + 0.125, z, s, delay);
    ItemEntity& e = items.back();
    e.mx = mx;
    e.my = my;
    e.mz = mz;
}

void ItemEntities::spawnFromWorld(World& w) {
    for (const World::Drop& d : w.drops) {
        ItemStack s;
        s.id = uint16_t(d.id);
        s.count = d.count;
        s.damage = int16_t(d.damage);
        s.tag = d.tag;
        spawn(d.x, d.y, d.z, s, 10);
    }
    w.drops.clear();
}

// Entity.moveEntity, without the step-up (items don't climb)
void ItemEntities::move(World& w, ItemEntity& e, double dx, double dy, double dz) {
    AABB bb = box(e);
    double ox = dx, oy = dy, oz = dz;
    boxes.clear();
    Physics::collidingBoxes(w, bb.addCoord(dx, dy, dz), boxes);
    for (auto& b : boxes) dy = b.calculateYOffset(bb, dy);
    bb = bb.offset(0, dy, 0);
    for (auto& b : boxes) dx = b.calculateXOffset(bb, dx);
    bb = bb.offset(dx, 0, 0);
    for (auto& b : boxes) dz = b.calculateZOffset(bb, dz);
    bb = bb.offset(0, 0, dz);
    e.x = (bb.minX + bb.maxX) / 2.0;
    e.y = bb.minY;
    e.z = (bb.minZ + bb.maxZ) / 2.0;
    e.onGround = oy != dy && oy < 0.0;
    if (ox != dx) e.mx = 0.0;
    if (oy != dy) e.my = 0.0;
    if (oz != dz) e.mz = 0.0;
}

void ItemEntities::tick(World& w, const AABB& player, Inventory& inv, bool& picked) {
    picked = false;
    for (size_t i = 0; i < items.size();) {
        ItemEntity& e = items[i];
        if (!w.isChunkLoaded(fl(e.x) >> 4, fl(e.z) >> 4)) { ++i; continue; } // frozen at the edge
        if (e.pickupDelay > 0) --e.pickupDelay;
        e.px = e.x;
        e.py = e.y;
        e.pz = e.z;
        e.my -= double(0.04f);
        double cy = e.y + 0.125; // posY
        int bx = fl(e.x), by = fl(cy), bz = fl(e.z);
        if (BP[w.getBlockId(bx, by, bz)].mat == Mat::lava) {
            e.my = double(0.2f);
            float a = rand.nextFloat(), b = rand.nextFloat();
            e.mx = double((a - b) * 0.2f);
            a = rand.nextFloat();
            b = rand.nextFloat();
            e.mz = double((a - b) * 0.2f);
            rand.nextFloat();
        }
        // func_466_g: pushed out of a solid block toward the nearest open side
        if (BP[w.getBlockId(bx, by, bz)].opaqueLookup) {
            double fx = e.x - bx, fy = cy - by, fz = e.z - bz;
            bool o[6] = {!BP[w.getBlockId(bx - 1, by, bz)].opaqueLookup, !BP[w.getBlockId(bx + 1, by, bz)].opaqueLookup,
                         !BP[w.getBlockId(bx, by - 1, bz)].opaqueLookup, !BP[w.getBlockId(bx, by + 1, bz)].opaqueLookup,
                         !BP[w.getBlockId(bx, by, bz - 1)].opaqueLookup, !BP[w.getBlockId(bx, by, bz + 1)].opaqueLookup};
            double d[6] = {fx, 1 - fx, fy, 1 - fy, fz, 1 - fz};
            int side = -1;
            double best = 9999.0;
            for (int k = 0; k < 6; ++k)
                if (o[k] && d[k] < best) { best = d[k]; side = k; }
            float v = rand.nextFloat() * 0.2f + 0.1f;
            if (side == 0) e.mx = -v;
            if (side == 1) e.mx = v;
            if (side == 2) e.my = -v;
            if (side == 3) e.my = v;
            if (side == 4) e.mz = -v;
            if (side == 5) e.mz = v;
        }
        // Entity.handleWaterMovement: the current carries it
        Physics::waterAcceleration(w, box(e).expand(0.0, double(-0.4f), 0.0), e.mx, e.my, e.mz);
        move(w, e, e.mx, e.my, e.mz);
        float f = 0.98f;
        if (e.onGround) {
            f = 0.1f * 0.1f * 58.8f;
            int below = w.getBlockId(fl(e.x), fl(e.y) - 1, fl(e.z));
            if (below > 0) f = Physics::slipperiness(below) * 0.98f;
        }
        e.mx *= double(f);
        e.my *= double(0.98f);
        e.mz *= double(f);
        if (e.onGround) e.my *= -0.5;
        ++e.age;
        bool dead = e.age >= 6000;
        // EntityPlayer.onLivingUpdate -> EntityItem.onCollideWithPlayer
        if (!dead && e.pickupDelay == 0 && box(e).intersects(player.expand(1.0, 0.0, 1.0))) {
            int before = e.item.count;
            inv.addStack(e.item);
            if (e.item.count != before) picked = true;
            if (e.item.count <= 0) dead = true;
        }
        if (dead) {
            items[i] = items.back();
            items.pop_back();
        } else {
            ++i;
        }
    }
}

void ItemEntities::render(World& w, Renderer& r, float partial, float camYaw, float camPitch) {
    if (items.empty()) return;
    C3D_Tex* itemsTex = r.texture("romfs:/textures/gui/items.rgba");
    // camera right / up for the billboards (RenderItem turns flat items to face the viewer)
    float rx = std::cos(camYaw), rz = std::sin(camYaw);
    float ux = std::sin(camPitch) * std::sin(camYaw), uy = std::cos(camPitch), uz = -std::sin(camPitch) * std::cos(camYaw);
    std::vector<axm::ModelVertex> tris;
    for (const ItemEntity& e : items) {
        double x = e.px + (e.x - e.px) * partial, y = e.py + (e.y - e.py) * partial, z = e.pz + (e.z - e.pz) * partial;
        float t = float(e.age) + partial;
        float bob = std::sin(t / 10.0f + e.spin) * 0.1f + 0.1f;
        float shade = BlockModels::lightBrightness(w, fl(x), fl(y + 0.2), fl(z));
        const ItemInfo* info = itemInfo(e.item.id, e.item.damage);
        tris.clear();
        bool cube = e.item.id < 256 && (BLOCKS[e.item.id].flags & BF_CUBE) && info && (info->flags & IF_BLOCK3D);
        float cy = float(y) + bob + 0.125f;
        if (cube) {
            // a small spinning block (RenderItem: 1/4 scale, (age + partial) / 20 * 57.3 degrees)
            float ang = t / 20.0f;
            float c = std::cos(ang), s = std::sin(ang), h = 0.125f;
            auto P = [&](float px, float py, float pz, float& ox, float& oy, float& oz) {
                ox = float(x) + px * c - pz * s;
                oy = cy + py;
                oz = float(z) + px * s + pz * c;
            };
            static const float F[6][4][3] = {
                {{-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}}, {{-1, 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}},
                {{1, 1, -1}, {-1, 1, -1}, {-1, -1, -1}, {1, -1, -1}}, {{-1, 1, 1}, {1, 1, 1}, {1, -1, 1}, {-1, -1, 1}},
                {{-1, 1, -1}, {-1, 1, 1}, {-1, -1, 1}, {-1, -1, -1}}, {{1, 1, 1}, {1, 1, -1}, {1, -1, -1}, {1, -1, 1}}};
            static const float SH[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
            for (int f = 0; f < 6; ++f) {
                int tile = BLOCKS[e.item.id].tex[e.item.damage & 15][f];
                float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) * 16.0f / 1024.0f;
                float u1 = u0 + 1 / 16.0f, v1 = v0 + 16.0f / 1024.0f;
                const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
                axm::ModelVertex q[4];
                for (int k = 0; k < 4; ++k) {
                    P(F[f][k][0] * h, F[f][k][1] * h, F[f][k][2] * h, q[k].x, q[k].y, q[k].z);
                    q[k].shade = shade * SH[f];
                    q[k].u = uv[k][0];
                    q[k].v = uv[k][1];
                }
                tris.insert(tris.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
            }
            r.drawTriangles(tris, r.terrainTexture());
            continue;
        }
        // flat sprite, half a block wide
        int icon;
        C3D_Tex* tex;
        float vScale;
        if (e.item.id < 256) {
            icon = info && info->icon >= 0 ? info->icon : BLOCKS[e.item.id].tex[e.item.damage & 15][2];
            if (info && (info->flags & IF_TERRAIN2)) icon = 256 + (icon & 255);
            tex = r.terrainTexture();
            vScale = 16.0f / 1024.0f;
        } else {
            if (!info || info->icon < 0) continue;
            icon = info->icon;
            tex = itemsTex;
            vScale = 1.0f / 16.0f;
        }
        if (!tex) continue;
        float u0 = (icon & 15) / 16.0f, v0 = (icon >> 4) * vScale, u1 = u0 + 1 / 16.0f, v1 = v0 + vScale;
        float s = 0.25f;
        float bx = float(x), by = cy, bz = float(z);
        axm::ModelVertex q[4] = {
            {bx - rx * s - ux * s, by - uy * s, bz - rz * s - uz * s, shade, u0, v1},
            {bx + rx * s - ux * s, by - uy * s, bz + rz * s - uz * s, shade, u1, v1},
            {bx + rx * s + ux * s, by + uy * s, bz + rz * s + uz * s, shade, u1, v0},
            {bx - rx * s + ux * s, by + uy * s, bz - rz * s + uz * s, shade, u0, v0}};
        tris.insert(tris.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
        uint32_t col = info && e.item.id >= 256 ? info->colour : 0xFFFFFF;
        float tint[4] = {((col >> 16) & 255) / 255.0f, ((col >> 8) & 255) / 255.0f, (col & 255) / 255.0f, 1.0f};
        r.drawTriangles(tris, tex, col == 0xFFFFFF ? nullptr : tint);
    }
}

void ItemEntities::chunkLoaded(Chunk& c) {
    if (!c.level) return;
    nbt::Tag* list = c.level->getList("Entities");
    if (!list) return;
    auto& v = list->items;
    for (size_t i = 0; i < v.size();) {
        nbt::Tag& t = *v[i];
        if (t.getString("id") != "Item") { ++i; continue; }
        ItemEntity e{};
        if (nbt::Tag* p = t.getList("Pos"); p && p->items.size() == 3) {
            e.x = e.px = p->items[0]->d;
            e.y = e.py = p->items[1]->d - 0.125; // saved posY is the middle (yOffset)
            e.z = e.pz = p->items[2]->d;
        }
        if (nbt::Tag* m = t.getList("Motion"); m && m->items.size() == 3) {
            e.mx = m->items[0]->d;
            e.my = m->items[1]->d;
            e.mz = m->items[2]->d;
        }
        if (nbt::Tag* r = t.getList("Rotation"); r && r->items.size() == 2) e.yaw = float(r->items[0]->d);
        e.onGround = t.getBool("OnGround");
        e.health = t.getShort("Health") & 255;
        e.age = t.getShort("Age");
        e.spin = float(rnd() * 6.283185307179586);
        if (nbt::Tag* it = t.getCompound("Item")) {
            e.item.id = uint16_t(it->getShort("id"));
            e.item.count = it->getByte("Count");
            e.item.damage = it->getShort("Damage");
            if (nbt::Tag* tag = it->getCompound("tag")) e.item.tag = std::shared_ptr<nbt::Tag>(nbt::clone(*tag).release());
        }
        if (!e.item.empty()) items.push_back(e);
        v.erase(v.begin() + i);
    }
}

void ItemEntities::writeToChunk(Chunk& c) const {
    if (!c.level) c.level = nbt::makeCompound();
    nbt::Tag* list = c.level->getList("Entities");
    if (!list) {
        c.level->set("Entities", nbt::makeList(nbt::Compound));
        list = c.level->getList("Entities");
    }
    auto& v = list->items;
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(), [](const nbt::TagPtr& t) { return t->getString("id") == "Item"; }), v.end());
    bool changed = v.size() != before;
    for (const ItemEntity& e : items) {
        if ((fl(e.x) >> 4) != c.xPosition || (fl(e.z) >> 4) != c.zPosition) continue;
        // Entity.writeToNBT + EntityItem.writeEntityToNBT
        nbt::TagPtr t = nbt::makeCompound();
        t->setString("id", "Item");
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(e.x));
        pos->items.push_back(nbt::makeDouble(e.y + 0.125));
        pos->items.push_back(nbt::makeDouble(e.z));
        t->set("Pos", std::move(pos));
        nbt::TagPtr mo = nbt::makeList(nbt::Double);
        mo->items.push_back(nbt::makeDouble(e.mx));
        mo->items.push_back(nbt::makeDouble(e.my));
        mo->items.push_back(nbt::makeDouble(e.mz));
        t->set("Motion", std::move(mo));
        nbt::TagPtr ro = nbt::makeList(nbt::Float);
        ro->items.push_back(nbt::makeFloat(e.yaw));
        ro->items.push_back(nbt::makeFloat(0.0f));
        t->set("Rotation", std::move(ro));
        t->setFloat("FallDistance", 0.0f);
        t->setShort("Fire", -1);
        t->setShort("Air", 300);
        t->setBool("OnGround", e.onGround);
        t->setShort("Health", int16_t(e.health));
        t->setShort("Age", int16_t(e.age));
        nbt::Tag& it = t->setCompound("Item");
        it.setShort("id", int16_t(e.item.id));
        it.setByte("Count", int8_t(e.item.count));
        it.setShort("Damage", e.item.damage);
        if (e.item.tag) it.set("tag", nbt::clone(*e.item.tag));
        list->listType = nbt::Compound;
        v.push_back(std::move(t));
        changed = true;
    }
    if (changed) c.modified = true;
}

void ItemEntities::chunkUnloaded(int cx, int cz) {
    items.erase(std::remove_if(items.begin(), items.end(),
                               [&](const ItemEntity& e) { return (fl(e.x) >> 4) == cx && (fl(e.z) >> 4) == cz; }),
                items.end());
}

bool ItemEntities::anyIn(const AABB& b) const {
    for (const ItemEntity& e : items) {
        AABB eb{e.x - 0.125, e.y, e.z - 0.125, e.x + 0.125, e.y + 0.25, e.z + 0.125};
        if (eb.intersects(b)) return true;
    }
    return false;
}

void ItemEntities::launch(const AABB& b, double mx, double my, double mz) {
    for (ItemEntity& e : items) {
        AABB eb{e.x - 0.125, e.y, e.z - 0.125, e.x + 0.125, e.y + 0.25, e.z + 0.125};
        if (eb.intersects(b)) { e.mx += mx; e.my += my; e.mz += mz; }
    }
}
