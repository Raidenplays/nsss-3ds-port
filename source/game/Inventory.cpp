#include "Inventory.h"
#include "Survival.h"
#include "RecipeBook.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

ItemStack* Inventory::slot(SlotGroup g, int i) {
    switch (g) {
    case SlotGroup::Main:        return (i >= 0 && i < MAIN) ? &main[i] : nullptr;
    case SlotGroup::Armour:      return (i >= 0 && i < ARMOUR) ? &armour[i] : nullptr;
    case SlotGroup::Craft:       return (i >= 0 && i < CRAFT) ? &craft[i] : nullptr;
    case SlotGroup::CraftResult: return &craftResult;
    case SlotGroup::Table:       return (i >= 0 && i < 9) ? &table[i] : nullptr;
    case SlotGroup::TableResult: return &tableResult;
    case SlotGroup::Container:   return (i >= 0 && i < int(container.size())) ? &container[i] : nullptr;
    }
    return nullptr;
}

namespace {
// SlotArmor.isItemValid (armour of the slot's kind); one at a time
bool slotTakes(SlotGroup g, int i, const ItemStack& st) {
    if (st.empty()) return true;
    if (g == SlotGroup::Armour) {
        int type = 3 - i; // armorInventory[3] is the helmet (armorType 0)
        if (st.id >= 298 && st.id <= 317) return (st.id - 298) % 4 == type;
        return st.id == 86 && type == 0;
    }
    return true;
}
bool singleSlot(SlotGroup g) { return g == SlotGroup::Armour; }
} // namespace

void Inventory::clickSlot(SlotGroup g, int i) {
    ItemStack* s = slot(g, i);
    if (!s) return;
    if (g == SlotGroup::CraftResult || g == SlotGroup::TableResult) { // SlotCrafting.onPickupFromSlot
        bool t = g == SlotGroup::TableResult;
        ItemStack& res = t ? tableResult : craftResult;
        ItemStack* grid = t ? table : craft;
        int n = t ? 9 : CRAFT;
        if (res.empty()) return;
        if (cursor.empty()) {
            cursor = res;
        } else if (cursor.id == res.id && cursor.damage == res.damage &&
                   cursor.count + res.count <= survival::maxStack(cursor.id)) {
            cursor.count += res.count;
        } else {
            return;
        }
        for (int k = 0; k < n; k++) {
            ItemStack& c = grid[k];
            if (c.empty()) continue;
            int id = c.id;
            if (--c.count <= 0) c.clear();
            if (id == 326 || id == 327 || id == 335) { // buckets of water / lava / milk leave the bucket
                c.clear();
                c.id = 325;
                c.count = 1;
            }
        }
        updateCraftResult();
        return;
    }
    if (singleSlot(g) && !cursor.empty()) { // only what fits, one at a time
        if (!slotTakes(g, i, cursor)) return;
        if (s->empty()) {
            *s = cursor;
            s->count = 1;
            if (--cursor.count <= 0) cursor.clear();
        } else if (cursor.count == 1) {
            std::swap(*s, cursor);
        }
        return;
    }
    if (g == SlotGroup::Container) containerDirty = true;
    if (g == SlotGroup::Container && containerKind == FURNACE && i == 2) { // SlotFurnace: take only
        if (s->empty()) return;
        if (cursor.empty()) { cursor = *s; s->clear(); }
        else if (cursor.id == s->id && cursor.damage == s->damage && cursor.count + s->count <= survival::maxStack(s->id)) {
            cursor.count += s->count;
            s->clear();
        }
        return;
    }
    if (cursor.empty()) {
        if (s->empty()) return;
        cursor = *s;
        s->clear();
    } else if (s->empty()) {
        *s = cursor;
        cursor.clear();
    } else if (s->id == cursor.id && s->damage == cursor.damage) {
        int room = survival::maxStack(s->id) - s->count;
        if (room < 0) room = 0;
        int move = cursor.count < room ? cursor.count : room;
        s->count += move;
        cursor.count -= move;
        if (cursor.count == 0) cursor.clear();
    } else {
        ItemStack tmp = *s;
        *s = cursor;
        cursor = tmp;
    }
}

void Inventory::rightClickSlot(SlotGroup g, int i) {
    ItemStack* s = slot(g, i);
    if (!s || g == SlotGroup::CraftResult || g == SlotGroup::TableResult) return;
    if (singleSlot(g) && !cursor.empty()) return clickSlot(g, i);
    if (g == SlotGroup::Container) {
        containerDirty = true;
        if (containerKind == FURNACE && i == 2) return clickSlot(g, i);
    }
    if (cursor.empty()) {
        if (s->empty()) return;
        int half = (s->count + 1) / 2;
        cursor = *s;
        cursor.count = half;
        s->count -= half;
        if (s->count == 0) s->clear();
    } else if (s->empty()) {
        *s = cursor;
        s->count = 1;
        if (--cursor.count == 0) cursor.clear();
    } else if (s->id == cursor.id && s->damage == cursor.damage && s->count < survival::maxStack(s->id)) {
        s->count++;
        if (--cursor.count == 0) cursor.clear();
    } else {
        ItemStack tmp = *s;
        *s = cursor;
        cursor = tmp;
    }
}

