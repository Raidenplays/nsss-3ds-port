#include "RecipeBook.h"
#include "game/ItemTable.h"
#include <algorithm>
#include <cstring>

namespace recipes {
using namespace survival;

namespace {

// a recipe's ingredients, grouped
std::vector<Need> needsOf(int recipe) {
    std::vector<Need> out;
    auto add = [&](const Ingredient& in) {
        if (in.id < 0) return;
        for (Need& n : out)
            if (n.what.id == in.id && n.what.damage == in.damage) { ++n.count; return; }
        Need n;
        n.what = in;
        n.count = 1;
        out.push_back(n);
    };
    if (recipe >= 0) {
        const Shaped& s = SHAPED[recipe];
        for (int i = 0; i < s.w * s.h; ++i) add(s.in[i]);
    } else {
        const Shapeless& s = SHAPELESS[-recipe - 1];
        for (int i = 0; i < s.n; ++i) add(s.in[i]);
    }
    return out;
}

bool fits2x2(int recipe) {
    if (recipe >= 0) return SHAPED[recipe].w <= 2 && SHAPED[recipe].h <= 2;
    return SHAPELESS[-recipe - 1].n <= 4;
}

Stack outOf(int recipe) { return recipe >= 0 ? SHAPED[recipe].out : SHAPELESS[-recipe - 1].out; }

// Takes one craft's worth out of `slots` (the storage slots), backpack first. False if short.
// `take` records which slots gave what so the caller can apply it.
bool payFrom(ItemStack* slots, const Entry& e, std::vector<std::pair<int, int>>* take) {
    for (const Need& n : e.needs) {
        int left = n.count;
        // backpack (9..35) first, then the hotbar, so the things in hand stay put
        for (int pass = 0; pass < 2 && left > 0; ++pass)
            for (int i = pass == 0 ? Inventory::HOTBAR : 0; i < (pass == 0 ? Inventory::STORAGE : Inventory::HOTBAR) && left > 0; ++i) {
                ItemStack& st = slots[i];
                if (st.empty() || !accepts(n.what, st.id, st.damage)) continue;
                int t = std::min(left, st.count);
                st.count -= t;
                left -= t;
                if (take) take->push_back({i, t});
            }
        if (left > 0) return false;
    }
    return true;
}

bool isFood(int id) {
    switch (id) {
    case 260: case 282: case 297: case 319: case 320: case 322: case 349: case 350: case 354: case 357: return true;
    }
    return false;
}

} // namespace

Category categoryOf(int id, int) {
    if (id < 256 || id == 323 || id == 324 || id == 330) return BLOCKS;
    if ((id >= 256 && id <= 259) || id == 261 || id == 262 || (id >= 267 && id <= 279) || (id >= 283 && id <= 286) ||
        (id >= 290 && id <= 294) || (id >= 325 && id <= 327) || id == 345 || id == 346 || id == 347 || id == 359)
        return TOOLS;
    if ((id >= 298 && id <= 317) || isFood(id)) return ARMOUR_FOOD;
    return MISC;
}

std::vector<Entry> list(const Inventory& inv, bool table, Category c) {
    std::vector<Entry> out;
    auto consider = [&](int r) {
        if (!table && !fits2x2(r)) return;
        Stack o = outOf(r);
        if (categoryOf(o.id, o.damage) != c) return;
        Entry e;
        e.out = o;
        e.category = c;
        e.recipe = r;
        e.needs = needsOf(r);
        for (Need& n : e.needs)
            for (int i = 0; i < Inventory::STORAGE; ++i) {
                const ItemStack& st = inv.main[i];
                if (!st.empty() && accepts(n.what, st.id, st.damage)) n.have += st.count;
            }
        ItemStack copy[Inventory::MAIN];
        for (int i = 0; i < Inventory::STORAGE; ++i) copy[i] = inv.main[i];
        e.craftable = payFrom(copy, e, nullptr);
        for (Entry& had : out)
            if (had.out.id == o.id && had.out.damage == o.damage) {
                if (!had.craftable && e.craftable) had = e;
                return;
            }
        out.push_back(e);
    };
    for (int r = 0; r < SHAPED_COUNT; ++r) consider(r);
    for (int r = 0; r < SHAPELESS_COUNT; ++r) consider(-r - 1);
    std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.craftable > b.craftable; });
    return out;
}

int timesCraftable(const Inventory& inv, const Entry& e) {
    int per = e.out.count > 0 ? e.out.count : 1;
    int limit = std::max(1, maxStack(e.out.id) / per);
    ItemStack copy[Inventory::MAIN];
    for (int i = 0; i < Inventory::STORAGE; ++i) copy[i] = inv.main[i];
    int n = 0;
    while (n < limit && payFrom(copy, e, nullptr)) ++n;
    return n;
}

bool craft(Inventory& inv, const Entry& e) {
    {
        ItemStack copy[Inventory::MAIN];
        for (int i = 0; i < Inventory::STORAGE; ++i) copy[i] = inv.main[i];
        if (!payFrom(copy, e, nullptr)) return false;
    }
    std::vector<std::pair<int, int>> take;
    payFrom(inv.main, e, &take);
    std::vector<ItemStack> back; // SlotCrafting: a full bucket leaves the bucket (getContainerItem)
    for (auto& t : take) {
        ItemStack& st = inv.main[t.first];
        if (st.count <= 0) {
            int id = st.id;
            st.clear();
            if (id == 326 || id == 327 || id == 335 || id == 380) {
                ItemStack b;
                b.id = 325;
                b.count = t.second;
                back.push_back(b);
            }
        } else if (st.id == 326 || st.id == 327 || st.id == 335 || st.id == 380) { // (they don't stack, but just in case)
            ItemStack b;
            b.id = 325;
            b.count = t.second;
            back.push_back(b);
        }
    }
    ItemStack made;
    made.id = uint16_t(e.out.id);
    made.count = e.out.count;
    made.damage = e.out.damage;
    back.insert(back.begin(), made);
    for (ItemStack& s : back) {
        inv.addStack(s);
        if (s.count > 0) inv.spill.push_back(s);
    }
    inv.updateCraftResult();
    return true;
}

// ------------------------------------------------------------------ descriptions

namespace {
bool has(const char* name, const char* part) { return name && std::strstr(name, part) != nullptr; }
bool ends(const char* name, const char* tail) {
    if (!name) return false;
    size_t a = std::strlen(name), b = std::strlen(tail);
    return a >= b && std::strcmp(name + a - b, tail) == 0;
}
const char* tierOf(int id) {
    // the material that sets how long a tool lasts
    if (id == 268 || id == 269 || id == 270 || id == 271 || id == 290 || id == 1015 || id == 1020 || id == 1025 || id == 1030) return "wood";
    if (id == 272 || id == 273 || id == 274 || id == 275 || id == 291 || id == 1016 || id == 1021 || id == 1026 || id == 1031) return "stone";
    if (id == 256 || id == 257 || id == 258 || id == 267 || id == 292 || id == 1017 || id == 1022 || id == 1027 || id == 1032) return "iron";
    if (id == 276 || id == 277 || id == 278 || id == 279 || id == 293 || id == 1018 || id == 1023 || id == 1028 || id == 1033) return "diamond";
    return "gold";
}
} // namespace

} // namespace recipes
