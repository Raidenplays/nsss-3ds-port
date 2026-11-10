#include "Paintings.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include "ItemEntities.h"
#include "Inventory.h"
#include "render/Vertex.h"
#include "render/Renderer.h"
#include "world/BlockProps.h"
#include "world/Chunk.h"
#include "world/Nbt.h"
#include "world/World.h"

namespace {
struct Art { const char* title; int w, h, u, v; }; // EnumArt
const Art ARTS[] = {
    {"Kebab", 16, 16, 0, 0},       {"Aztec", 16, 16, 16, 0},     {"Alban", 16, 16, 32, 0},
    {"Aztec2", 16, 16, 48, 0},     {"Bomb", 16, 16, 64, 0},      {"Plant", 16, 16, 80, 0},
    {"Wasteland", 16, 16, 96, 0},  {"Pool", 32, 16, 0, 32},      {"Courbet", 32, 16, 32, 32},
    {"Sea", 32, 16, 64, 32},       {"Sunset", 32, 16, 96, 32},   {"Creebet", 32, 16, 128, 32},
    {"Wanderer", 16, 32, 0, 64},   {"Graham", 16, 32, 16, 64},   {"Match", 32, 32, 0, 128},
    {"Bust", 32, 32, 32, 128},     {"Stage", 32, 32, 64, 128},   {"Void", 32, 32, 96, 128},
    {"SkullAndRoses", 32, 32, 128, 128}, {"Fighters", 64, 32, 0, 96}, {"Pointer", 64, 64, 0, 192},
    {"Pigscene", 64, 64, 64, 192}, {"BurningSkull", 64, 64, 128, 192}, {"Skeleton", 64, 48, 192, 64},
    {"DonkeyKong", 64, 48, 192, 112},
};
constexpr int ART_COUNT = int(sizeof(ARTS) / sizeof(ARTS[0]));
int fl(double v) { return int(std::floor(v)); }
float half(int size) { return size == 32 ? 0.5f : size == 64 ? 0.5f : 0.0f; } // func_411_c
} // namespace

void Paintings::position(P& p) { // EntityPainting.func_412_b
    const Art& a = ARTS[p.art];
    float w = float(a.w), h = float(a.h), d = float(a.w);
    if (p.dir != 0 && p.dir != 2) w = 0.5f;
    else d = 0.5f;
    w /= 32.0f;
    h /= 32.0f;
    d /= 32.0f;
    float x = float(p.tileX) + 0.5f, y = float(p.tileY) + 0.5f, z = float(p.tileZ) + 0.5f;
    const float o = 9.0f / 16.0f;
    if (p.dir == 0) z -= o;
    if (p.dir == 1) x -= o;
    if (p.dir == 2) z += o;
    if (p.dir == 3) x += o;
    if (p.dir == 0) x -= half(a.w);
    if (p.dir == 1) z += half(a.w);
    if (p.dir == 2) x += half(a.w);
    if (p.dir == 3) z -= half(a.w);
    y += half(a.h);
    p.x = x;
    p.y = y;
    p.z = z;
    const float e = -(0.1f / 16.0f);
    p.box = AABB{double(x - w - e), double(y - h - e), double(z - d - e), double(x + w + e), double(y + h + e), double(z + d + e)};
}

bool Paintings::valid(World& w, const P& p, int ignore) const { // func_410_i
    std::vector<AABB> boxes;
    Physics::collidingBoxes(w, p.box, boxes);
    if (!boxes.empty()) return false;
    const Art& a = ARTS[p.art];
    int nw = a.w / 16, nh = a.h / 16;
    int x0 = p.tileX, z0 = p.tileZ;
    if (p.dir == 0 || p.dir == 2) x0 = fl(p.x - double(float(a.w) / 32.0f));
    else z0 = fl(p.z - double(float(a.w) / 32.0f));
    int y0 = fl(p.y - double(float(a.h) / 32.0f));
    for (int i = 0; i < nw; ++i)
        for (int j = 0; j < nh; ++j) {
            Mat m = p.dir != 0 && p.dir != 2 ? BP[w.getBlockId(p.tileX, y0 + j, z0 + i)].mat : BP[w.getBlockId(x0 + i, y0 + j, p.tileZ)].mat;
            if (!matSolid(m)) return false;
        }
    for (size_t i = 0; i < list.size(); ++i)
        if (int(i) != ignore && list[i].box.intersects(p.box)) return false;
    return true;
}

