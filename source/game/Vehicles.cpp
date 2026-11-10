#include "Vehicles.h"
#include <algorithm>
#include <cmath>
#include <string>
#include "Containers.h"
#include "ItemEntities.h"
#include "Mobs.h"
#include "Player.h"
#include "audio/Sound.h"
#include "model/BlockModels.h"
#include "render/BoxModel.h"
#include "render/Particles.h"
#include "render/Renderer.h"
#include "world/Chunk.h"
#include "world/JavaRandom.h"
#include "world/MathHelper.h"
#include "world/BlockTable.h"
#include "world/World.h"

namespace {
constexpr double PI_D = 3.141592653589793;
constexpr float W = 1.5f, H = 0.6f, YOFF = 0.3f; // setSize(1.5, 0.6), yOffset = height / 2
JavaRandom rnd{0xB0A7ULL}; // Entity.rand
int fl(double v) { int i = int(v); return v < double(i) ? i - 1 : i; }
Vehicles* active = nullptr;
int movingId = 0;     // the boat being moved doesn't collide with itself
int excludedId = 0;   // the ridden boat doesn't hold up its rider

// World.getCollidingBoundingBoxes: boats are solid to everything else
void boatBoxes(const AABB& area, std::vector<AABB>& out) {
    if (!active) return;
    for (const Vehicles::Boat& b : active->boats)
        if (!b.dead && b.id != movingId && b.id != excludedId && b.box.intersects(area)) out.push_back(b.box);
}
} // namespace

Vehicles::Vehicles() {
    active = this;
    Physics::entityBoxes = boatBoxes;
}

Vehicles::~Vehicles() {
    if (active == this) {
        active = nullptr;
        Physics::entityBoxes = nullptr;
    }
}

void Vehicles::clear() {
    boats.clear();
    carts.clear();
    riddenId = riddenCart = 0;
    excludedId = 0;
}

Vehicles::Boat* Vehicles::byId(int id) {
    for (Boat& b : boats)
        if (b.id == id && !b.dead) return &b;
    return nullptr;
}

const Vehicles::Boat* Vehicles::byId(int id) const {
    for (const Boat& b : boats)
        if (b.id == id && !b.dead) return &b;
    return nullptr;
}

void Vehicles::setPosition(Boat& b, double x, double y, double z) { // Entity.setPosition
    b.x = x;
    b.y = y;
    b.z = z;
    double hw = double(W / 2.0f);
    b.box = AABB{x - hw, y - double(YOFF), z - hw, x + hw, y - double(YOFF) + double(H), z + hw};
}

// ---------------------------------------------------------------- saving (the chunk's "Entities")

void Vehicles::chunkLoaded(Chunk& c) {
    if (!c.level) return;
    nbt::Tag* list = c.level->getList("Entities");
    if (!list) return;
    auto& v = list->items;
    for (size_t i = 0; i < v.size();) {
        nbt::Tag* e = v[i].get();
        if (e && e->getString("id") == "Minecart" && e->getList("Pos") && e->getList("Pos")->items.size() >= 3) {
            nbt::Tag* cp = e->getList("Pos");
            Cart cart;
            cart.tag = std::shared_ptr<nbt::Tag>(v[i].release());
            setPosition(cart, cp->items[0]->d, cp->items[1]->d, cp->items[2]->d);
            cart.px = cart.x; cart.py = cart.y; cart.pz = cart.z;
            if (nbt::Tag* mo = e->getList("Motion"); mo && mo->items.size() >= 3) {
                cart.mx = mo->items[0]->d;
                cart.my = mo->items[1]->d;
                cart.mz = mo->items[2]->d;
            }
            if (nbt::Tag* rot = e->getList("Rotation"); rot && rot->items.size() >= 2) {
                cart.yaw = cart.prevYaw = float(rot->items[0]->d);
                cart.pitch = float(rot->items[1]->d);
            }
            cart.onGround = e->getBool("OnGround");
            cart.type = e->getInt("Type"); // EntityMinecart.readEntityFromNBT
            if (cart.type == 2) {
                cart.pushX = e->getDouble("PushX");
                cart.pushZ = e->getDouble("PushZ");
                cart.fuel = e->getShort("Fuel");
            } else if (cart.type == 1) {
                cart.items.assign(27, ItemStack{});
                if (nbt::Tag* list = e->getList("Items"))
                    for (auto& it : list->items) {
                        int slot = it->getByte("Slot") & 255;
                        if (slot < 27) cart.items[slot] = containers::fromNBT(*it);
                    }
            }
            cart.id = nextId++;
            carts.push_back(std::move(cart));
            v.erase(v.begin() + i);
            c.modified = true;
            continue;
        }
        nbt::Tag* pos = e && e->getString("id") == "Boat" ? e->getList("Pos") : nullptr;
        if (!pos || pos->items.size() < 3) { ++i; continue; }
        Boat b;
        b.tag = std::shared_ptr<nbt::Tag>(v[i].release());
        setPosition(b, pos->items[0]->d, pos->items[1]->d, pos->items[2]->d);
        b.px = b.x; b.py = b.y; b.pz = b.z;
        if (nbt::Tag* mo = e->getList("Motion"); mo && mo->items.size() >= 3) {
            b.mx = mo->items[0]->d;
            b.my = mo->items[1]->d;
            b.mz = mo->items[2]->d;
        }
        if (nbt::Tag* rot = e->getList("Rotation"); rot && !rot->items.empty()) b.yaw = b.prevYaw = float(rot->items[0]->d);
        b.onGround = e->getBool("OnGround");
        b.id = nextId++;
        boats.push_back(std::move(b));
        v.erase(v.begin() + i);
        c.modified = true;
    }
}

