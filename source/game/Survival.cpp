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
bool anyCobble(int id) { return id == 4 || id == 101 || id == 178; }
bool anyStone(int id) { return id == 1 || id == 102 || (id >= 171 && id <= 177); }
bool usesAnyCobble(int out) {
    // pickaxe/shovel/axe/hoe/sword/dagger/spear/war axe (stone), furnace, lever, dispenser
    static const int OUT[] = {274, 273, 275, 291, 272, 1016, 1021, 1026, 61, 69, 23};
    for (int o : OUT)
        if (o == out) return true;
    return false;
}
const Stack* at(const Stack* grid, int w, int x, int y) {
    if (x < 0 || x >= w || y < 0 || y >= w) return nullptr;
    const Stack* s = &grid[x + y * w];
    return s->id == 0 ? nullptr : s;
}
} // namespace

bool accepts(const Ingredient& want, int id, int damage, int out) {
    if (want.id < 0) return false;
    if (want.id != id)
        return (want.id == 4 && anyCobble(id) && usesAnyCobble(out)) || (want.id == 1 && anyStone(id) && out == 1031);
    return want.damage == -1 || want.damage == damage;
}

bool locked(int out, bool (*has)(const char*)) {
    for (int i = 0; i < LOCK_COUNT; ++i)
        if (LOCKS[i].out == out) return !(has && has(LOCKS[i].achievement));
    return false;
}

Stack craft(const Stack* grid, int w, bool (*has)(const char*)) {
    auto locked = [&](int out) {
        for (int i = 0; i < LOCK_COUNT; ++i)
            if (LOCKS[i].out == out) return !(has && has(LOCKS[i].achievement));
        return false;
    };
    // CraftingManager keeps shaped and shapeless in one list; the dump lists shaped first, which
    // only matters if two recipes match the same grid (none do)
    for (int r = 0; r < SHAPED_COUNT; ++r) {
        const Shaped& s = SHAPED[r];
        for (int ox = 0; ox <= 3 - s.w; ++ox)
            for (int oy = 0; oy <= 3 - s.h; ++oy)
                for (int mirror = 1; mirror >= 0; --mirror) {
                    bool ok = true;
                    for (int x = 0; x < 3 && ok; ++x)
                        for (int y = 0; y < 3 && ok; ++y) {
                            int rx = x - ox, ry = y - oy;
                            const Ingredient* want = nullptr;
                            if (rx >= 0 && ry >= 0 && rx < s.w && ry < s.h) {
                                const Ingredient& in = mirror ? s.in[s.w - rx - 1 + ry * s.w] : s.in[rx + ry * s.w];
                                if (in.id >= 0) want = &in;
                            }
                            const Stack* got = at(grid, w, x, y);
                            if (!got && !want) continue;
                            if (!got || !want) { ok = false; break; }
                            if (want->id != got->id) {
                                // AxeolonRecipeSubs: any cobblestone for stone tools etc., any stone for the mace
                                bool sub = (want->id == 4 && anyCobble(got->id) && usesAnyCobble(s.out.id)) ||
                                           (want->id == 1 && anyStone(got->id) && s.out.id == 1031);
                                if (!sub) ok = false;
                            } else if (want->damage != -1 && want->damage != got->damage) {
                                ok = false;
                            }
                        }
                    if (ok) return locked(s.out.id) ? Stack{0, 0, 0} : s.out;
                }
    }
    for (int r = 0; r < SHAPELESS_COUNT; ++r) {
        const Shapeless& s = SHAPELESS[r];
        bool used[9] = {};
        bool ok = true;
        for (int y = 0; y < 3 && ok; ++y)
            for (int x = 0; x < 3 && ok; ++x) {
                const Stack* got = at(grid, w, x, y);
                if (!got) continue;
                bool found = false;
                for (int i = 0; i < s.n; ++i)
                    if (!used[i] && got->id == s.in[i].id && (s.in[i].damage == -1 || got->damage == s.in[i].damage)) {
                        used[i] = true;
                        found = true;
                        break;
                    }
                if (!found) ok = false;
            }
        for (int i = 0; i < s.n && ok; ++i)
            if (!used[i]) ok = false;
        if (ok) return locked(s.out.id) ? Stack{0, 0, 0} : s.out;
    }
    return Stack{0, 0, 0};
}

Stack smelt(int id, int damage) {
    if (id == 178) return Stack{int16_t(171 + (damage >= 0 && damage < 7 ? damage : 0)), 1, 0};
    if (id >= 217 && id <= 223) {
        int k = id == 223 ? 3 : id - 217;
        switch (k) {
        case 1: return Stack{265, 1, 0};
        case 2: return Stack{266, 1, 0};
        case 0: return Stack{263, 1, 0};
        case 4: return Stack{264, 1, 0};
        case 5: return Stack{351, 1, 4};
        default: return Stack{0, 0, 0};
        }
    }
    if (id == 6) return Stack{119, 1, int16_t(damage & 7)};
    for (int i = 0; i < SMELT_COUNT; ++i)
        if (SMELTING[i].in == id) return SMELTING[i].out;
    return Stack{0, 0, 0};
}
} // namespace survival
