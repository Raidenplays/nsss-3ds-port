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
    case SlotGroup::Charm:       return (i >= 0 && i < CHARMS) ? &charms[i] : nullptr;
    case SlotGroup::Craft:       return (i >= 0 && i < CRAFT) ? &craft[i] : nullptr;
    case SlotGroup::CraftResult: return &craftResult;
    case SlotGroup::Table:       return (i >= 0 && i < 9) ? &table[i] : nullptr;
    case SlotGroup::TableResult: return &tableResult;
    case SlotGroup::Container:   return (i >= 0 && i < int(container.size())) ? &container[i] : nullptr;
    case SlotGroup::CutInput:    return &cutInput;
    case SlotGroup::CutResult:   return &cutResult;
    }
    return nullptr;
}

namespace {
// SlotArmor.isItemValid (armour of the slot's kind, or a pumpkin on the head) and
// SlotCharm.isItemValid (only charms); both hold one at a time
bool slotTakes(SlotGroup g, int i, const ItemStack& st) {
    if (st.empty()) return true;
    if (g == SlotGroup::Armour) {
        int type = 3 - i; // armorInventory[3] is the helmet (armorType 0)
        if (st.id >= 298 && st.id <= 317) return (st.id - 298) % 4 == type;
        return st.id == 86 && type == 0;
    }
    if (g == SlotGroup::Charm) {
        switch (st.id) {
        case 1007: case 1008: case 1009: case 1010: case 1011: case 1012: case 1013: case 1014: case 1041: case 1045:
            return true;
        default: return false;
        }
    }
    return true;
}
bool singleSlot(SlotGroup g) { return g == SlotGroup::Armour || g == SlotGroup::Charm; }
} // namespace

