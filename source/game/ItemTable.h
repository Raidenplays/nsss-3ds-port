#pragma once
// Every item's GUI icon, as Axeolon's RenderItem draws it (dumped by tools/java/DumpItems).
#include <cstdint>

enum ItemFlags { IF_BLOCK3D = 1, IF_TERRAIN2 = 2, IF_ITEM = 4 };

struct ItemInfo {
    uint16_t id, damage;
    int16_t icon;        // tile in terrain.png / terrain2.png (blocks) or gui/items.png (items)
    uint32_t colour;     // 0xRRGGBB tint
    int16_t overlay;     // second icon drawn on top (-1 none)
    uint32_t overlayColour;
    uint8_t flags;
    const char* name;
};
extern const ItemInfo ITEMS[];   // sorted by id, damage
extern const int ITEM_COUNT;

// The entry for this stack (exact damage, else damage 0). Null for unknown ids.
inline const ItemInfo* itemInfo(int id, int damage) {
    int lo = 0, hi = ITEM_COUNT - 1;
    const ItemInfo* first = nullptr;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const ItemInfo& e = ITEMS[mid];
        if (e.id < id || (e.id == id && e.damage < damage)) lo = mid + 1;
        else hi = mid - 1;
    }
    for (int i = lo - 16 < 0 ? 0 : lo - 16; i < ITEM_COUNT && ITEMS[i].id <= id; i++) {
        if (ITEMS[i].id != id) continue;
        if (ITEMS[i].damage == damage) return &ITEMS[i];
        if (ITEMS[i].damage == 0) first = &ITEMS[i];
    }
    return first;
}
