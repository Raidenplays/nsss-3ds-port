#pragma once
// NSSS InventoryPlayer + ItemStack. Slot numbering as in the jar: main[0..8] is the hotbar, main[9..35] the
// backpack. Armour 0=boots .. 3=helmet, like InventoryPlayer.armorInventory.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "world/Nbt.h"

struct ItemStack {
    uint16_t id = 0;   // 0 = empty
    int count = 0;     // stackSize (saved as a byte)
    int16_t damage = 0;

    bool empty() const { return id == 0 || count <= 0; }
    void clear() { id = 0; count = 0; damage = 0; }
};

enum class SlotGroup : uint8_t { Main, Armour, Craft, CraftResult, Table, TableResult, Container };

class Inventory {
public:
    static constexpr int MAIN = 36;
    static constexpr int STORAGE = 36;       // hotbar + backpack (what pickups fill)
    static constexpr int HOTBAR = 9;
    static constexpr int ARMOUR = 4;
    static constexpr int CRAFT = 4;   // 2x2 grid
    static constexpr int MAX_STACK = 64;

    ItemStack main[MAIN];
    ItemStack armour[ARMOUR];
    ItemStack craft[CRAFT];
    ItemStack craftResult;
    // ContainerWorkbench: the 3x3 grid while a crafting table is open
    ItemStack table[9];
    ItemStack tableResult;
    bool tableOpen = false;
    void openTable() { tableOpen = true; updateCraftResult(); }
    void closeTable(); // the grid goes back into the inventory; what doesn't fit lands in spill
    std::vector<ItemStack> spill; // items the game should drop at the player's feet
    // An open chest (27), large chest (54), crate (27) or furnace (3: input, fuel, output)
    enum ContainerKind { NONE, CHEST, FURNACE };
    ContainerKind containerKind = NONE;
    std::vector<ItemStack> container;
    bool containerDirty = false;      // a slot changed: write it back
    int containerPos[2][3] = {};      // tile entities (a large chest has two)
    int containerParts = 0;
    int cartId = 0;                   // a chest minecart's contents (EntityMinecart as IInventory), not a block
    float furnaceCook = 0, furnaceBurn = 0; // progress bars, 0..1
    ItemStack cursor; // stack "held by the pointer" while moving items
    int currentItem = 0;

    ItemStack* slot(SlotGroup g, int i);
    ItemStack& held() { return main[currentItem]; }
    const ItemStack& held() const { return main[currentItem]; }

    // InventoryPlayer.changeCurrentItem
    void scrollHotbar(int dir);
    // InventoryPlayer.setCurrentItem (GuiBuild): the id into the held slot (swapping in a hotbar copy)
    void setCurrentItem(int id, int count);

    // InventoryPlayer.writeToNBT/readFromNBT: slots 0-35, armour 100-103, crafting 80-83 (written, never read)
    void readFromNBT(const nbt::Tag& list);
    nbt::TagPtr writeToNBT() const;

    // Beta's left-click behaviour on a slot: pick up, put down, merge or swap.
    void clickSlot(SlotGroup g, int i);
    // Beta's right-click: take half / place one.
    void rightClickSlot(SlotGroup g, int i);

    // Returns false if the inventory is full. Merges into hotbar first, like Beta.
    bool addItem(uint16_t id, int count);
    // InventoryPlayer.addItemStackToInventory: merges then fills empty slots; s.count is what's left
    bool addStack(ItemStack& s);
    // Use one of the held item (placing a block). Returns false if empty.
    bool consumeHeld();

    // InventoryCrafting -> CraftingManager.findMatchingRecipe -> craftResult (call after the grid changes)
    void updateCraftResult();
    bool creative = false;

    // Put the cursor stack back into the inventory (when closing the screen).
    void returnCursor();
};
