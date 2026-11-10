#include "RecipeBook.h"
#include "game/ItemTable.h"
#include <algorithm>
#include <cstring>

namespace recipes {
using namespace survival;

namespace {

const std::string* gAch = nullptr;
bool hasAch(const char* a) {
    if (!gAch) return false;
    const std::string& s = *gAch;
    size_t n = std::strlen(a), p = 0;
    while (p <= s.size()) {
        size_t e = s.find(',', p);
        if (e == std::string::npos) e = s.size();
        if (e - p == n && s.compare(p, n, a) == 0) return true;
        p = e + 1;
    }
    return false;
}

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
                if (st.empty() || !accepts(n.what, st.id, st.damage, e.out.id)) continue;
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
        (id >= 290 && id <= 294) || (id >= 325 && id <= 327) || id == 345 || id == 346 || id == 347 || id == 359 ||
        (id >= 1015 && id <= 1038) || id == 1042 || id == 1001)
        return TOOLS;
    if ((id >= 298 && id <= 317) || (id >= 1048 && id <= 1050) || isFood(id)) return ARMOUR_FOOD;
    return MISC;
}

std::vector<Entry> list(const Inventory& inv, bool table, Category c) {
    gAch = &inv.achievements;
    std::vector<Entry> out;
    auto consider = [&](int r) {
        if (!table && !fits2x2(r)) return;
        Stack o = outOf(r);
        if (categoryOf(o.id, o.damage) != c || locked(o.id, hasAch)) return;
        Entry e;
        e.out = o;
        e.category = c;
        e.recipe = r;
        e.needs = needsOf(r);
        for (Need& n : e.needs)
            for (int i = 0; i < Inventory::STORAGE; ++i) {
                const ItemStack& st = inv.main[i];
                if (!st.empty() && accepts(n.what, st.id, st.damage, o.id)) n.have += st.count;
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
    std::vector<ItemStack> back; // SlotCrafting: a bucket of milk, water or lava leaves the bucket
    for (auto& t : take) {
        ItemStack& st = inv.main[t.first];
        if (st.count <= 0) {
            int id = st.id;
            st.clear();
            if (id == 326 || id == 327 || id == 335) {
                ItemStack b;
                b.id = 325;
                b.count = t.second;
                back.push_back(b);
            }
        } else if (st.id == 326 || st.id == 327 || st.id == 335) { // (they don't stack, but just in case)
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

// ------------------------------------------------------------------ the stonecutter

std::vector<Stack> stonecut(int id, int damage) {
    std::vector<Stack> r;
    auto add = [&](int i, int n, int m) { r.push_back(Stack{int16_t(i), int8_t(n), int16_t(m)}); };
    // StonecutterRecipes: a stone only cuts into its own pieces (slabs, stairs, cobble, polished, bricks)
    if (id == 4) { add(44, 2, 4); add(67, 1, 0); add(1, 1, 0); }
    else if (id == 1) { add(44, 2, 1); add(104, 1, 0); add(4, 1, 0); }
    else if (id == 100) { add(44, 2, 0); add(108, 1, 0); add(1, 4, 0); }
    else if (id == 101) { add(44, 2, 5); add(107, 1, 0); }
    else if (id == 102) { add(44, 2, 2); add(105, 1, 0); add(101, 4, 0); }
    else if (id == 103) { add(44, 2, 3); add(106, 1, 0); add(102, 4, 0); }
    else if ((id >= 171 && id <= 177) || id == 178 || id == 179 || id == 180) {
        // AxeolonStone: 171-177 the natural stones, 178 cobble, 179 polished, 180 bricks (type in the damage)
        const int STONE = 0, COBBLE = 1, POLISHED = 2, BRICKS = 3;
        int kind = id == 178 ? COBBLE : id == 179 ? POLISHED : id == 180 ? BRICKS : STONE;
        int type = kind == STONE ? id - 171 : (damage < 0 || damage >= 7 ? 0 : damage);
        static const int SLABS[4] = {182, 184, 186, 188};
        add(SLABS[kind], 2, type);
        add(189 + type * 4 + kind, 1, 0);
        if (kind == STONE) add(178, 1, type);
        if (kind == COBBLE) add(171 + type, 1, 0); // back into its stone
        if (kind <= POLISHED) add(kind == POLISHED ? 180 : 179, 1, type);
    }
    else if (id == 225) { add(229, 2, 0); add(227, 1, 0); }
    else if (id == 226) { add(229, 2, 1); add(228, 1, 0); }
    else if (id == 22) { add(225, 1, 0); }
    else if (id == 224) { add(226, 1, 0); }
    else if (id == 12) { add(24, 1, 0); }
    else if (id == 24) { add(44, 2, 6); add(112, 1, 0); }
    return r;
}

std::vector<Entry> stonecutList(const Inventory& inv) {
    std::vector<Entry> out;
    std::vector<std::pair<int, int>> seen;
    for (int i = 0; i < Inventory::STORAGE; ++i) {
        const ItemStack& st = inv.main[i];
        if (st.empty() || st.id >= 256) continue;
        std::pair<int, int> key(st.id, st.damage);
        if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
        seen.push_back(key);
        int have = 0;
        for (int k = 0; k < Inventory::STORAGE; ++k)
            if (inv.main[k].id == st.id && inv.main[k].damage == st.damage && !inv.main[k].empty()) have += inv.main[k].count;
        for (const Stack& o : stonecut(st.id, st.damage)) {
            Entry e;
            e.out = o;
            e.category = BLOCKS;
            e.recipe = 0;
            Need n;
            n.what = Ingredient{int16_t(st.id), st.damage};
            n.count = 1;
            n.have = have;
            e.needs.push_back(n);
            e.craftable = true;
            out.push_back(e);
        }
    }
    return out;
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
std::string tierLine(int id) {
    std::string t = tierOf(id);
    if (t == "wood") return " Wood wears out fast.";
    if (t == "stone") return " Stone lasts a good while.";
    if (t == "iron") return " Iron is tough and dependable.";
    if (t == "diamond") return " Diamond lasts almost forever.";
    return " Gold is fast, but breaks quickly.";
}
} // namespace

std::string describe(int id, int damage) {
    const ItemInfo* info = itemInfo(id, damage);
    const char* name = info ? info->name : "";
    switch (id) {
    // ---- tools
    case 270: case 274: case 257: case 278: case 285:
        return std::string("Mines stone and ore. Better pickaxes can mine harder ores.") + tierLine(id);
    case 269: case 273: case 256: case 277: case 284:
        return std::string("Digs dirt, sand, gravel, clay and snow quickly.") + tierLine(id);
    case 271: case 275: case 258: case 279: case 286:
        return std::string("Chops wood, planks and other wooden blocks quickly.") + tierLine(id);
    case 290: case 291: case 292: case 293: case 294:
        return std::string("Tills dirt and grass into farmland for planting seeds.") + tierLine(id);
    case 268: case 272: case 267: case 276: case 283:
        return std::string("The classic weapon. In battle mode it also fits the off hand.") + tierLine(id);
    case 1015: case 1016: case 1017: case 1018: case 1019:
        return std::string("A quick, light blade. Fits the off hand in battle mode.") + tierLine(id);
    case 1020: case 1021: case 1022: case 1023: case 1024:
        return std::string("Hits from 5 blocks away instead of 3. Keep your distance.") + tierLine(id);
    case 1025: case 1026: case 1027: case 1028: case 1029:
        return std::string("A heavy axe for fighting. Hits harder than any sword.") + tierLine(id);
    case 1030: case 1031: case 1032: case 1033: case 1034:
        return std::string("Knocks whatever it hits flying. Any stone works for the stone one.") + tierLine(id);
    case 1035: case 1036: case 1037: case 1038: case 1042:
        return "Goes in the off hand in battle mode. Hold to block: it stops every hit from the front, "
               "and wears by however much it stopped.";
    case 261: return "Shoots an arrow each time you use it, as long as you have arrows.";
    case 262: return "Ammunition for the bow. Arrows that miss can be picked up again.";
    case 259: return "Lights fires, TNT and Nether portals.";
    case 325: return "Carries water, lava or milk. Fill it by using it on a source block or a cow.";
    case 346: return "Cast it into water and wait for the bobber to dip, then reel in a fish.";
    case 345: return "Always points to the world's spawn point.";
    case 347: return "Shows the sun and moon, so you know the time even underground.";
    case 1001: return "Holds the spells you learn from spell pages. Tap to cast, hold to pick a spell. "
                      "Lose the book, lose the spells.";
    // ---- armour and food
    case 298: case 302: case 306: case 310: case 314: return "A helmet. Wear it in the top armour slot.";
    case 299: case 303: case 307: case 311: case 315: return "A chestplate. The armour piece that protects you most.";
    case 300: case 304: case 308: case 312: case 316: return "Leggings. Wear them in the third armour slot.";
    case 301: case 305: case 309: case 313: case 317: return "Boots. Wear them in the bottom armour slot.";
    case 1048: case 1049: case 1050: return "Armour for a tamed wolf. Use it on your wolf to put it on.";
    case 297: return "Restores health. Made from wheat.";
    case 282: return "Restores health. You get the bowl back.";
    case 322: return "Restores all of your health at once.";
    case 354: return "Place it down and take a slice at a time. Six slices to a cake.";
    // ---- blocks
    case 58: return "Use it to craft with the full recipe list, including tools, armour and more.";
    case 61: return "Smelts ore into ingots and cooks food. Needs fuel like coal or wood.";
    case 54: return "Stores 27 stacks of items. Put two side by side for a large chest.";
    case 142: return "Stores items like a chest, and keeps them inside when you break it. Take your stuff with you.";
    case 50: return "Lights up the dark and keeps monsters from spawning nearby.";
    case 96: return "A torch that gives off a lapis signal. It turns off while the block it's on is lapis powered.";
    case 98: return "Lights up while it gets a lapis signal: lapis dust, a lapis torch, lapis bricks or a power repeater.";
    case 65: return "Climb up and down walls.";
    case 66: return "Track for minecarts. Rails curve on their own to join up.";
    case 85: return "Keeps animals in and monsters out. Too tall to jump over.";
    case 324: case 330: return "Opens and closes. Iron doors only open with redstone.";
    case 323: return "Write a message on it for anyone who passes.";
    case 47: return "A shelf full of books. Looks good in a library.";
    case 46: return "Explodes a few seconds after it's lit. Stand back.";
    case 84: return "Plays music discs. Mobs nearby can't help dancing.";
    case 25: return "Plays a note when powered. Tap it to change the pitch.";
    case 23: return "Shoots out its items when powered: arrows, snowballs, eggs. Loaded with lapis it fires a magic bolt.";
    case 238: return "Pushes blocks in front of it when powered.";
    case 239: return "Pushes blocks when powered, and pulls them back when it turns off.";
    case 243: return "Moves items from the container above into the one its spout points at. A redstone signal locks it.";
    case 93: return "Repeats a redstone or lapis signal, with a delay. It also turns one kind of signal into the other.";
    case 69: return "A switch: flip it to send a redstone signal.";
    case 77: return "Sends a short redstone pulse when pressed.";
    case 70: case 72: case 154: return "Sends a redstone signal while something stands on it.";
    case 76: return "A torch that powers redstone. It turns off when the block it's on is powered.";
    case 89: return "Glows bright. Found in the Nether.";
    case 232: return "Dyed glowstone. Gives off light in its own colour.";
    case 91: return "A pumpkin with a torch inside. Gives off light.";
    case 110: return "Cuts stone into slabs, stairs, polished blocks and bricks, without losing any.";
    case 1004: return "Holds one small plant. Use a plant on it to pot it.";
    case 1005: return "Holds a full-size plant, even two-block-tall flowers.";
    case 156: return damage == 1 ? "Bricks of compacted ice. Never melts." : "Ice packed hard. It never melts, even next to a torch.";
    case 82: return "Bake clay balls into bricks. Clay blocks break back into balls.";
    case 80: return "A full block of snow.";
    case 24: return "Smooth stone made from sand. Common in deserts.";
    case 41: case 42: case 57: case 22: case 224: case 231:
        return "A compact way to store nine of something. Craft it back to get them out.";
    // ---- misc
    case 280: return "Used to craft torches, arrows, signs, ladders, fences and the handles of tools and weapons.";
    case 281: return "Holds mushroom stew.";
    case 339: return "Used to make books.";
    case 340: return "Used to make bookshelves.";
    case 353: return "Used to bake cakes.";
    case 321: return "Hang it on a wall. You get a random painting that fits the space.";
    case 328: return "Ride it along rails. Mobs it runs into climb aboard.";
    case 342: return "A minecart with a chest on it. Carries items along the rails.";
    case 343: return "Burns coal to push other minecarts along.";
    case 333: return "Lets you travel across water. Breaks if it hits something too fast.";
    case 329: return "Put it on a pig to ride it.";
    case 265: return "Used to craft iron tools, armour, buckets, rails and minecarts.";
    case 266: return "Used to craft gold tools, armour, clocks and golden apples.";
    case 264: return "The toughest material. Used to craft the best tools, weapons and armour.";
    case 263: return "Fuel for furnaces. Also crafted into torches.";
    case 331: return "Lay it on the ground to carry a redstone signal.";
    case 1041: return "Wear it in a charm slot. Wolves only take meat from you while you wear it.";
    case 1045: return "Wear it in a charm slot. You can't be knocked back, and you get 2 extra hearts.";
    case 1039: return "Use it on the ground to place a spawn crystal. That's where you respawn now. Single use.";
    case 1010: case 1011: case 1012: case 1013:
        return "Wear it in a charm slot. Ore you mine has a chance to drop double, or triple at high tiers. "
               "Wears out as it works.";
    }
    // families, by name
    if (id == 351) {
        if (damage == 15) return "Bone meal. Makes crops and saplings grow at once. Also a white dye.";
        if (damage == 4) return "Powers lapis things when laid as dust. Also a blue dye.";
        return "A dye. Colours wool, tinted glass and glowstone, or a sheep.";
    }
    if (id == 35) return damage == 0 ? "Made from string. Can be dyed any colour." : "Dyed wool. A colourful building block.";
    if (id == 233) return "Glass with a colour to it. Light shines through.";
    if (has(name, "Stairs")) return "Lets you walk up a block at a time without jumping.";
    if (has(name, "Slab")) return "Half a block tall. Place two on top of each other to make a full block.";
    if (has(name, "Planks")) return has(name, "Charred") ? "Planks from charred logs. A darker building block."
                                                          : "Basic building block. Used to craft lots of things.";
    if (has(name, "Bricks")) return "A sturdy, tidy building block.";
    if (has(name, "Polished")) return "Smoothed stone. A clean building block.";
    if (has(name, "Stone")) return "A building block.";
    if (id < 256) return "A building block.";
    return "";
}

} // namespace recipes
