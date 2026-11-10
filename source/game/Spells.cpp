#include "Spells.h"
#include <cmath>
#include "Inventory.h"
#include "Mobs.h"
#include "Player.h"
#include "audio/Sound.h"
#include "render/Particles.h"
#include "world/BlockProps.h"
#include "world/MathHelper.h"
#include "world/Nbt.h"
#include "world/World.h"

namespace spells {

const Spell SPELLS[COUNT] = {
    {"magicMissile", "Magic Missile", 2, 40, 16},
    {"fireball", "Fireball", 6, 8, 0},
    {"heal", "Heal", 8, 15, 5},
    {"frostBolt", "Frost Bolt", 4, 15, 8},
    {"light", "Light", 3, 30, 12},
    {"blink", "Blink", 20, 2, 0},
};

namespace {
constexpr int MAGIC_LIGHT = 111;
constexpr float PI_F = 3.1415927f;

nbt::Tag& tagOf(ItemStack& s) {
    if (!s.tag) s.tag = nbt::makeCompound();
    return *s.tag;
}

void setUses(ItemStack& book, int index, int value) {
    std::vector<uint8_t> sp = spellsIn(book), uses(sp.size());
    for (size_t i = 0; i < sp.size(); ++i) uses[i] = uint8_t(int(i) == index ? value : usesOf(book, int(i)));
    tagOf(book).setByteArray("Uses", uses.data(), uses.size());
}

bool learn(ItemStack& book, int id, bool eternal) {
    const Spell* s = get(id);
    int gained = eternal || s->usesPerPage <= 0 ? PERMANENT : s->usesPerPage;
    std::vector<uint8_t> sp = spellsIn(book);
    for (size_t i = 0; i < sp.size(); ++i)
        if (sp[i] == id) {
            int cur = usesOf(book, int(i));
            if (cur == PERMANENT) return false;
            setUses(book, int(i), gained == PERMANENT ? PERMANENT : std::min(MAX_USES, cur + gained));
            return true;
        }
    std::vector<uint8_t> uses(sp.size());
    for (size_t i = 0; i < sp.size(); ++i) uses[i] = uint8_t(usesOf(book, int(i)));
    bool first = sp.empty();
    sp.push_back(uint8_t(id));
    uses.push_back(uint8_t(gained));
    tagOf(book).setByteArray("Spells", sp.data(), sp.size());
    tagOf(book).setByteArray("Uses", uses.data(), uses.size());
    if (first) setSelected(book, 0);
    return true;
}

void removeSpell(ItemStack& book, int index) {
    std::vector<uint8_t> old = spellsIn(book), sp, uses;
    for (size_t i = 0; i < old.size(); ++i)
        if (int(i) != index) {
            sp.push_back(old[i]);
            uses.push_back(uint8_t(usesOf(book, int(i))));
        }
    tagOf(book).setByteArray("Spells", sp.data(), sp.size());
    tagOf(book).setByteArray("Uses", uses.data(), uses.size());
    int sel = book.tag->getByte("Selected");
    if (sel >= index && sel > 0) setSelected(book, sel - 1);
}

void look(const Player& p, double& lx, double& ly, double& lz) {
    float yaw = p.rotationYaw, pitch = p.rotationPitch;
    lx = -MathHelper::sin(yaw * PI_F / 180.0f) * MathHelper::cos(pitch * PI_F / 180.0f);
    ly = -MathHelper::sin(pitch * PI_F / 180.0f);
    lz = MathHelper::cos(yaw * PI_F / 180.0f) * MathHelper::cos(pitch * PI_F / 180.0f);
}

double rnd() { static JavaRandom r(int64_t(0x5eed)); return r.nextDouble(); }

bool castHeal(Player& p) {
    if (p.health >= 20) return false; // already at full health, don't waste mana
    p.heal(6);
    sound::play("random.pop", p.posX, p.posY, p.posZ, 0.8f, 0.6f);
    if (gParticles)
        for (int i = 0; i < 12; ++i)
            gParticles->spawn(Particles::REDDUST, p.posX + (rnd() - 0.5), p.posY - 1.0 + rnd() * 2.0, p.posZ + (rnd() - 0.5), 0, 0, 0);
    return true;
}

bool castLight(World& w, Player& p, bool editWorld) {
    if (!editWorld) return false;
    double ex = p.posX, ey = p.posY + 0.12, ez = p.posZ, lx, ly, lz;
    look(p, lx, ly, lz);
    RayHit hit = w.rayTrace(float(ex), float(ey), float(ez), float(lx), float(ly), float(lz), 6.0f);
    int x, y, z;
    if (hit.hit) {
        x = hit.x + hit.nx;
        y = hit.y + hit.ny;
        z = hit.z + hit.nz;
    } else {
        x = MathHelper::floor_double(ex + lx * 3.0);
        y = MathHelper::floor_double(ey + ly * 3.0);
        z = MathHelper::floor_double(ez + lz * 3.0);
    }
    if (w.getBlockId(x, y, z) != 0) return false;
    w.setBlockWithNotify(x, y, z, MAGIC_LIGHT);
    sound::play("random.pop", x + 0.5, y + 0.5, z + 0.5, 0.6f, 1.4f);
    return true;
}

bool blocked(World& w, int x, int y, int z) { return matSolid(blockMat(uint8_t(w.getBlockId(x, y, z)))); }

void portalBurst(double x, double y, double z) {
    if (!gParticles) return;
    for (int i = 0; i < 24; ++i)
        gParticles->spawn(Particles::PORTAL, x + (rnd() - 0.5), y - 1.0 + rnd() * 2.0, z + (rnd() - 0.5), (rnd() - 0.5) * 2.0, -rnd(), (rnd() - 0.5) * 2.0);
}

bool castBlink(World& w, Player& p) {
    const double RANGE = 16.0;
    double ex = p.posX, ey = p.posY + 0.12, ez = p.posZ, lx, ly, lz;
    look(p, lx, ly, lz);
    double tx = ex + lx * RANGE, ty = ey + ly * RANGE, tz = ez + lz * RANGE;
    RayHit hit = w.rayTrace(float(ex), float(ey), float(ez), float(lx), float(ly), float(lz), float(RANGE));
    if (hit.hit) { // the point on the face we hit, then half a block back
        double t = RANGE;
        if (hit.nx != 0) t = (hit.x + (hit.nx > 0 ? 1.0 : 0.0) - ex) / lx;
        else if (hit.ny != 0) t = (hit.y + (hit.ny > 0 ? 1.0 : 0.0) - ey) / ly;
        else if (hit.nz != 0) t = (hit.z + (hit.nz > 0 ? 1.0 : 0.0) - ez) / lz;
        tx = ex + lx * t - lx * 0.5;
        ty = ey + ly * t - ly * 0.5;
        tz = ez + lz * t - lz * 0.5;
    }
    double feetOffset = 0.12 + 1.62;
    int bx = MathHelper::floor_double(tx), bz = MathHelper::floor_double(tz), by = MathHelper::floor_double(ty - feetOffset);
    static const int TRIES[4] = {0, 1, -1, 2};
    for (int d : TRIES) {
        int y = by + d;
        if (blocked(w, bx, y, bz) || blocked(w, bx, y + 1, bz)) continue;
        double nx = bx + 0.5, nz = bz + 0.5, ny = y + 1.62;
        double dd = (nx - p.posX) * (nx - p.posX) + (nz - p.posZ) * (nz - p.posZ) + (ny - p.posY) * (ny - p.posY);
        if (dd < 1.0) return false; // nowhere to go
        portalBurst(p.posX, p.posY, p.posZ);
        p.setPosition(nx, ny, nz);
        p.motionX = p.motionY = p.motionZ = 0;
        p.fallDistance = 0;
        portalBurst(p.posX, p.posY, p.posZ);
        sound::play("random.pop", p.posX, p.posY, p.posZ, 1.0f, 0.5f);
        return true;
    }
    return false;
}
} // namespace

std::vector<uint8_t> spellsIn(const ItemStack& book) {
    if (!book.tag) return {};
    const std::vector<uint8_t>* a = book.tag->getByteArray("Spells");
    return a ? *a : std::vector<uint8_t>{};
}

int usesOf(const ItemStack& book, int index) {
    std::vector<uint8_t> sp = spellsIn(book);
    if (index < 0 || index >= int(sp.size())) return 0;
    const Spell* s = get(sp[index]);
    if (s && s->usesPerPage <= 0) return PERMANENT;
    const std::vector<uint8_t>* u = book.tag ? book.tag->getByteArray("Uses") : nullptr;
    if (u && index < int(u->size())) return (*u)[index];
    return s ? s->usesPerPage : 0;
}

int selectedIndex(const ItemStack& book) {
    std::vector<uint8_t> sp = spellsIn(book);
    if (sp.empty()) return -1;
    int i = book.tag->getByte("Selected");
    return i >= 0 && i < int(sp.size()) ? i : 0;
}

void setSelected(ItemStack& book, int index) { tagOf(book).setByte("Selected", int8_t(index)); }

std::string usePage(Inventory& inv, ItemStack& page, bool& used) {
    used = false;
    const Spell* s = get(page.damage);
    if (!s) return "";
    ItemStack* book = nullptr;
    for (int i = 0; i < Inventory::MAIN && !book; ++i)
        if (inv.main[i].id == BOOK && !inv.main[i].empty()) book = &inv.main[i];
    if (!book) return "You need a spell book to learn this.";
    int id = page.damage;
    bool known = false;
    for (uint8_t v : spellsIn(*book)) known |= v == id;
    bool eternal = page.tag && page.tag->getBool("Eternal");
    if (!learn(*book, id, eternal)) return std::string("Your spell book already knows ") + s->name + " for good.";
    std::vector<uint8_t> sp = spellsIn(*book);
    int idx = 0;
    while (idx < int(sp.size()) && sp[idx] != id) ++idx;
    int uses = usesOf(*book, idx);
    used = true;
    if (--page.count <= 0) page.clear();
    if (uses == PERMANENT) return std::string("Learned ") + s->name + " for good!";
    if (known) return std::string(s->name) + " now has " + std::to_string(uses) + " uses.";
    return std::string("Learned ") + s->name + " (" + std::to_string(uses) + " uses)!";
}

bool castSelected(ItemStack& book, World& w, Player& p, Mobs& mobs, bool creative, bool editWorld, std::string& msg) {
    int index = selectedIndex(book);
    const Spell* s = index < 0 ? nullptr : get(spellsIn(book)[index]);
    if (!s) { msg = "The book is empty."; return false; }
    if (!creative && p.mana < s->mana) {
        sound::play("random.fizz", p.posX, p.posY, p.posZ, 0.5f, 1.5f);
        return false;
    }
    p.swingItem();
    int id = int(s - SPELLS);
    bool ok;
    switch (id) {
    case 0: case 1: case 3:
        mobs.castBolt(id, p);
        if (id == 0) sound::play("random.pop", p.posX, p.posY, p.posZ, 0.6f, 1.6f);
        if (id == 1) sound::play("mob.ghast.fireball", p.posX, p.posY, p.posZ, 0.6f, 1.2f);
        if (id == 3) sound::play("random.bow", p.posX, p.posY, p.posZ, 0.5f, 1.8f);
        ok = true;
        break;
    case 2: ok = castHeal(p); break;
    case 4: ok = castLight(w, p, editWorld); break;
    default: ok = castBlink(w, p); break;
    }
    if (!ok) {
        sound::play("random.fizz", p.posX, p.posY, p.posZ, 0.5f, 1.5f);
        return false;
    }
    if (!creative) p.mana -= s->mana;
    int uses = usesOf(book, index);
    if (uses != PERMANENT && !creative) {
        if (uses <= 1) {
            removeSpell(book, index);
            msg = std::string("Your ") + s->name + " spell has faded from the book.";
        } else {
            setUses(book, index, uses - 1);
        }
    }
    return true;
}

} // namespace spells
