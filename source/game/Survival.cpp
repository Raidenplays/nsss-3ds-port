#include "Survival.h"
#include "world/BlockProps.h"

namespace survival {

const ItemData* item(int id) {
    int lo = 0, hi = ITEM_DATA_COUNT - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (ITEM_DATA[m].id == id) return &ITEM_DATA[m];
        if (ITEM_DATA[m].id < id) lo = m + 1;
        else hi = m - 1;
    }
    return nullptr;
}

int maxStack(int id) {
    const ItemData* d = item(id);
    return d ? d->maxStack : 64;
}

int maxDamage(int id) {
    const ItemData* d = item(id);
    return d ? d->maxDamage : 0;
}

int burnTime(int id, int) {
    const ItemData* d = item(id);
    return d ? d->burnTime : 0;
}

float toolSpeed(int it, int block) {
    if (it <= 0) return 1.0f;
    for (int i = 0; i < TOOL_ENTRY_COUNT; ++i)
        if (TOOL_ENTRIES[i].item == it && TOOL_ENTRIES[i].block == block) return TOOL_ENTRIES[i].speed;
    return 1.0f;
}

bool canHarvest(int it, int block) {
    Mat m = BP[block].mat;
    if (m != Mat::rock && m != Mat::iron && m != Mat::builtSnow && m != Mat::snow) return true;
    if (it <= 0) return false;
    for (int i = 0; i < TOOL_ENTRY_COUNT; ++i)
        if (TOOL_ENTRIES[i].item == it && TOOL_ENTRIES[i].block == block) return TOOL_ENTRIES[i].harvests;
    return false;
}

float blockStrength(int block, int held, bool inWater, bool onGround) {
    float h = HARDNESS[block];
    if (h < 0.0f) return 0.0f;
    if (!canHarvest(held, block)) return 1.0f / h / 100.0f;
    float s = toolSpeed(held, block);
    if (inWater) s /= 5.0f;
    if (!onGround) s /= 5.0f;
    return s / h / 30.0f;
}

namespace {
const Stack* at(const Stack* grid, int w, int x, int y) { // InventoryCrafting.getStackInPos
    if (x < 0 || x >= w || y < 0 || y >= w) return nullptr;
    const Stack* s = &grid[x + y * w];
    return s->id == 0 ? nullptr : s;
}

bool shapedFits(const Shaped& s, const Stack* grid, int w, int ox, int oy, bool mirror) { // ShapedRecipes.fits
    for (int x = 0; x < 3; ++x)
        for (int y = 0; y < 3; ++y) {
            int rx = x - ox, ry = y - oy;
            const Ingredient* want = nullptr;
            if (rx >= 0 && ry >= 0 && rx < s.w && ry < s.h) {
                const Ingredient& in = mirror ? s.in[s.w - rx - 1 + ry * s.w] : s.in[rx + ry * s.w];
                if (in.id >= 0) want = &in;
            }
            const Stack* got = at(grid, w, x, y);
            if (!got && !want) continue;
            if (!got || !want) return false;
            if (want->id != got->id) return false;
            if (want->damage != -1 && want->damage != got->damage) return false;
        }
    return true;
}

bool shapelessMatches(const Shapeless& s, const Stack* grid, int w) { // ShapelessRecipes.matches
    bool used[9] = {};
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x) {
            const Stack* got = at(grid, w, x, y);
            if (!got) continue;
            bool found = false;
            for (int i = 0; i < s.n; ++i)
                if (!used[i] && got->id == s.in[i].id && (s.in[i].damage == -1 || got->damage == s.in[i].damage)) {
                    used[i] = true;
                    found = true;
                    break;
                }
            if (!found) return false;
        }
    for (int i = 0; i < s.n; ++i)
        if (!used[i]) return false;
    return true;
}
} // namespace

bool accepts(const Ingredient& want, int id, int damage) {
    if (want.id < 0 || want.id != id) return false;
    return want.damage == -1 || want.damage == damage;
}

Stack craft(const Stack* grid, int w) { // CraftingManager.findMatchingRecipe
    for (int r = 0; r < RECIPE_COUNT; ++r) {
        int k = RECIPE_ORDER[r];
        if (k >= 0) {
            const Shaped& s = SHAPED[k];
            for (int ox = 0; ox <= 3 - s.w; ++ox)
                for (int oy = 0; oy <= 3 - s.h; ++oy)
                    if (shapedFits(s, grid, w, ox, oy, true) || shapedFits(s, grid, w, ox, oy, false)) return s.out;
        } else if (shapelessMatches(SHAPELESS[~k], grid, w)) {
            return SHAPELESS[~k].out;
        }
    }
    return Stack{0, 0, 0};
}

Stack smelt(int id, int) {
    for (int i = 0; i < SMELT_COUNT; ++i)
        if (SMELTING[i].in == id) return SMELTING[i].out;
    return Stack{0, 0, 0};
}
} // namespace survival