void Paintings::chunkLoaded(Chunk& c) {
    if (!c.level) return;
    nbt::Tag* ents = c.level->getList("Entities");
    if (!ents) return;
    for (auto& t : ents->items) {
        if (t->getString("id") != "Painting") continue;
        P p{};
        p.dir = t->getByte("Dir");
        p.tileX = t->getInt("TileX");
        p.tileY = t->getInt("TileY");
        p.tileZ = t->getInt("TileZ");
        std::string m = t->getString("Motive");
        p.art = 0; // Java leaves it null for an unknown name; Kebab is safer than crashing
        for (int i = 0; i < ART_COUNT; ++i)
            if (m == ARTS[i].title) p.art = i;
        position(p);
        list.push_back(p);
    }
}

void Paintings::chunkUnloaded(int cx, int cz) {
    list.erase(std::remove_if(list.begin(), list.end(), [&](const P& p) { return (fl(p.x) >> 4) == cx && (fl(p.z) >> 4) == cz; }),
               list.end());
}

void Paintings::removeSaved(World& w, const P& p) {
    Chunk* c = w.getChunk(fl(p.x) >> 4, fl(p.z) >> 4);
    if (!c || !c->level) return;
    nbt::Tag* ents = c->level->getList("Entities");
    if (!ents) return;
    auto& v = ents->items;
    for (size_t i = 0; i < v.size(); ++i) {
        nbt::Tag& t = *v[i];
        if (t.getString("id") == "Painting" && t.getInt("TileX") == p.tileX && t.getInt("TileY") == p.tileY &&
            t.getInt("TileZ") == p.tileZ && t.getByte("Dir") == p.dir) {
            v.erase(v.begin() + long(i));
            c->modified = true;
            return;
        }
    }
}

void Paintings::addSaved(World& w, const P& p) {
    Chunk* c = w.getChunk(fl(p.x) >> 4, fl(p.z) >> 4);
    if (!c) return;
    if (!c->level) c->level = nbt::makeCompound();
    nbt::Tag* ents = c->level->getList("Entities");
    if (!ents) {
        c->level->set("Entities", nbt::makeList(nbt::Compound));
        ents = c->level->getList("Entities");
    }
    // Entity.writeToNBT + EntityPainting.writeEntityToNBT
    nbt::TagPtr t = nbt::makeCompound();
    t->setString("id", "Painting");
    nbt::TagPtr pos = nbt::makeList(nbt::Double);
    pos->items.push_back(nbt::makeDouble(p.x));
    pos->items.push_back(nbt::makeDouble(p.y));
    pos->items.push_back(nbt::makeDouble(p.z));
    t->set("Pos", std::move(pos));
    nbt::TagPtr mo = nbt::makeList(nbt::Double);
    for (int i = 0; i < 3; ++i) mo->items.push_back(nbt::makeDouble(0.0));
    t->set("Motion", std::move(mo));
    nbt::TagPtr ro = nbt::makeList(nbt::Float);
    ro->items.push_back(nbt::makeFloat(float(p.dir * 90)));
    ro->items.push_back(nbt::makeFloat(0.0f));
    t->set("Rotation", std::move(ro));
    t->setFloat("FallDistance", 0.0f);
    t->setShort("Fire", 0);
    t->setShort("Air", 300);
    t->setBool("OnGround", false);
    t->setByte("Dir", int8_t(p.dir));
    t->setString("Motive", ARTS[p.art].title);
    t->setInt("TileX", p.tileX);
    t->setInt("TileY", p.tileY);
    t->setInt("TileZ", p.tileZ);
    ents->listType = nbt::Compound;
    ents->items.push_back(std::move(t));
    c->modified = true;
}

bool Paintings::place(World& w, int x, int y, int z, int side, bool& hung) {
    hung = false;
    if (side == 0 || side == 1) return false;
    int dir = side == 4 ? 1 : side == 3 ? 2 : side == 5 ? 3 : 0;
    // EntityPainting(world, x, y, z, dir): a random one of the arts that fit
    std::vector<int> fits;
    P p{};
    p.tileX = x;
    p.tileY = y;
    p.tileZ = z;
    p.dir = dir;
    for (int i = 0; i < ART_COUNT; ++i) {
        p.art = i;
        position(p);
        if (valid(w, p, -1)) fits.push_back(i);
    }
    if (!fits.empty()) p.art = fits[rand.nextInt(int(fits.size()))];
    position(p);
    if (valid(w, p, -1)) {
        list.push_back(p);
        addSaved(w, p);
        hung = true;
    }
    return true;
}

