#pragma once
// Stand-in for b1.2_02 InventoryPlayer + ItemStack, plus Axeolon's charm row.
// Slot numbering matches Beta: main[0..8] is the hotbar, main[9..35] the
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
    std::shared_ptr<nbt::Tag> tag; // ItemStack.stackTag (enchant-like Axeolon data), kept as-is

    bool empty() const { return id == 0 || count <= 0; }
    void clear() { id = 0; count = 0; damage = 0; tag.reset(); }
};

enum class SlotGroup : uint8_t { Main, Armour, Charm, Craft, CraftResult, Table, TableResult, Container, CutInput, CutResult };

class Inventory {
public:
    static constexpr int MAIN = 42;          // Axeolon: 36 storage + 6 battle slots
    static constexpr int STORAGE = 36;       // hotbar + backpack (what pickups fill)
    static constexpr int BATTLE_START = 36;  // 3 weapon pairs: main hand, off hand
    static constexpr int HOTBAR = 9;
    static constexpr int ARMOUR = 4;
    static constexpr int CHARMS = 3;  // InventoryCharms.SIZE
    static constexpr int CRAFT = 4;   // 2x2 grid
    static constexpr int MAX_STACK = 64;

    ItemStack main[MAIN];
    ItemStack armour[ARMOUR];
    ItemStack charms[CHARMS];
    ItemStack craft[CRAFT];
    ItemStack craftResult;
    // ContainerWorkbench: the 3x3 grid while a crafting table is open
    ItemStack table[9];
    ItemStack tableResult;
    bool tableOpen = false;
    void openTable() { tableOpen = true; updateCraftResult(); }
    void closeTable(); // the grid goes back into the inventory; what doesn't fit lands in spill
    std::vector<ItemStack> spill; // items the game should drop at the player's feet
    // CraftingInventoryStonecutterCB: one input slot, the pieces it can be cut into, the picked one
    bool cutterOpen = false;
    ItemStack cutInput, cutResult;
    std::vector<ItemStack> cutChoices;
    int cutSelected = -1;
    void openStonecutter() { cutterOpen = true; refreshCuts(); }
    void closeStonecutter(); // the input goes back into the inventory
    void selectCut(int i);   // selectRecipe
    void refreshCuts();      // updateResults: a new input clears the choice
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
    bool battleMode = false;
    int battleSlot = 0;   // which of the 3 pairs is in use
    int hotbarSlot = 0;   // hotbar slot to go back to when leaving battle mode

    ItemStack* slot(SlotGroup g, int i);
    ItemStack& held() { return main[currentItem]; }
    const ItemStack& held() const { return main[currentItem]; }

    // InventoryPlayer.changeCurrentItem (in battle mode it steps through the 3 pairs)
    void scrollHotbar(int dir);
    void setBattleMode(bool on);
    void selectBattleSlot(int pair);
    ItemStack* offhand(); // battle mode off-hand slot (no two-handed check yet)

    // InventoryPlayer.writeToNBT/readFromNBT: slots 0-41, armour 100-103, charms 150-152
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
    // The player's achievements (AxAchievements), for recipes the mod locks behind them
    std::string achievements;
    bool sawNight = false; // AxSawNight: up through a night, waiting for the morning (Night Watch)
    int tutorial = -1;     // AxTutorial: the how-to-play step (-1 = not started, 99 = done)
    std::string seenItems; // AxSeenItems: every item id that's been in the inventory, comma-separated
    bool creative = false; // the Almanac shows every recipe in creative
    bool hasSeen(int id) const;
    void markSeen();       // AxeolonProgress.tick, every second: what's in the inventory now

    // Put the cursor stack back into the inventory (when closing the screen).
    void returnCursor();
};
