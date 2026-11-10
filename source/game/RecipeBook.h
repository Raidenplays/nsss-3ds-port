#pragma once
// MATTIS ("Minecraft Advanced Touch Technology Interface System"): Pocket Edition's crafting screen
// before 0.9.0, built on NSSS's own CraftingManager recipes. Every recipe the player can use
// is listed by what it makes, in four groups like the Pocket tabs. One tap crafts: the
// ingredients come straight out of the inventory, with no grid to fill.
//
// The 2x2 recipes are always there; a crafting table adds the 3x3 ones.
#include <cstdint>
#include <string>
#include <vector>
#include "game/Inventory.h"
#include "game/Survival.h"

namespace recipes {

enum Category { BLOCKS, TOOLS, ARMOUR_FOOD, MISC, CATEGORIES };

struct Need {          // one kind of ingredient: how many the recipe takes and how many you have
    survival::Ingredient what;
    int count = 0;
    int have = 0;
};

struct Entry {
    survival::Stack out;
    Category category;
    int recipe;              // >= 0: SHAPED[recipe]; < 0: SHAPELESS[-recipe - 1]
    std::vector<Need> needs; // grouped by ingredient
    bool craftable = false;  // enough of everything in the inventory
};

// What the player could make right now with a 2x2 grid (table = false) or a crafting table,
// craftable recipes first. One entry per thing made (the first recipe the inventory can pay for).
std::vector<Entry> list(const Inventory& inv, bool table, Category c);

// Which group a result goes in
Category categoryOf(int id, int damage);

// Crafts `e` once if the inventory still has everything: takes the ingredients (backpack first,
// buckets leave the bucket), puts the result in the inventory (what doesn't fit goes to inv.spill).
bool craft(Inventory& inv, const Entry& e);

// How many times in a row `e` could be crafted now (stops at what one stack of the result holds)
int timesCraftable(const Inventory& inv, const Entry& e);

} // namespace recipes