void Vehicles::writeToChunk(Chunk& c) const {
    if (!c.level) c.level = nbt::makeCompound();
    nbt::Tag* list = c.level->getList("Entities");
    if (!list) {
        c.level->set("Entities", nbt::makeList(nbt::Compound));
        list = c.level->getList("Entities");
    }
    auto& v = list->items;
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(), [](const nbt::TagPtr& t) { return t->getString("id") == "Boat"; }), v.end());
    bool any = v.size() != before;
    for (const Boat& b : boats) {
        if (b.dead || (fl(b.x) >> 4) != c.xPosition || (fl(b.z) >> 4) != c.zPosition) continue;
        // Entity.writeToNBT + EntityBoat.writeEntityToNBT
        nbt::TagPtr t = b.tag ? nbt::clone(*b.tag) : nbt::makeCompound();
        t->setString("id", "Boat");
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(b.x));
        pos->items.push_back(nbt::makeDouble(b.y));
        pos->items.push_back(nbt::makeDouble(b.z));
        t->set("Pos", std::move(pos));
        nbt::TagPtr mo = nbt::makeList(nbt::Double);
        mo->items.push_back(nbt::makeDouble(b.mx));
        mo->items.push_back(nbt::makeDouble(b.my));
        mo->items.push_back(nbt::makeDouble(b.mz));
        t->set("Motion", std::move(mo));
        nbt::TagPtr ro = nbt::makeList(nbt::Float);
        ro->items.push_back(nbt::makeFloat(b.yaw));
        ro->items.push_back(nbt::makeFloat(0.0f));
        t->set("Rotation", std::move(ro));
        t->setFloat("FallDistance", 0.0f);
        t->setShort("Fire", -1);
        t->setShort("Air", 300);
        t->setBool("OnGround", b.onGround);
        v.push_back(std::move(t));
        any = true;
    }
    v.erase(std::remove_if(v.begin(), v.end(), [](const nbt::TagPtr& t) { return t->getString("id") == "Minecart"; }), v.end());
    for (const Cart& k : carts) {
        if (k.dead || (fl(k.x) >> 4) != c.xPosition || (fl(k.z) >> 4) != c.zPosition) continue;
        nbt::TagPtr t = k.tag ? nbt::clone(*k.tag) : nbt::makeCompound();
        t->setString("id", "Minecart");
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(k.x));
        pos->items.push_back(nbt::makeDouble(k.y));
        pos->items.push_back(nbt::makeDouble(k.z));
        t->set("Pos", std::move(pos));
        nbt::TagPtr mo = nbt::makeList(nbt::Double);
        mo->items.push_back(nbt::makeDouble(k.mx));
        mo->items.push_back(nbt::makeDouble(k.my));
        mo->items.push_back(nbt::makeDouble(k.mz));
        t->set("Motion", std::move(mo));
        nbt::TagPtr ro = nbt::makeList(nbt::Float);
        ro->items.push_back(nbt::makeFloat(k.yaw));
        ro->items.push_back(nbt::makeFloat(k.pitch));
        t->set("Rotation", std::move(ro));
        t->setFloat("FallDistance", 0.0f);
        t->setShort("Fire", -1);
        t->setShort("Air", 300);
        t->setBool("OnGround", k.onGround);
        t->setInt("Type", k.type);
        if (k.type == 2) {
            t->setDouble("PushX", k.pushX);
            t->setDouble("PushZ", k.pushZ);
            t->setShort("Fuel", int16_t(k.fuel));
        } else if (k.type == 1) {
            nbt::TagPtr list = nbt::makeList(nbt::Compound);
            for (size_t i = 0; i < k.items.size(); ++i) {
                if (k.items[i].empty()) continue;
                nbt::TagPtr it = nbt::makeCompound();
                it->setByte("Slot", int8_t(i));
                containers::toNBT(k.items[i], *it);
                list->items.push_back(std::move(it));
            }
            t->set("Items", std::move(list));
        }
        v.push_back(std::move(t));
        any = true;
    }
    if (any) c.modified = true;
}

void Vehicles::chunkUnloaded(int cx, int cz) {
    for (Cart& k : carts)
        if ((fl(k.x) >> 4) == cx && (fl(k.z) >> 4) == cz && k.id != riddenCart) k.dead = true;
    carts.erase(std::remove_if(carts.begin(), carts.end(), [](const Cart& k) { return k.dead; }), carts.end());
    for (Boat& b : boats)
        if ((fl(b.x) >> 4) == cx && (fl(b.z) >> 4) == cz && b.id != riddenId) b.dead = true;
    boats.erase(std::remove_if(boats.begin(), boats.end(), [](const Boat& b) { return b.dead; }), boats.end());
}

// ---------------------------------------------------------------- the player and boats

bool Vehicles::placeBoat(World& w, const Player& p) {
    // ItemBoat.onItemRightClick: along the look for 5 blocks, stopping at still water too
    float yaw = p.rotationYaw, pitch = p.rotationPitch;
    float c = MathHelper::cos(-yaw * (3.1415927f / 180.0f) - 3.1415927f), s = MathHelper::sin(-yaw * (3.1415927f / 180.0f) - 3.1415927f);
    float cp = -MathHelper::cos(-pitch * (3.1415927f / 180.0f)), sp = MathHelper::sin(-pitch * (3.1415927f / 180.0f));
    float dx = s * cp, dy = sp, dz = c * cp;
    RayHit h = w.rayTrace(float(p.posX), float(p.posY), float(p.posZ), dx, dy, dz, 5.0f, true);
    if (!h.hit) return false;
    Boat b;
    setPosition(b, double(float(h.x) + 0.5f), double(float(h.y) + 1.5f) + double(YOFF), double(float(h.z) + 0.5f));
    b.px = b.x; b.py = b.y; b.pz = b.z;
    b.id = nextId++;
    boats.push_back(std::move(b));
    return true;
}