int Paintings::pick(double ox, double oy, double oz, double dx, double dy, double dz, double maxDist, double& dist) const {
    int best = -1;
    dist = maxDist;
    for (size_t i = 0; i < list.size(); ++i) {
        const AABB& b = list[i].box;
        double t0 = 0.0, t1 = dist;
        const double o[3] = {ox, oy, oz}, d[3] = {dx, dy, dz};
        const double lo[3] = {b.minX, b.minY, b.minZ}, hi[3] = {b.maxX, b.maxY, b.maxZ};
        bool ok = true;
        for (int a = 0; a < 3 && ok; ++a) {
            if (std::fabs(d[a]) < 1e-9) { if (o[a] < lo[a] || o[a] > hi[a]) ok = false; continue; }
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) ok = false;
        }
        if (ok && t0 <= dist) { dist = t0; best = int(i); }
    }
    return best;
}

void Paintings::knockDown(World& w, int index, ItemEntities& items) {
    if (index < 0 || index >= int(list.size())) return;
    P p = list[size_t(index)];
    list.erase(list.begin() + index);
    removeSaved(w, p);
    ItemStack st;
    st.id = 321;
    st.count = 1;
    items.spawn(p.x, p.y, p.z, st, 10);
}

void Paintings::tick(World& w, ItemEntities& items) {
    if (++timer < 100) return;
    timer = 0;
    for (int i = int(list.size()) - 1; i >= 0; --i) {
        const P& p = list[size_t(i)];
        if (!w.isChunkLoaded(fl(p.x) >> 4, fl(p.z) >> 4)) continue;
        if (!valid(w, p, i)) knockDown(w, i, items);
    }
}

void Paintings::render(const World& w, Renderer& r) { // RenderPainting
    if (list.empty()) return;
    C3D_Tex* tex = r.texture("romfs:/textures/art/kz.rgba");
    if (!tex) return;
    std::vector<QuadVertex> tris;
    for (const P& p : list) {
        const Art& a = ARTS[p.art];
        float yaw = float(p.dir * 90) * 3.1415927f / 180.0f, c = std::cos(yaw), s = std::sin(yaw);
        float shade = w.getBrightness(fl(p.x), fl(p.y), fl(p.z));
        auto at = [&](float lx, float ly, float lz, float u, float v) { // glRotatef(yaw, 0, 1, 0), 1/16 scale
            lx /= 16.0f;
            ly /= 16.0f;
            lz /= 16.0f;
            return QuadVertex{float(p.x) + lx * c + lz * s, float(p.y) + ly, float(p.z) - lx * s + lz * c, shade, u, v};
        };
        float x0 = -a.w / 2.0f, x1 = a.w / 2.0f, y0 = -a.h / 2.0f, y1 = a.h / 2.0f;
        float u0 = float(a.u + a.w) / 256.0f, u1 = float(a.u) / 256.0f; // x0 -> the art's right edge
        float v0 = float(a.v + a.h) / 256.0f, v1 = float(a.v) / 256.0f; // y0 -> its bottom
        // the picture (facing -Z in its own frame) and the wooden back
        QuadVertex f[4] = {at(x1, y0, -0.5f, u1, v0), at(x0, y0, -0.5f, u0, v0), at(x0, y1, -0.5f, u0, v1), at(x1, y1, -0.5f, u1, v1)};
        tris.insert(tris.end(), {f[0], f[1], f[2], f[0], f[2], f[3]});
        const float bu0 = 12.0f / 16.0f, bu1 = 13.0f / 16.0f, bv0 = 0.0f, bv1 = 1.0f / 16.0f;
        QuadVertex b[4] = {at(x1, y1, 0.5f, bu0, bv0), at(x0, y1, 0.5f, bu1, bv0), at(x0, y0, 0.5f, bu1, bv1), at(x1, y0, 0.5f, bu0, bv1)};
        for (auto& v : b) v.shade *= 0.8f;
        tris.insert(tris.end(), {b[0], b[1], b[2], b[0], b[2], b[3]});
    }
    r.drawTriangles(tris, tex);
}
