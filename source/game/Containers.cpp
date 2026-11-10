#include "Containers.h"
#include <algorithm>
#include <array>
#include <unordered_map>
#include "Survival.h"
#include "world/World.h"

namespace containers {
namespace {
bool isChest(int id) { return id == 54; }
std::unordered_map<int64_t, int> currentBurn; // TileEntityFurnace.currentItemBurnTime (not saved)
int64_t posKey(int x, int y, int z) { return (int64_t(x) & 0x3FFFFFF) << 34 | (int64_t(z) & 0x3FFFFFF) << 8 | (y & 255); }

std::vector<ItemStack> readItems(nbt::Tag* te, int size) {
    std::vector<ItemStack> v(size);
    if (!te) return v;
    if (nbt::Tag* list = te->getList("Items"))
        for (auto& t : list->items) {
            int slot = t->getByte("Slot") & 255;
            if (slot < size) v[slot] = fromNBT(*t);
        }
    return v;
}

void writeItems(nbt::Tag* te, const ItemStack* s, int size) {
    if (!te) return;
    nbt::TagPtr list = nbt::makeList(nbt::Compound);
    for (int i = 0; i < size; ++i) {
        if (s[i].empty()) continue;
        nbt::TagPtr t = nbt::makeCompound();
        t->setByte("Slot", int8_t(i));
        toNBT(s[i], *t);
        list->items.push_back(std::move(t));
    }
    te->set("Items", std::move(list));
}
} // namespace

ItemStack fromNBT(const nbt::Tag& t) {
    ItemStack s;
    s.id = uint16_t(t.getShort("id"));
    s.count = t.getByte("Count");
    s.damage = t.getShort("Damage");
    if (nbt::Tag* tag = t.getCompound("tag")) s.tag = std::shared_ptr<nbt::Tag>(nbt::clone(*tag).release());
    return s;
}

void toNBT(const ItemStack& s, nbt::Tag& t) {
    t.setShort("id", int16_t(s.id));
    t.setByte("Count", int8_t(s.count));
    t.setShort("Damage", s.damage);
    if (s.tag) t.set("tag", nbt::clone(*s.tag));
}

bool isContainer(int id) { return id == 54 || id == 142 || id == 61 || id == 62; }

bool open(World& w, Inventory& inv, int x, int y, int z) {
    int id = w.getBlockId(x, y, z);
    if (!isContainer(id)) return false;
    inv.containerParts = 0;
    auto add = [&](int px, int py, int pz) {
        int k = inv.containerParts++;
        inv.containerPos[k][0] = px;
        inv.containerPos[k][1] = py;
        inv.containerPos[k][2] = pz;
    };
    if (id == 61 || id == 62) {
        add(x, y, z);
        inv.containerKind = Inventory::FURNACE;
        inv.container = readItems(w.tileEntityAt(x, y, z), 3);
        refresh(w, inv);
        return true;
    }
    if (isChest(id)) {
        // a block on top keeps it shut (on either half)
        if (w.isBlockOpaqueCube(x, y + 1, z)) return false;
        static const int N[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        for (auto& n : N)
            if (w.getBlockId(x + n[0], y, z + n[1]) == id && w.isBlockOpaqueCube(x + n[0], y + 1, z + n[1])) return false;
        // InventoryLargeChest: west / north half first
        int other = -1;
        for (int k = 0; k < 4; ++k)
            if (w.getBlockId(x + N[k][0], y, z + N[k][1]) == id) other = k;
        if (other == 0 || other == 2) add(x + N[other][0], y, z + N[other][1]);
        add(x, y, z);
        if (other == 1 || other == 3) add(x + N[other][0], y, z + N[other][1]);
    } else {
        add(x, y, z); // crate
    }
    inv.containerKind = Inventory::CHEST;
    inv.container.clear();
    for (int k = 0; k < inv.containerParts; ++k) {
        auto part = readItems(w.tileEntityAt(inv.containerPos[k][0], inv.containerPos[k][1], inv.containerPos[k][2]), 27);
        inv.container.insert(inv.container.end(), part.begin(), part.end());
    }
    return true;
}

void (*saveCart)(int cartId, const std::vector<ItemStack>& items) = nullptr;

void openCart(Inventory& inv, int cartId, const std::vector<ItemStack>& items) {
    inv.containerParts = 0;
    inv.cartId = cartId;
    inv.containerKind = Inventory::CHEST;
    inv.container = items;
    inv.container.resize(27);
}

void save(World& w, Inventory& inv) {
    if (inv.containerKind == Inventory::NONE) return;
    if (inv.cartId) {
        if (saveCart) saveCart(inv.cartId, inv.container);
        inv.containerDirty = false;
        return;
    }
    int per = inv.containerKind == Inventory::FURNACE ? 3 : 27;
    for (int k = 0; k < inv.containerParts; ++k) {
        auto& p = inv.containerPos[k];
        nbt::Tag* te = w.tileEntityAt(p[0], p[1], p[2]);
        if (int(inv.container.size()) >= (k + 1) * per) writeItems(te, &inv.container[k * per], per);
        if (Chunk* c = w.getChunk(p[0] >> 4, p[2] >> 4)) c->modified = true;
    }
    inv.containerDirty = false;
}

void refresh(World& w, Inventory& inv) {
    if (inv.containerKind != Inventory::FURNACE) return;
    auto& p = inv.containerPos[0];
    nbt::Tag* te = w.tileEntityAt(p[0], p[1], p[2]);
    if (!te) return;
    if (!inv.containerDirty && inv.cursor.empty()) inv.container = readItems(te, 3);
    int cur = currentBurn.count(posKey(p[0], p[1], p[2])) ? currentBurn[posKey(p[0], p[1], p[2])] : 0;
    if (cur == 0) cur = 200;
    inv.furnaceBurn = float(te->getShort("BurnTime")) / float(cur);
    inv.furnaceCook = float(te->getShort("CookTime")) / 200.0f;
}

void close(World& w, Inventory& inv) {
    save(w, inv);
    inv.cartId = 0;
    inv.containerKind = Inventory::NONE;
    inv.container.clear();
}

// TileEntityFurnace.updateEntity
static void tickFurnace(World& w, int x, int y, int z) {
    nbt::Tag* te = w.tileEntityAt(x, y, z);
    if (!te) return;
    std::vector<ItemStack> s = readItems(te, 3);
    int burn = te->getShort("BurnTime"), cook = te->getShort("CookTime");
    int64_t key = posKey(x, y, z);
    if (!currentBurn.count(key)) currentBurn[key] = s[1].empty() ? 0 : survival::burnTime(s[1].id, s[1].damage);
    int& cur = currentBurn[key];
    auto canSmelt = [&]() {
        if (s[0].empty()) return false;
        survival::Stack r = survival::smelt(s[0].id, s[0].damage);
        if (r.id <= 0) return false;
        if (s[2].empty()) return true;
        if (s[2].id != r.id || s[2].damage != r.damage) return false;
        if (s[2].count < 64 && s[2].count < survival::maxStack(s[2].id)) return true;
        return s[2].count < survival::maxStack(r.id);
    };
    bool wasBurning = burn > 0, changed = false;
    if (burn > 0) --burn;
    if (burn == 0 && canSmelt()) {
        cur = burn = s[1].empty() ? 0 : survival::burnTime(s[1].id, s[1].damage);
        if (burn > 0) {
            changed = true;
            if (!s[1].empty() && --s[1].count == 0) {
                // TODO(port): a lava bucket leaves... Beta 1.2 just uses it up
                s[1].clear();
            }
        }
    }
    if (burn > 0 && canSmelt()) {
        if (++cook == 200) {
            cook = 0;
            survival::Stack r = survival::smelt(s[0].id, s[0].damage);
            if (s[2].empty()) {
                s[2].id = uint16_t(r.id);
                s[2].count = r.count;
                s[2].damage = r.damage;
            } else if (s[2].id == r.id) {
                ++s[2].count;
            }
            if (--s[0].count <= 0) s[0].clear();
            changed = true;
        }
    } else {
        cook = 0;
    }
    te->setShort("BurnTime", int16_t(burn));
    te->setShort("CookTime", int16_t(cook));
    if (changed) writeItems(te, s.data(), 3);
    if (Chunk* c = w.getChunk(x >> 4, z >> 4)) c->modified = true;
    if (wasBurning != (burn > 0)) {
        // BlockFurnace.updateFurnaceBlockState: swap lit / unlit, keeping the facing and contents
        nbt::TagPtr keep = nbt::clone(*te);
        int meta = w.getBlockMetadata(x, y, z);
        w.setBlockWithNotify(x, y, z, burn > 0 ? 62 : 61);
        w.setBlockMetadataWithNotify(x, y, z, meta);
        if (nbt::Tag* fresh = w.tileEntityAt(x, y, z)) *fresh = std::move(*keep);
        w.markBlockNeedsUpdate(x, y, z);
    }
}

void tickFurnaces(World& w) {
    std::vector<std::array<int, 3>> furnaces;
    w.forEachChunk([&](Chunk& c) {
        if (!c.level) return;
        nbt::Tag* list = c.level->getList("TileEntities");
        if (!list) return;
        for (auto& t : list->items)
            if (t->getString("id") == "Furnace") furnaces.push_back({t->getInt("x"), t->getInt("y"), t->getInt("z")});
    });
    for (auto& f : furnaces) {
        int id = w.getBlockId(f[0], f[1], f[2]);
        if (id == 61 || id == 62) tickFurnace(w, f[0], f[1], f[2]);
    }
}
} // namespace containers