int Vehicles::pick(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const {
    int best = -1;
    dist = reach;
    for (size_t i = 0; i < boats.size(); ++i) {
        const Boat& b = boats[i];
        if (b.dead || b.id == riddenId) continue;
        double t;
        if (b.box.expand(0.1, 0.1, 0.1).rayHit(ox, oy, oz, dx, dy, dz, dist, t)) { dist = t; best = int(i); } // border 0.1
    }
    return best;
}

void Vehicles::hit(int index, int damage, ItemEntities& items) {
    if (index < 0 || index >= int(boats.size())) return;
    Boat& b = boats[index];
    if (b.dead) return;
    b.hurtDir = -b.hurtDir;
    b.hurtTime = 10;
    b.damage += damage * 10;
    if (b.damage > 40) { // EntityBoat.attackEntityFrom: the boat comes back as an item
        ItemStack st;
        st.id = 333;
        st.count = 1;
        items.spawn(b.x, b.y, b.z, st, 10);
        if (b.id == riddenId) riddenId = excludedId = 0;
        b.dead = true;
    }
}

void Vehicles::use(int index, Player& p) {
    if (index < 0 || index >= int(boats.size())) return;
    Boat& b = boats[index];
    if (b.id == riddenId) { dismount(p); return; }
    if (riddenCart) dismount(p);
    riddenId = excludedId = b.id; // mountEntity
    riderMx = riderMz = 0.0;
}

void Vehicles::dismount(Player& p) {
    if (riddenCart) {
        Cart* k = cartById(riddenCart);
        riddenCart = 0;
        if (!k) return;
        p.place(k->x, k->box.minY + double(0.7f) + double(Player::YOFFSET), k->z, p.rotationYaw, p.rotationPitch);
        p.motionX = p.motionY = p.motionZ = 0.0;
        return;
    }
    Boat* b = byId(riddenId);
    riddenId = excludedId = 0;
    if (!b) return;
    // setLocationAndAngles(x, minY + height, z): standing on top of it
    p.place(b->x, b->box.minY + double(H) + double(Player::YOFFSET), b->z, p.rotationYaw, p.rotationPitch);
    p.motionX = p.motionY = p.motionZ = 0.0;
}

void Vehicles::setRiderInput(float strafe, float forward, float yaw, bool riderInWater) {
    // EntityLiving.onLivingUpdate + moveEntityWithHeading on a rider whose motion was zeroed by
    // updateRidden: moveFlying(.., 0.02) then the air (or water) drag; the boat takes a fifth of it
    strafe *= 0.98f;
    forward *= 0.98f;
    double mx = 0.0, mz = 0.0;
    float l = MathHelper::sqrt_float(strafe * strafe + forward * forward);
    if (l >= 0.01f) {
        if (l < 1.0f) l = 1.0f;
        l = 0.02f / l;
        float a = strafe * l, f = forward * l;
        float sn = MathHelper::sin(yaw * 3.1415927f / 180.0f), cs = MathHelper::cos(yaw * 3.1415927f / 180.0f);
        mx = double(a * cs - f * sn);
        mz = double(f * cs + a * sn);
    }
    double k = riderInWater ? double(0.8f) : double(0.91f);
    riderMx = mx * k;
    riderMz = mz * k;
}

bool Vehicles::riderSeat(double& x, double& y, double& z) const {
    if (riddenCart) { // EntityMinecart: the default updateRiderPosition, right in the middle
        for (const Cart& k : carts)
            if (k.id == riddenCart && !k.dead) {
                x = k.x;
                y = k.y + double(0.7f) * 0.0 - double(0.3f) + double(Player::YOFFSET - 0.5f);
                z = k.z;
                return true;
            }
        return false;
    }
    const Boat* b = byId(riddenId);
    if (!b) return false;
    // updateRiderPosition: a little behind the middle; EntityPlayer.getYOffset is yOffset - 0.5
    x = b->x + std::cos(double(b->yaw) * PI_D / 180.0) * 0.4;
    y = b->y + double(H) * 0.0 - double(0.3f) + double(Player::YOFFSET - 0.5f);
    z = b->z + std::sin(double(b->yaw) * PI_D / 180.0) * 0.4;
    return true;
}

// ---------------------------------------------------------------- EntityBoat.onUpdate

void Vehicles::move(const World& w, Boat& b, double dx, double dy, double dz) { // Entity.moveEntity, no step up
    double odx = dx, ody = dy, odz = dz;
    movingId = b.id;
    Physics::collidingBoxes(w, b.box.addCoord(dx, dy, dz), scratch);
    movingId = 0;
    for (const AABB& a : scratch) dy = a.calculateYOffset(b.box, dy);
    b.box.move(0.0, dy, 0.0);
    for (const AABB& a : scratch) dx = a.calculateXOffset(b.box, dx);
    b.box.move(dx, 0.0, 0.0);
    for (const AABB& a : scratch) dz = a.calculateZOffset(b.box, dz);
    b.box.move(0.0, 0.0, dz);
    b.x = (b.box.minX + b.box.maxX) / 2.0;
    b.y = b.box.minY + double(YOFF);
    b.z = (b.box.minZ + b.box.maxZ) / 2.0;
    b.collidedH = odx != dx || odz != dz;
    b.onGround = ody != dy && ody < 0.0;
    if (odx != dx) b.mx = 0.0;
    if (ody != dy) b.my = 0.0;
    if (odz != dz) b.mz = 0.0;
}

void Vehicles::tick(World& w, Player& p, ItemEntities& items, Mobs* mobsIn) {
    (void)p;
    for (Boat& b : boats) {
        if (b.dead) continue;
        if (!w.isChunkLoaded(fl(b.x) >> 4, fl(b.z) >> 4)) continue;
        if (b.hurtTime > 0) --b.hurtTime;
        if (b.damage > 0) --b.damage;
        b.px = b.x;
        b.py = b.y;
        b.pz = b.z;
        b.prevYaw = b.yaw;
        // how much of it is in water, in five slices
        const int N = 5;
        double wet = 0.0;
        for (int i = 0; i < N; ++i) {
            double y0 = b.box.minY + (b.box.maxY - b.box.minY) * double(i) / double(N) - 0.125;
            double y1 = b.box.minY + (b.box.maxY - b.box.minY) * double(i + 1) / double(N) - 0.125;
            AABB s{b.box.minX, y0, b.box.minZ, b.box.maxX, y1, b.box.maxZ};
            if (Physics::isMaterialInBB(w, s, false)) wet += 1.0 / double(N);
        }
        b.my += double(0.04f) * (wet * 2.0 - 1.0);
        bool ridden = b.id == riddenId;
        if (ridden) {
            b.mx += riderMx * 0.2;
            b.mz += riderMz * 0.2;
        }
        const double cap = 0.4;
        b.mx = std::max(-cap, std::min(cap, b.mx));
        b.mz = std::max(-cap, std::min(cap, b.mz));
        if (b.onGround) {
            b.mx *= 0.5;
            b.my *= 0.5;
            b.mz *= 0.5;
        }
        move(w, b, b.mx, b.my, b.mz);
        double speed = std::sqrt(b.mx * b.mx + b.mz * b.mz);
        if (speed > 0.15 && gParticles) { // the bow wave
            double c = std::cos(double(b.yaw) * PI_D / 180.0), s = std::sin(double(b.yaw) * PI_D / 180.0);
            for (int i = 0; double(i) < 1.0 + speed * 60.0; ++i) {
                double f = double(rnd.nextFloat() * 2.0f - 1.0f);
                double side = double(rnd.nextInt(2) * 2 - 1) * 0.7;
                double sx, sz;
                if (rnd.nextBoolean()) {
                    sx = b.x - c * f * 0.8 + s * side;
                    sz = b.z - s * f * 0.8 - c * side;
                } else {
                    sx = b.x + c + s * f * 0.7;
                    sz = b.z + s - c * f * 0.7;
                }
                gParticles->spawn(Particles::SPLASH, sx, b.y - 0.125, sz, b.mx, b.my, b.mz);
            }
        }
        if (!b.collidedH || !(speed > 0.15)) { // (a boat never breaks up: running aground just doesn't slow it)
            b.mx *= double(0.99f);
            b.my *= double(0.95f);
            b.mz *= double(0.99f);
        }
        // it turns toward where it's going, 20 degrees a tick at most
        double want = double(b.yaw);
        double ddx = b.px - b.x, ddz = b.pz - b.z;
        if (ddx * ddx + ddz * ddz > 0.001) want = double(float(std::atan2(ddz, ddx) * 180.0 / PI_D));
        double turn = want - double(b.yaw);
        while (turn >= 180.0) turn -= 360.0;
        while (turn < -180.0) turn += 360.0;
        turn = std::max(-20.0, std::min(20.0, turn));
        b.yaw = float(double(b.yaw) + turn);
    }
    for (Cart& k : carts) k.mobAboard = false;
    if (mobsIn) // who's riding what
        mobsIn->forEachBody([&](const AABB&, double, double, double&, double&, int& vehicle) {
            if (vehicle < 0)
                if (Cart* k = cartById(-vehicle)) k->mobAboard = true;
        });
    for (Cart& k : carts)
        if (!k.dead && w.isChunkLoaded(fl(k.x) >> 4, fl(k.z) >> 4)) tickCart(w, k);
    cartCollisions(p, mobsIn);
    carts.erase(std::remove_if(carts.begin(), carts.end(), [](const Cart& k) { return k.dead; }), carts.end());
    // boats bump each other apart (applyEntityCollision)
    for (size_t i = 0; i < boats.size(); ++i) {
        Boat& a = boats[i];
        if (a.dead) continue;
        AABB ab = a.box.expand(double(0.2f), 0.0, double(0.2f));
        for (size_t j = 0; j < boats.size(); ++j) {
            if (i == j) continue;
            Boat& o = boats[j];
            if (o.dead || o.id == riddenId || !ab.intersects(o.box)) continue;
            double dx = a.x - o.x, dz = a.z - o.z;
            double d = std::max(std::fabs(dx), std::fabs(dz));
            if (d < 0.01) continue;
            d = std::sqrt(d);
            dx /= d;
            dz /= d;
            double f = std::min(1.0, 1.0 / d);
            dx *= f * 0.05;
            dz *= f * 0.05;
            o.mx -= dx;
            o.mz -= dz;
            a.mx += dx;
            a.mz += dz;
        }
    }
    boats.erase(std::remove_if(boats.begin(), boats.end(), [](const Boat& b) { return b.dead; }), boats.end());
}

// ================================================================ EntityMinecart

namespace {
constexpr float CW = 0.98f, CH = 0.7f, CYOFF = 0.35f; // setSize(0.98, 0.7), yOffset = height / 2
constexpr int RAIL = 66;
// field_855_j: each rail shape's two ends (dx, dy, dz)
const int ENDS[10][2][3] = {{{0, 0, -1}, {0, 0, 1}},   {{-1, 0, 0}, {1, 0, 0}},  {{-1, -1, 0}, {1, 0, 0}}, {{-1, 0, 0}, {1, -1, 0}},
                            {{0, 0, -1}, {0, -1, 1}},  {{0, -1, -1}, {0, 0, 1}}, {{0, 0, 1}, {1, 0, 0}},   {{0, 0, 1}, {-1, 0, 0}},
                            {{0, 0, -1}, {-1, 0, 0}},  {{0, 0, -1}, {1, 0, 0}}};
struct V3 { double x, y, z; };

// func_514_g: the point on the rail under (x, y, z), or false off the rails
bool railPoint(const World& w, double x, double y, double z, V3& out) {
    int bx = fl(x), by = fl(y), bz = fl(z);
    if (w.getBlockId(bx, by - 1, bz) == RAIL) --by;
    if (w.getBlockId(bx, by, bz) != RAIL) return false;
    int m = w.getBlockMetadata(bx, by, bz);
    if (m > 9) m = 0;
    const int (*e)[3] = ENDS[m];
    double x0 = bx + 0.5 + e[0][0] * 0.5, y0 = by + 0.5 + e[0][1] * 0.5, z0 = bz + 0.5 + e[0][2] * 0.5;
    double x1 = bx + 0.5 + e[1][0] * 0.5, y1 = by + 0.5 + e[1][1] * 0.5, z1 = bz + 0.5 + e[1][2] * 0.5;
    double dx = x1 - x0, dy = (y1 - y0) * 2.0, dz = z1 - z0;
    double t;
    if (dx == 0.0) {
        x = bx + 0.5;
        t = z - bz;
    } else if (dz == 0.0) {
        z = bz + 0.5;
        t = x - bx;
    } else {
        t = ((x - x0) * dx + (z - z0) * dz) * 2.0;
    }
    x = x0 + dx * t;
    y = y0 + dy * t;
    z = z0 + dz * t;
    if (dy < 0.0) y += 1.0;
    if (dy > 0.0) y += 0.5;
    out = {x, y, z};
    return true;
}

// func_515_a: the rail point a distance along the rail from (x, y, z)
bool railPointAhead(const World& w, double x, double y, double z, double d, V3& out) {
    int bx = fl(x), by = fl(y), bz = fl(z);
    if (w.getBlockId(bx, by - 1, bz) == RAIL) --by;
    if (w.getBlockId(bx, by, bz) != RAIL) return false;
    int m = w.getBlockMetadata(bx, by, bz);
    if (m > 9) m = 0;
    y = double(by);
    if (m >= 2 && m <= 5) y = double(by + 1);
    const int (*e)[3] = ENDS[m];
    double dx = e[1][0] - e[0][0], dz = e[1][2] - e[0][2];
    double l = std::sqrt(dx * dx + dz * dz);
    dx /= l;
    dz /= l;
    x += dx * d;
    z += dz * d;
    if (e[0][1] != 0 && fl(x) - bx == e[0][0] && fl(z) - bz == e[0][2]) y += e[0][1];
    else if (e[1][1] != 0 && fl(x) - bx == e[1][0] && fl(z) - bz == e[1][2]) y += e[1][1];
    return railPoint(w, x, y, z, out);
}
} // namespace

void Vehicles::setPosition(Cart& c, double x, double y, double z) {
    c.x = x;
    c.y = y;
    c.z = z;
    double hw = double(CW / 2.0f);
    c.box = AABB{x - hw, y - double(CYOFF), z - hw, x + hw, y - double(CYOFF) + double(CH), z + hw};
}

Vehicles::Cart* Vehicles::cartById(int id) {
    for (Cart& k : carts)
        if (k.id == id && !k.dead) return &k;
    return nullptr;
}

bool Vehicles::placeCart(World& w, int x, int y, int z, int type) {
    if (w.getBlockId(x, y, z) != RAIL) return false;
    Cart k;
    setPosition(k, double(float(x) + 0.5f), double(float(y) + 0.5f) + double(CYOFF), double(float(z) + 0.5f));
    k.px = k.x; k.py = k.y; k.pz = k.z;
    k.type = type;
    if (type == 1) k.items.assign(27, ItemStack{});
    k.id = nextId++;
    carts.push_back(std::move(k));
    return true;
}

int Vehicles::pickCart(double ox, double oy, double oz, double dx, double dy, double dz, double reach, double& dist) const {
    int best = -1;
    dist = reach;
    for (size_t i = 0; i < carts.size(); ++i) {
        const Cart& k = carts[i];
        if (k.dead || k.id == riddenCart) continue;
        double t;
        if (k.box.expand(0.1, 0.1, 0.1).rayHit(ox, oy, oz, dx, dy, dz, dist, t)) { dist = t; best = int(i); }
    }
    return best;
}

void Vehicles::hitCart(int index, int damage, ItemEntities& items) {
    if (index < 0 || index >= int(carts.size())) return;
    Cart& k = carts[index];
    if (k.dead) return;
    k.hurtDir = -k.hurtDir;
    k.hurtTime = 10;
    k.damage += damage * 10;
    if (k.damage <= 40) return;
    auto drop = [&](int id) {
        ItemStack st;
        st.id = uint16_t(id);
        st.count = 1;
        items.spawn(k.x, k.y, k.z, st, 10);
    };
    drop(328); // Item.minecartEmpty, and what sat in it
    if (k.type == 1) drop(54);
    else if (k.type == 2) drop(61);
    for (ItemStack& st : k.items) { // setEntityDead: the chest's contents spill out
        if (st.empty()) continue;
        float ox = rnd.nextFloat() * 0.8f + 0.1f, oy = rnd.nextFloat() * 0.8f + 0.1f, oz = rnd.nextFloat() * 0.8f + 0.1f;
        while (st.count > 0) {
            int n = rnd.nextInt(21) + 10;
            if (n > st.count) n = st.count;
            st.count = decltype(st.count)(st.count - n);
            ItemStack part = st;
            part.count = decltype(part.count)(n);
            const float v = 0.05f;
            items.spawnMoving(k.x + double(ox), k.y + double(oy), k.z + double(oz), part, double(float(rnd.nextGaussian()) * v),
                              double(float(rnd.nextGaussian()) * v + 0.2f), double(float(rnd.nextGaussian()) * v), 10);
        }
    }
    if (k.id == riddenCart) riddenCart = 0;
    k.dead = true;
}

int Vehicles::useCart(int index, Player& p, Inventory& inv, bool creative) {
    if (index < 0 || index >= int(carts.size())) return 0;
    Cart& k = carts[index];
    if (k.type == 0) {
        if (k.id == riddenCart) { dismount(p); return 0; }
        if (riding()) dismount(p);
        riddenCart = k.id; // mountEntity
        return 0;
    }
    if (k.type == 1) return k.id; // displayGUIChest
    // the furnace cart: coal keeps it going, and it sets off away from you
    ItemStack& h = inv.held();
    if (!h.empty() && h.id == 263) {
        if (!creative) inv.consumeHeld();
        k.fuel += 1200;
    }
    k.pushX = k.x - p.posX;
    k.pushZ = k.z - p.posZ;
    return 0;
}

void Vehicles::saveCartItems(int id, const std::vector<ItemStack>& items) {
    if (Cart* k = cartById(id)) {
        k->items = items;
        k->items.resize(27);
    }
}

void Vehicles::moveCart(const World& w, Cart& c, double dx, double dy, double dz) { // Entity.moveEntity, no step
    double odx = dx, ody = dy, odz = dz;
    Physics::collidingBoxes(w, c.box.addCoord(dx, dy, dz), scratch);
    for (const AABB& a : scratch) dy = a.calculateYOffset(c.box, dy);
    c.box.move(0.0, dy, 0.0);
    for (const AABB& a : scratch) dx = a.calculateXOffset(c.box, dx);
    c.box.move(dx, 0.0, 0.0);
    for (const AABB& a : scratch) dz = a.calculateZOffset(c.box, dz);
    c.box.move(0.0, 0.0, dz);
    c.x = (c.box.minX + c.box.maxX) / 2.0;
    c.y = c.box.minY + double(CYOFF);
    c.z = (c.box.minZ + c.box.maxZ) / 2.0;
    c.collidedH = odx != dx || odz != dz;
    c.onGround = ody != dy && ody < 0.0;
    if (odx != dx) c.mx = 0.0;
    if (ody != dy) c.my = 0.0;
    if (odz != dz) c.mz = 0.0;
}

void Vehicles::tickCart(World& w, Cart& c) { // EntityMinecart.onUpdate
    if (c.hurtTime > 0) --c.hurtTime;
    if (c.damage > 0) --c.damage;
    c.px = c.x;
    c.py = c.y;
    c.pz = c.z;
    c.prevYaw = c.yaw;
    c.my -= double(0.04f);
    int bx = fl(c.x), by = fl(c.y), bz = fl(c.z);
    if (w.getBlockId(bx, by - 1, bz) == RAIL) --by;
    const double maxV = 0.4;
    bool pushing = false;
    const double slope = 1.0 / 128.0;
    bool ridden = c.id == riddenCart || c.mobAboard;
    if (w.getBlockId(bx, by, bz) == RAIL) {
        V3 before;
        bool hadBefore = railPoint(w, c.x, c.y, c.z, before);
        int m = w.getBlockMetadata(bx, by, bz);
        if (m > 9) m = 0;
        c.y = double(by);
        if (m >= 2 && m <= 5) c.y = double(by + 1);
        if (m == 2) c.mx -= slope;
        if (m == 3) c.mx += slope;
        if (m == 4) c.mz += slope;
        if (m == 5) c.mz -= slope;
        const int (*e)[3] = ENDS[m];
        double ex = e[1][0] - e[0][0], ez = e[1][2] - e[0][2];
        double el = std::sqrt(ex * ex + ez * ez);
        if (c.mx * ex + c.mz * ez < 0.0) {
            ex = -ex;
            ez = -ez;
        }
        double speed = std::sqrt(c.mx * c.mx + c.mz * c.mz);
        c.mx = speed * ex / el;
        c.mz = speed * ez / el;
        double t = 0.0;
        double x0 = bx + 0.5 + e[0][0] * 0.5, z0 = bz + 0.5 + e[0][2] * 0.5;
        double x1 = bx + 0.5 + e[1][0] * 0.5, z1 = bz + 0.5 + e[1][2] * 0.5;
        ex = x1 - x0;
        ez = z1 - z0;
        if (ex == 0.0) {
            c.x = bx + 0.5;
            t = c.z - bz;
        } else if (ez == 0.0) {
            c.z = bz + 0.5;
            t = c.x - bx;
        } else {
            t = ((c.x - x0) * ex + (c.z - z0) * ez) * 2.0;
        }
        c.x = x0 + ex * t;
        c.z = z0 + ez * t;
        setPosition(c, c.x, c.y + double(CYOFF), c.z);
        double vx = c.mx, vz = c.mz;
        if (ridden) {
            vx *= 0.75;
            vz *= 0.75;
        }
        vx = std::max(-maxV, std::min(maxV, vx));
        vz = std::max(-maxV, std::min(maxV, vz));
        moveCart(w, c, vx, 0.0, vz);
        if (e[0][1] != 0 && fl(c.x) - bx == e[0][0] && fl(c.z) - bz == e[0][2]) setPosition(c, c.x, c.y + e[0][1], c.z);
        else if (e[1][1] != 0 && fl(c.x) - bx == e[1][0] && fl(c.z) - bz == e[1][2]) setPosition(c, c.x, c.y + e[1][1], c.z);
        if (ridden) {
            c.mx *= double(0.997f);
            c.my *= 0.0;
            c.mz *= double(0.997f);
        } else {
            if (c.type == 2) {
                double pl = double(MathHelper::sqrt_double(c.pushX * c.pushX + c.pushZ * c.pushZ));
                if (pl > 0.01) {
                    pushing = true;
                    c.pushX /= pl;
                    c.pushZ /= pl;
                    c.mx *= double(0.8f);
                    c.my *= 0.0;
                    c.mz *= double(0.8f);
                    c.mx += c.pushX * 0.04;
                    c.mz += c.pushZ * 0.04;
                } else {
                    c.mx *= double(0.9f);
                    c.my *= 0.0;
                    c.mz *= double(0.9f);
                }
            }
            c.mx *= double(0.96f);
            c.my *= 0.0;
            c.mz *= double(0.96f);
        }
        V3 after;
        if (railPoint(w, c.x, c.y, c.z, after) && hadBefore) { // downhill speeds it up, uphill slows it
            double gain = (before.y - after.y) * 0.05;
            speed = std::sqrt(c.mx * c.mx + c.mz * c.mz);
            if (speed > 0.0) {
                c.mx = c.mx / speed * (speed + gain);
                c.mz = c.mz / speed * (speed + gain);
            }
            setPosition(c, c.x, after.y, c.z);
        }
        int nx = fl(c.x), nz = fl(c.z);
        if (nx != bx || nz != bz) { // onto the next rail: straight on along it
            speed = std::sqrt(c.mx * c.mx + c.mz * c.mz);
            c.mx = speed * double(nx - bx);
            c.mz = speed * double(nz - bz);
        }
        if (c.type == 2) {
            double pl = double(MathHelper::sqrt_double(c.pushX * c.pushX + c.pushZ * c.pushZ));
            if (pl > 0.01 && c.mx * c.mx + c.mz * c.mz > 0.001) {
                c.pushX /= pl;
                c.pushZ /= pl;
                if (c.pushX * c.mx + c.pushZ * c.mz < 0.0) {
                    c.pushX = c.pushZ = 0.0;
                } else {
                    c.pushX = c.mx;
                    c.pushZ = c.mz;
                }
            }
        }
    } else { // off the rails: it slides and falls like anything else
        c.mx = std::max(-maxV, std::min(maxV, c.mx));
        c.mz = std::max(-maxV, std::min(maxV, c.mz));
        if (c.onGround) {
            c.mx *= 0.5;
            c.my *= 0.5;
            c.mz *= 0.5;
        }
        moveCart(w, c, c.mx, c.my, c.mz);
        if (!c.onGround) {
            c.mx *= double(0.95f);
            c.my *= double(0.95f);
            c.mz *= double(0.95f);
        }
    }
    c.pitch = 0.0f;
    double dx = c.px - c.x, dz = c.pz - c.z;
    if (dx * dx + dz * dz > 0.001) {
        c.yaw = float(std::atan2(dz, dx) * 180.0 / PI_D);
        if (c.flipped) c.yaw += 180.0f;
    }
    double turn = double(c.yaw - c.prevYaw);
    while (turn >= 180.0) turn -= 360.0;
    while (turn < -180.0) turn += 360.0;
    if (turn < -170.0 || turn >= 170.0) {
        c.yaw += 180.0f;
        c.flipped = !c.flipped;
    }
    if (pushing && rnd.nextInt(4) == 0) { // the furnace burns its coal, smoking
        if (--c.fuel < 0) c.pushX = c.pushZ = 0.0;
        if (gParticles) gParticles->spawnLargeSmoke(c.x, c.y + 0.8, c.z);
    }
}

// EntityMinecart.applyEntityCollision: carts push each other (a furnace cart shoves the others
// along), and the player walking into one pushes it
void Vehicles::cartCollisions(Player& p, Mobs* mobsIn) {
    auto shove = [&](Cart& a, double ox, double oz, double& omx, double& omz, Cart* other) {
        double dx = ox - a.x, dz = oz - a.z;
        double d = dx * dx + dz * dz;
        if (d < double(1.0e-4f)) return;
        d = double(MathHelper::sqrt_double(d));
        dx /= d;
        dz /= d;
        double f = std::min(1.0, 1.0 / d);
        dx *= f * double(0.1f) * 0.5;
        dz *= f * double(0.1f) * 0.5;
        if (other) {
            Cart& o = *other;
            double mx = (o.mx + a.mx) / 2.0, mz = (o.mz + a.mz) / 2.0;
            if (o.type == 2 && a.type != 2) {
                a.mx *= double(0.2f);
                a.mz *= double(0.2f);
                a.mx += o.mx - dx;
                a.mz += o.mz - dz;
                o.mx *= double(0.7f);
                o.mz *= double(0.7f);
            } else if (o.type != 2 && a.type == 2) {
                o.mx *= double(0.2f);
                o.mz *= double(0.2f);
                o.mx += a.mx + dx;
                o.mz += a.mz + dz;
                a.mx *= double(0.7f);
                a.mz *= double(0.7f);
            } else {
                a.mx *= double(0.2f);
                a.mz *= double(0.2f);
                a.mx += mx - dx;
                a.mz += mz - dz;
                o.mx *= double(0.2f);
                o.mz *= double(0.2f);
                o.mx += mx + dx;
                o.mz += mz + dz;
            }
        } else {
            a.mx -= dx;
            a.mz -= dz;
            omx += dx / 4.0;
            omz += dz / 4.0;
        }
    };
    for (size_t i = 0; i < carts.size(); ++i) {
        Cart& a = carts[i];
        if (a.dead) continue;
        AABB ab = a.box.expand(double(0.2f), 0.0, double(0.2f));
        for (size_t j = 0; j < carts.size(); ++j) { // EntityMinecart.onUpdate: the carts it touches
            if (i == j || carts[j].dead || carts[j].id == riddenCart || !ab.intersects(carts[j].box)) continue;
            double unused = 0;
            shove(carts[j], a.x, a.z, unused, unused, &a);
        }
        // EntityLiving.collideWithNearbyEntities, from the player's side
        if (a.id != riddenCart && !riding() && !p.dead() && p.boundingBox.expand(double(0.2f), 0.0, double(0.2f)).intersects(a.box))
            shove(a, p.posX, p.posZ, p.motionX, p.motionZ, nullptr);
        // ... and every mob's: an empty plain cart rolling past scoops it up
        if (mobsIn)
            mobsIn->forEachBody([&](const AABB& box, double mx0, double mz0, double& mmx, double& mmz, int& vehicle) {
                if (vehicle != 0 || a.dead || !box.expand(double(0.2f), 0.0, double(0.2f)).intersects(a.box)) return;
                bool empty = a.id != riddenCart && !a.mobAboard;
                if (a.type == 0 && a.mx * a.mx + a.mz * a.mz > 0.01 && empty) {
                    vehicle = -a.id; // mountEntity
                    a.mobAboard = true;
                    return;
                }
                shove(a, mx0, mz0, mmx, mmz, nullptr);
            });
    }
}

// ---------------------------------------------------------------- RenderBoat + ModelBoat

void Vehicles::render(const World& w, Renderer& r, float partial) {
    using namespace boxmodel;
    std::vector<axm::ModelVertex> q;
    {
        for (const Boat& b : boats) {
            if (b.dead) continue;
            float x = float(b.px + (b.x - b.px) * partial), y = float(b.py + (b.y - b.py) * partial),
                  z = float(b.pz + (b.z - b.pz) * partial);
            float yaw = b.prevYaw + (b.yaw - b.prevYaw) * partial;
            float light = w.getBrightness(fl(x), fl(double(y) - double(YOFF) + double(H) * 0.66), fl(z));
            M4 t;
            t.translate(x, y, z);
            t.rotate(180.0f - yaw, 1);
            float ht = float(b.hurtTime) - partial, dmg = std::max(0.0f, float(b.damage) - partial);
            if (ht > 0.0f) t.rotate(MathHelper::sin(ht) * ht * dmg / 10.0f * float(b.hurtDir), 0); // the rock when hit
            t.scale(-1.0f, -1.0f, 1.0f);
            // ModelBoat: the floor and four sides (24 long, 20 wide, 6 high)
            const float L = 24, Wd = 20, Hh = 6, Y = 4;
            box(q, part(t, 0, Y, 0, 3.1415927f * 0.5f, 0, 0), -L / 2, -Wd / 2 + 2, -3, int(L), int(Wd - 4), 4, 0, 8, light);
            box(q, part(t, -L / 2 + 1, Y, 0, 0, 3.1415927f * 3.0f / 2.0f, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
            box(q, part(t, L / 2 - 1, Y, 0, 0, 3.1415927f * 0.5f, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
            box(q, part(t, 0, Y, -Wd / 2 + 1, 0, 3.1415927f, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
            box(q, part(t, 0, Y, Wd / 2 - 1, 0, 0, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
        }
        C3D_Tex* tex = r.texture("romfs:/textures/item/boat.rgba");
        if (tex && !q.empty()) r.drawTriangles(q, tex);
    }
    renderCarts(w, r, partial);
}

// ---------------------------------------------------------------- RenderMinecart + ModelMinecart

namespace {
// RenderBlocks.func_1227_a: a block as a cube round the origin, its inventory faces
void blockCube(std::vector<axm::ModelVertex>& q, const boxmodel::M4& t, int id, float light) {
    static const float C[6][4][3] = {
        {{-.5f, -.5f, -.5f}, {.5f, -.5f, -.5f}, {.5f, -.5f, .5f}, {-.5f, -.5f, .5f}}, // bottom
        {{-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}, {-.5f, .5f, -.5f}},     // top
        {{.5f, -.5f, -.5f}, {-.5f, -.5f, -.5f}, {-.5f, .5f, -.5f}, {.5f, .5f, -.5f}}, // north
        {{-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}},     // south
        {{-.5f, -.5f, -.5f}, {-.5f, -.5f, .5f}, {-.5f, .5f, .5f}, {-.5f, .5f, -.5f}}, // west
        {{.5f, -.5f, .5f}, {.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}},     // east
    };
    static const float SHADE[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
    for (int side = 0; side < 6; ++side) {
        int tile = BLOCKS[id].tex[0][side];
        float u0 = float(tile & 15) / 16.0f, u1 = u0 + 1.0f / 16.0f;
        float v0 = float(tile >> 4) * 16.0f / 1024.0f, v1 = v0 + 16.0f / 1024.0f;
        float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
        boxmodel::quad(q, t, C[side], uv, light * SHADE[side]);
    }
}
} // namespace

void Vehicles::renderCarts(const World& w, Renderer& r, float partial) {
    using namespace boxmodel;
    if (carts.empty()) return;
    std::vector<axm::ModelVertex> body, blocks;
    for (const Cart& k : carts) {
        if (k.dead) continue;
        double x = k.px + (k.x - k.px) * partial, y = k.py + (k.y - k.py) * partial, z = k.pz + (k.z - k.pz) * partial;
        double rx = x, ry = y, rz = z;
        float yaw = k.prevYaw + (k.yaw - k.prevYaw) * partial, pitch = k.pitch;
        V3 mid; // sat on the rail, tipped along slopes
        if (railPoint(w, x, y, z, mid)) {
            V3 a, b;
            if (!railPointAhead(w, x, y, z, 0.3, a)) a = mid;
            if (!railPointAhead(w, x, y, z, -0.3, b)) b = mid;
            rx += mid.x - x;
            ry += (a.y + b.y) / 2.0 - y;
            rz += mid.z - z;
            double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
            double l = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (l != 0.0) {
                dx /= l;
                dy /= l;
                dz /= l;
                yaw = float(std::atan2(dz, dx) * 180.0 / PI_D);
                pitch = float(std::atan(dy) * 73.0);
            }
        }
        float light = w.getBrightness(fl(x), fl(y - double(CYOFF) + double(CH) * 0.66), fl(z));
        M4 t;
        t.translate(float(rx), float(ry), float(rz));
        t.rotate(180.0f - yaw, 1);
        t.rotate(-pitch, 2);
        float ht = float(k.hurtTime) - partial, dmg = std::max(0.0f, float(k.damage) - partial);
        if (ht > 0.0f) t.rotate(MathHelper::sin(ht) * ht * dmg / 10.0f * float(k.hurtDir), 0);
        if (k.type != 0) { // the chest or furnace riding in it
            M4 bt = t;
            bt.scale(0.75f, 0.75f, 0.75f);
            bt.translate(0.0f, 5.0f / 16.0f, 0.0f);
            bt.rotate(90.0f, 1);
            blockCube(blocks, bt, k.type == 1 ? 54 : 61, light);
        }
        t.scale(-1.0f, -1.0f, 1.0f);
        const float L = 20, Hh = 8, Wd = 16, Y = 4;
        box(body, part(t, 0, Y, 0, 3.1415927f * 0.5f, 0, 0), -L / 2, -Wd / 2, -1, int(L), int(Wd), 2, 0, 10, light);
        box(body, part(t, 0, Y + 0.1f, 0, 3.1415927f * -0.5f, 0, 0), -L / 2 + 1, -Wd / 2 + 1, -1, int(L - 2), int(Wd - 2), 1, 44, 10, light);
        box(body, part(t, -L / 2 + 1, Y, 0, 0, 3.1415927f * 3.0f / 2.0f, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
        box(body, part(t, L / 2 - 1, Y, 0, 0, 3.1415927f * 0.5f, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
        box(body, part(t, 0, Y, -Wd / 2 + 1, 0, 3.1415927f, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
        box(body, part(t, 0, Y, Wd / 2 - 1, 0, 0, 0), -L / 2 + 2, -Hh - 1, -1, int(L - 4), int(Hh), 2, 0, 0, light);
    }
    if (!blocks.empty()) r.drawTriangles(blocks, r.terrainTexture());
    if (C3D_Tex* tex = r.texture("romfs:/textures/item/cart.rgba")) r.drawTriangles(body, tex);
}