void Inventory::clickSlot(SlotGroup g, int i) {
    ItemStack* s = slot(g, i);
    if (!s) return;
    if (g == SlotGroup::CutResult) { // SlotStonecutterResult: always the whole result, never put anything in
        if (cutResult.empty()) return;
        if (cursor.empty()) cursor = cutResult;
        else if (cursor.id == cutResult.id && cursor.damage == cutResult.damage && !cursor.tag &&
                 cursor.count + cutResult.count <= survival::maxStack(cursor.id))
            cursor.count += cutResult.count;
        else return;
        // onResultTaken: one input used up, the result refilled while there's more
        if (--cutInput.count <= 0) cutInput.clear();
        refreshCuts();
        if (!cutInput.empty() && cutSelected >= 0 && cutSelected < int(cutChoices.size())) cutResult = cutChoices[cutSelected];
        else cutResult.clear();
        return;
    }
    if (g == SlotGroup::CraftResult || g == SlotGroup::TableResult) { // SlotCrafting.onPickupFromSlot
        bool t = g == SlotGroup::TableResult;
        ItemStack& res = t ? tableResult : craftResult;
        ItemStack* grid = t ? table : craft;
        int n = t ? 9 : CRAFT;
        if (res.empty()) return;
        if (cursor.empty()) {
            cursor = res;
        } else if (cursor.id == res.id && cursor.damage == res.damage && !cursor.tag &&
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
        else if (cursor.id == s->id && cursor.damage == s->damage && !cursor.tag && cursor.count + s->count <= survival::maxStack(s->id)) {
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
    } else if (s->id == cursor.id && s->damage == cursor.damage && !s->tag && !cursor.tag) {
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
    if (g == SlotGroup::CutResult) return clickSlot(g, i);
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
    } else if (s->id == cursor.id && s->damage == cursor.damage && !s->tag && !cursor.tag && s->count < survival::maxStack(s->id)) {
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
        if (main[i].id == id && !main[i].tag && main[i].count < MAX_STACK) {
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

namespace {
const std::string* currentAchievements = nullptr;
bool hasAchievement(const char* a) {
    if (!currentAchievements) return false;
    const std::string& s = *currentAchievements;
    size_t n = std::strlen(a), p = 0;
    while (p <= s.size()) {
        size_t e = s.find(',', p);
        if (e == std::string::npos) e = s.size();
        if (e - p == n && s.compare(p, n, a) == 0) return true;
        p = e + 1;
    }
    return false;
}
} // namespace

void Inventory::updateCraftResult() {
    if (cutterOpen) refreshCuts();
    survival::Stack grid[4];
    for (int k = 0; k < CRAFT; k++)
        grid[k] = craft[k].empty() ? survival::Stack{0, 0, 0} : survival::Stack{int16_t(craft[k].id), int8_t(craft[k].count), craft[k].damage};
    currentAchievements = &achievements;
    survival::Stack out = survival::craft(grid, 2, hasAchievement);
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
        survival::Stack o = survival::craft(g9, 3, hasAchievement);
        if (o.id > 0) {
            tableResult.id = uint16_t(o.id);
            tableResult.count = o.count;
            tableResult.damage = o.damage;
        }
    }
}

bool Inventory::hasSeen(int id) const {
    char want[12];
    std::snprintf(want, sizeof want, "%d", id);
    size_t n = std::strlen(want), p = 0;
    while (p <= seenItems.size()) {
        size_t e = seenItems.find(',', p);
        if (e == std::string::npos) e = seenItems.size();
        if (e - p == n && seenItems.compare(p, n, want) == 0) return true;
        p = e + 1;
    }
    return false;
}

void Inventory::markSeen() {
    for (const ItemStack& st : main)
        if (!st.empty() && !hasSeen(st.id)) {
            if (!seenItems.empty()) seenItems += ',';
            seenItems += std::to_string(st.id);
        }
}

void Inventory::refreshCuts() {
    static int lastId = -2, lastDamage = -2;
    int id = cutInput.empty() ? -1 : cutInput.id, dmg = cutInput.empty() ? -1 : cutInput.damage;
    if (id == lastId && dmg == lastDamage && !(cutChoices.empty() && id > 0)) return;
    lastId = id;
    lastDamage = dmg;
    cutChoices.clear();
    if (id > 0)
        for (const survival::Stack& r : recipes::stonecut(id, dmg)) {
            ItemStack st;
            st.id = uint16_t(r.id);
            st.count = r.count;
            st.damage = r.damage;
            cutChoices.push_back(st);
        }
    cutSelected = -1;
    cutResult.clear();
}

void Inventory::selectCut(int i) {
    if (i < 0 || i >= int(cutChoices.size()) || cutInput.empty()) return;
    cutSelected = i;
    cutResult = cutChoices[i];
}

void Inventory::closeStonecutter() {
    if (!cutInput.empty()) {
        ItemStack left = cutInput;
        addStack(left);
        if (left.count > 0) spill.push_back(left);
    }
    cutInput.clear();
    cutResult.clear();
    cutChoices.clear();
    cutSelected = -1;
    cutterOpen = false;
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
    if (!damaged && !s.tag) // storePartialItemStack
        for (int i = 0; i < STORAGE && s.count > 0; i++) {
            ItemStack& m = main[i];
            if (m.id == s.id && !m.tag && m.damage == s.damage && max > 1 && m.count < max) {
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
    if (battleMode) {
        selectBattleSlot((battleSlot + (dir > 0 ? 2 : 1)) % 3);
        return;
    }
    currentItem = ((currentItem - (dir > 0 ? 1 : dir < 0 ? -1 : 0)) % HOTBAR + HOTBAR) % HOTBAR;
}

void Inventory::setBattleMode(bool on) {
    if (on && !battleMode) {
        hotbarSlot = currentItem;
        currentItem = BATTLE_START + battleSlot * 2;
    } else if (!on && battleMode) {
        currentItem = hotbarSlot;
    }
    battleMode = on;
}

void Inventory::selectBattleSlot(int pair) {
    battleSlot = pair < 0 ? 0 : pair > 2 ? 2 : pair;
    if (battleMode) currentItem = BATTLE_START + battleSlot * 2;
}

ItemStack* Inventory::offhand() {
    if (!battleMode || currentItem + 1 >= MAIN) return nullptr;
    return &main[currentItem + 1];
}

static void readStack(const nbt::Tag& c, ItemStack& s) {
    s.id = uint16_t(c.getShort("id"));
    s.count = int8_t(c.getByte("Count"));
    s.damage = c.getShort("Damage");
    s.tag.reset();
    if (const nbt::Tag* t = c.getCompound("tag")) s.tag = std::shared_ptr<nbt::Tag>(nbt::clone(*t).release());
}

void Inventory::readFromNBT(const nbt::Tag& list) {
    for (auto& s : main) s.clear();
    for (auto& s : armour) s.clear();
    for (auto& s : charms) s.clear();
    for (auto& it : list.items) {
        if (!it || it->type != nbt::Compound) continue;
        int slot = uint8_t(it->getByte("Slot"));
        ItemStack s;
        readStack(*it, s);
        if (s.id == 0) continue; // Java: getItem() == null
        if (slot < MAIN) main[slot] = s;
        if (slot >= 100 && slot < 100 + ARMOUR) armour[slot - 100] = s;
        if (slot >= 150 && slot < 150 + CHARMS) charms[slot - 150] = s;
    }
}

static void writeStack(nbt::Tag& list, int slot, const ItemStack& s) {
    nbt::TagPtr c = nbt::makeCompound();
    c->setByte("Slot", int8_t(slot));
    c->setShort("id", int16_t(s.id));
    c->setByte("Count", int8_t(s.count));
    c->setShort("Damage", s.damage);
    if (s.tag) c->set("tag", nbt::clone(*s.tag));
    list.items.push_back(std::move(c));
}

nbt::TagPtr Inventory::writeToNBT() const {
    nbt::TagPtr list = nbt::makeList(nbt::Compound);
    // Java writes a null slot as nothing; an emptied stack (count 0) is still a stack there
    for (int i = 0; i < MAIN; i++) if (main[i].id) writeStack(*list, i, main[i]);
    for (int i = 0; i < ARMOUR; i++) if (armour[i].id) writeStack(*list, i + 100, armour[i]);
    for (int i = 0; i < CHARMS; i++) if (charms[i].id) writeStack(*list, i + 150, charms[i]);
    return list;
}