bool Inventory::addItem(uint16_t id, int count) {
    for (int i = 0; i < STORAGE && count > 0; i++) {
        if (main[i].id == id && main[i].count < MAX_STACK) {
            int move = MAX_STACK - main[i].count;
            if (move > count) move = count;
            main[i].count += move;
            count -= move;
        }
    }
    for (int i = 0; i < STORAGE && count > 0; i++) {
        if (main[i].empty()) {
            int move = count > MAX_STACK ? MAX_STACK : count;
            main[i].id = id;
            main[i].count = move;
            main[i].damage = 0;
            count -= move;
        }
    }
    return count == 0;
}

void Inventory::updateCraftResult() {
    survival::Stack grid[4];
    for (int k = 0; k < CRAFT; k++)
        grid[k] = craft[k].empty() ? survival::Stack{0, 0, 0} : survival::Stack{int16_t(craft[k].id), int8_t(craft[k].count), craft[k].damage};
    survival::Stack out = survival::craft(grid, 2);
    craftResult.clear();
    if (out.id > 0) {
        craftResult.id = uint16_t(out.id);
        craftResult.count = out.count;
        craftResult.damage = out.damage;
    }
    tableResult.clear();
    if (tableOpen) {
        survival::Stack g9[9];
        for (int k = 0; k < 9; k++)
            g9[k] = table[k].empty() ? survival::Stack{0, 0, 0} : survival::Stack{int16_t(table[k].id), int8_t(table[k].count), table[k].damage};
        survival::Stack o = survival::craft(g9, 3);
        if (o.id > 0) {
            tableResult.id = uint16_t(o.id);
            tableResult.count = o.count;
            tableResult.damage = o.damage;
        }
    }
}

void Inventory::closeTable() {
    for (ItemStack& t : table) {
        if (t.empty()) continue;
        ItemStack left = t;
        addStack(left);
        if (left.count > 0) spill.push_back(left);
        t.clear();
    }
    tableResult.clear();
    tableOpen = false;
}

bool Inventory::addStack(ItemStack& s) {
    if (s.empty()) return true;
    int max = survival::maxStack(s.id);
    bool damaged = survival::maxDamage(s.id) > 0 && s.damage > 0;
    if (!damaged) // storePartialItemStack
        for (int i = 0; i < STORAGE && s.count > 0; i++) {
            ItemStack& m = main[i];
            if (m.id == s.id && m.damage == s.damage && max > 1 && m.count < max) {
                int move = std::min(max - m.count, s.count);
                m.count += move;
                s.count -= move;
            }
        }
    while (s.count > 0) {
        int slot = -1;
        for (int i = 0; i < STORAGE; i++)
            if (main[i].empty()) { slot = i; break; }
        if (slot < 0) return false;
        main[slot] = s;
        int move = std::min(s.count, max);
        main[slot].count = move;
        s.count -= move;
    }
    s.count = 0;
    return true;
}

bool Inventory::consumeHeld() {
    ItemStack& h = held();
    if (h.empty()) return false;
    if (--h.count == 0) h.clear();
    return true;
}

void Inventory::returnCursor() {
    if (cursor.empty()) return;
    ItemStack c = cursor;
    addStack(c);
    if (c.count > 0) spill.push_back(c); // GuiContainer: whatever won't fit is thrown out
    cursor.clear();
}

void Inventory::scrollHotbar(int dir) {
    currentItem = ((currentItem - (dir > 0 ? 1 : dir < 0 ? -1 : 0)) % HOTBAR + HOTBAR) % HOTBAR;
}

void Inventory::setCurrentItem(int id, int count) {
    int j = -1;
    for (int k = 0; k < MAIN; ++k)
        if (main[k].id == id && main[k].count > 0) { j = k; break; }
    if (id > 0) {
        main[currentItem].clear();
        main[currentItem].id = uint16_t(id);
        main[currentItem].count = count;
    }
    if (j >= 0 && j < 9) std::swap(main[currentItem], main[j]);
}

static void readStack(const nbt::Tag& c, ItemStack& s) {
    s.id = uint16_t(c.getShort("id"));
    s.count = int8_t(c.getByte("Count"));
    s.damage = c.getShort("Damage");
}

void Inventory::readFromNBT(const nbt::Tag& list) {
    for (auto& s : main) s.clear();
    for (auto& s : armour) s.clear();
    for (auto& it : list.items) {
        if (!it || it->type != nbt::Compound) continue;
        int slot = uint8_t(it->getByte("Slot"));
        ItemStack s;
        readStack(*it, s);
        if (s.id == 0) continue; // Java: getItem() == null
        if (slot < MAIN) main[slot] = s;
        if (slot >= 100 && slot < 100 + ARMOUR) armour[slot - 100] = s;
    }
}

static void writeStack(nbt::Tag& list, int slot, const ItemStack& s) {
    nbt::TagPtr c = nbt::makeCompound();
    c->setByte("Slot", int8_t(slot));
    c->setShort("id", int16_t(s.id));
    c->setByte("Count", int8_t(s.count));
    c->setShort("Damage", s.damage);
    list.items.push_back(std::move(c));
}

nbt::TagPtr Inventory::writeToNBT() const {
    nbt::TagPtr list = nbt::makeList(nbt::Compound);
    // Java writes a null slot as nothing; an emptied stack (count 0) is still a stack there
    for (int i = 0; i < MAIN; i++) if (main[i].id) writeStack(*list, i, main[i]);
    for (int i = 0; i < ARMOUR; i++) if (armour[i].id) writeStack(*list, i + 100, armour[i]);
    for (int i = 0; i < CRAFT; i++) if (craft[i].id) writeStack(*list, i + 80, craft[i]);
    return list;
}
