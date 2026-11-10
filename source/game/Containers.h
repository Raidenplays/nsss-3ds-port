#pragma once
// Chests, crates and furnaces: their GUI contents (InventoryLargeChest, ContainerFurnace) backed
// by the tile entity's saved NBT, and TileEntityFurnace.updateEntity for every loaded furnace.
#include <vector>
#include "Inventory.h"

class World;

namespace containers {
// BlockChest / BlockCrate / BlockFurnace .blockActivated: fills inv.container and remembers the
// tile entities. Returns false if the block isn't a container or can't be opened (a block on top).
bool open(World& w, Inventory& inv, int x, int y, int z);
void close(World& w, Inventory& inv);  // writes the slots back
void save(World& w, Inventory& inv);   // after every change while open
void refresh(World& w, Inventory& inv); // re-read (the furnace changes by itself)
void tickFurnaces(World& w);           // once a game tick
bool isContainer(int id);
// a chest minecart opened like a chest: its 27 slots, written back through saveCart
void openCart(Inventory& inv, int cartId, const std::vector<ItemStack>& items);
extern void (*saveCart)(int cartId, const std::vector<ItemStack>& items);

// ItemStack <-> NBT (ItemStack.writeToNBT / readFromNBT)
ItemStack fromNBT(const nbt::Tag& t);
void toNBT(const ItemStack& s, nbt::Tag& t);
} // namespace containers
