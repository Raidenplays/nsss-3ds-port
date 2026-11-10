// CraftingInventoryCreativeCB.buildItemList / rank / section / buildRows, line for line
#include "game/CreativeMenu.h"

#include <algorithm>

#include "game/ItemTable.h"
#include "game/Spells.h"
#include "world/BlockTable.h"

namespace creative {
namespace {

const char* const SECTIONS[] = {"Building Blocks", "Wood", "Slabs & Stairs", "Wool", "Nature", "Plants & Flowers", "Nether",
                                "Utility", "Redstone, Lapis & Transport", "Tools & Weapons", "Armour", "Food", "Materials",
                                "Magic", "Spawn Eggs"};
constexpr int SECTION_COUNT = 15;

constexpr int PLANK_TYPES = 5, WOOD_TYPES = 10, STONE_TYPES = 7, ORE_BASES = 8, STEP_TYPES = 7;
constexpr int SPAWN_EGGS[] = {50, 51, 52, 54, 55, 56, 57, 90, 91, 92, 93, 94, 200, 210, 211, 212, 213};
constexpr int GOLDENROD = 2, GLOW_MANABLOOM = 0, GLOW_WISP_LILY = 1;
constexpr int ASHCAP = 0, EMBER_BUD = 1, DUSKPETAL_CLOSED = 2, DROGBLOOM = 4;
constexpr int TALLY_STICK = 1000; // a test item, debug builds only

bool hiddenBlock(int id) {
    switch (id) {
    case 145: case 146: case 147: case 148: case 149: case 150: case 151: case 152: case 153:
    case 158: case 160: case 162:
    case 181: case 183: case 185: case 187:
    case 95: case 234: case 235: case 236: case 237:
    case 240:
    case 94: case 97: case 99: case 223:
    case 230:
    case 133:
    case 143:
    case 134:
    case 137: case 138: case 139: case 140:
    case 7: case 8: case 9: case 10: case 11: case 43: case 120: case 129: case 111: case 31: case 51: case 52:
    case 55: case 59: case 60: case 62: case 63: case 64: case 68: case 71: case 74: case 75: case 83: case 90: case 92:
        return true;
    default:
        return false;
    }
}

ItemStack stack(int id, int meta) {
    ItemStack s;
    s.id = uint16_t(id);
    s.count = 1;
    s.damage = int16_t(meta);
    return s;
}

std::vector<ItemStack> buildItemList() {
    std::vector<ItemStack> all;
    auto add = [&](int id, int meta) { all.push_back(stack(id, meta)); };
    auto range = [&](int id, int n) { for (int m = 0; m < n; ++m) add(id, m); };
    for (int id = 1; id < 256; ++id) {
        if (!BLOCKS[id].name || hiddenBlock(id)) continue;
        if (id == 35) range(id, 16);
        else if (id == 17 || id == 5 || id == 6 || id == 18 || id == 117 || id == 118 || id == 119) range(id, PLANK_TYPES);
        else if (id == 116) { add(id, ASHCAP); add(id, EMBER_BUD); add(id, DUSKPETAL_CLOSED); add(id, DROGBLOOM); }
        else if (id == 113) range(id, 8);
        else if (id == 135) { add(id, GLOW_MANABLOOM); add(id, GLOW_WISP_LILY); }
        else if (id == 130) range(id, 2);
        else if (id == 121) range(id, 8);
        else if (id == 85 || id == 58 || id == 54) range(id, WOOD_TYPES);
        else if (id == 178 || id == 179 || id == 180 || id == 182 || id == 184 || id == 186 || id == 188) range(id, STONE_TYPES);
        else if (id >= 217 && id <= 222) range(id, ORE_BASES);
        else if (id == 232 || id == 233) range(id, 16);
        else if (id == 229) range(id, 2);
        else if (id == 159 || id == 161 || id == 163 || id == 156) range(id, 2);
        else if (id == 72 || id == 154) range(id, 5);
        else if (id == 44) range(id, STEP_TYPES);
        else add(id, 0);
    }
    int last = -1;
    for (int i = 0; i < ITEM_COUNT; ++i) {
        int id = ITEMS[i].id;
        if (id < 256 || id == last) continue;
        last = id;
        if (id == 351) range(id, 16);
        else if (id == TALLY_STICK) continue;
        else if (id == 1003) { for (int e : SPAWN_EGGS) add(id, e); }
        else if (id == spells::PAGE) range(id, spells::COUNT);
        else if (id == 324 || id == 323 || id == 333) range(id, WOOD_TYPES);
        else if (id == 263) range(id, 2);
        else add(id, 0);
    }
    return all;
}

constexpr int STAIRS_ORDER[] = {67, 104, 108, 107, 105, 106, 112, 53, 122, 123, 124, 131, 125, 126, 127, 128, 132};

double rank(const ItemStack& s) {
    int id = s.id, meta = s.damage;
    double base = double(id) * 100.0 + double(meta);
    switch (id) {
    case 100: return 100.1;
    case 48: return 400.1;
    case 101: return 400.2;
    case 109: return 400.3;
    case 102: return 400.4;
    case 103: return 400.5;
    case 24: return 400.6;
    case 17: return 500.0 + meta * 0.01;
    case 5: return 500.1 + meta * 0.01;
    case 117: return 500.2 + meta * 0.01;
    case 118: return 500.3 + meta * 0.01;
    case 6: return 600.0 + meta * 0.001;
    case 119: return 600.1 + meta * 0.001;
    case 18: return 600.2 + meta * 0.001;
    case 113: return meta == GOLDENROD ? 600.56 : (meta < 2 ? 600.3 + meta * 0.001 : 600.57 + meta * 0.001);
    case 135: return 600.565 + meta * 0.001;
    case 141: return 600.37;
    case 136: return 600.569;
    case 32: return 600.35;
    case 37: return 600.4;
    case 38: return 600.41;
    case 114: return 600.5;
    case 115: return 600.51;
    case 116: return 600.52 + meta * 0.001;
    case 39: return 600.6;
    case 40: return 600.61;
    case 44: return 4400.0 + meta * 0.01;
    case 121: return 4400.5 + meta * 0.001;
    case 130: return 4400.6 + meta * 0.001;
    case 110: return 5810.0;
    case 171: case 172: case 173: case 174: case 175: case 176: case 177: return 400.55 + (id - 171) * 0.004;
    case 178: return 400.55 + meta * 0.004 + 0.001;
    case 179: return 400.55 + meta * 0.004 + 0.002;
    case 180: return 400.55 + meta * 0.004 + 0.003;
    case 233: return 2000.1 + meta * 0.001;
    case 224: return 2199.1;
    case 226: return 2199.2;
    case 22: return 2199.3;
    case 225: return 2199.4;
    case 231: return 4200.6;
    case 232: return 8900.1 + meta * 0.001;
    case 217: return 1600.1 + meta * 0.001;
    case 218: return 1500.1 + meta * 0.001;
    case 219: return 1400.1 + meta * 0.001;
    case 220: return 7300.1 + meta * 0.001;
    case 221: return 5600.1 + meta * 0.001;
    case 222: return 2100.1 + meta * 0.001;
    case 170: return 8600.5;
    case 182: case 184: case 186: case 188: return 4400.62 + meta * 0.0004 + (id - 182) / 2 * 0.0001;
    case 159: case 161: case 163: return 4400.66 + (id - 159) * 0.001 + meta * 0.0001;
    case 229: return 4400.67 + meta * 0.001;
    case 164: case 165: case 166: case 167: case 168: case 169: return 4400.725 + (id - 164) * 0.0001;
    case 227: case 228: return 4400.73 + (id - 227) * 0.0001;
    case 154: return 7210.0 + meta * 0.01;
    case 243: return 2300.5;
    case 1046: return 26500.5;
    case 1047: return 26600.5;
    case 1048: case 1049: case 1050: return 31700.5 + (id - 1048) * 0.01;
    case 1001: return 34000.5;
    case 1002: return 34000.6 + meta * 0.001;
    case 1003: return 1.0e9 + meta;
    }
    for (int i = 0; i < int(sizeof STAIRS_ORDER / sizeof STAIRS_ORDER[0]); ++i)
        if (STAIRS_ORDER[i] == id) return 4400.7 + i * 0.001;
    if (id >= 189 && id <= 216) return 4400.72 + (id - 189) * 0.0001;
    return base;
}

int section(const ItemStack& s) {
    int id = s.id;
    switch (id) {
    case 1: case 100: case 4: case 48: case 101: case 109: case 102: case 103: case 24:
    case 45: case 20: case 49: case 41: case 42: case 57: case 19:
        return 0;
    case 17: case 5: case 117: case 118:
        return 1;
    case 44: case 121: case 130: case 67: case 53: case 104: case 105: case 106: case 107: case 108: case 112:
    case 122: case 123: case 124: case 125: case 126: case 127: case 128: case 131: case 132:
        return 2;
    case 35:
        return 3;
    case 2: case 3: case 12: case 13: case 82: case 79: case 78: case 80: case 81: case 86:
    case 14: case 15: case 16: case 21: case 56: case 73: case 144:
        return 4;
    case 6: case 119: case 18: case 113: case 135: case 136: case 141: case 32: case 37: case 38: case 114: case 115: case 116: case 39: case 40:
        return 5;
    case 87: case 88: case 89:
        return 6;
    case 58: case 110: case 61: case 54: case 142: case 50: case 91: case 65: case 47: case 84: case 25: case 85:
    case 323: case 324: case 330: case 321: case 325: case 326: case 327: case 2256: case 2257:
        return 7;
    case 23: case 46: case 66: case 69: case 70: case 72: case 76: case 77:
    case 331: case 328: case 342: case 343: case 333: case 329:
        return 8;
    case 256: case 257: case 258: case 259: case 261: case 262: case 267: case 268: case 269: case 270: case 271:
    case 272: case 273: case 274: case 275: case 276: case 277: case 278: case 279: case 283: case 284: case 285: case 286:
    case 290: case 291: case 292: case 293: case 294: case 345: case 346: case 347:
        return 9;
    case 260: case 282: case 297: case 319: case 320: case 322: case 349: case 350: case 354: case 335:
        return 11;
    case 1004: case 1005: case 1006:
        return 7;
    case 1039: case 1041:
    case 1000: case 1001: case 1002: case 1007: case 1008: case 1009: case 1010: case 1011: case 1012: case 1013: case 1014:
        return 13;
    case 1003:
        return 14;
    }
    if (id == 1042) return 9;
    if ((id >= 182 && id <= 216 && id != 181) || id == 159 || id == 161 || id == 163 || (id >= 164 && id <= 169) || id == 227 ||
        id == 228 || id == 229)
        return 2;
    if ((id >= 217 && id <= 222) || id == 170) return 4;
    if (id == 232) return 6;
    if (id == 93 || id == 96 || id == 98 || id == 154 || id == 238 || id == 239 || id == 243 || id == 22 || id == 224 || id == 225 ||
        id == 226)
        return 8;
    if (id == 1048 || id == 1049 || id == 1050) return 10;
    if (id == 1045) return 13;
    if (id >= 1015 && id <= 1038) return 9;
    if (id >= 298 && id <= 317) return 10;
    return id < 256 ? 0 : 12;
}

} // namespace

const std::vector<Row>& rows() {
    static std::vector<Row> built;
    if (!built.empty()) return built;
    std::vector<ItemStack> all = buildItemList();
    std::stable_sort(all.begin(), all.end(), [](const ItemStack& a, const ItemStack& b) { return rank(a) < rank(b); });
    std::vector<ItemStack> sections[SECTION_COUNT];
    for (const ItemStack& s : all) sections[section(s)].push_back(s);
    for (int i = 0; i < SECTION_COUNT; ++i) {
        if (sections[i].empty()) continue;
        Row title;
        title.title = SECTIONS[i];
        built.push_back(title);
        for (size_t j = 0; j < sections[i].size(); ++j) {
            if (j % 9 == 0) built.emplace_back();
            built.back().items[j % 9] = sections[i][j];
        }
    }
    return built;
}

} // namespace creative
