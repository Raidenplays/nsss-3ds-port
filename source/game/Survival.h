#pragma once
// Survival rules from NSSS: block hardness and tool speeds (Block.blockStrength), what a block
// drops, item stack sizes / durability / fuel, crafting (CraftingManager) and smelting
// (TileEntityFurnace.getCookResult). Data in SurvivalData.cpp, generated from the jar (tools/gen_survival.py).
#include <cstdint>
#include <string>

namespace survival {
struct Drop { int16_t id, damage, quantity; }; // -2 = depends on the random (Blocks.cpp)
struct ItemData { uint16_t id; uint8_t maxStack; uint16_t maxDamage; uint16_t burnTime; };
struct ToolEntry { uint16_t item; uint8_t block; float speed; bool harvests; };
struct Stack { int16_t id; int8_t count; int16_t damage; };
struct Ingredient { int16_t id, damage; }; // id -1 = empty, damage -1 = any
struct Shaped { uint8_t w, h; Stack out; Ingredient in[9]; };
struct Shapeless { Stack out; uint8_t n; Ingredient in[9]; };
struct Smelt { uint16_t in; Stack out; };

extern const float HARDNESS[256];
extern const Drop DROPS[256][16];
extern const ItemData ITEM_DATA[];
extern const int ITEM_DATA_COUNT;
extern const ToolEntry TOOL_ENTRIES[];
extern const int TOOL_ENTRY_COUNT;
extern const Shaped SHAPED[];
extern const int SHAPED_COUNT;
extern const Shapeless SHAPELESS[];
extern const int SHAPELESS_COUNT;
extern const Smelt SMELTING[];
extern const int SMELT_COUNT;
extern const int16_t RECIPE_ORDER[];
extern const int RECIPE_COUNT;

const ItemData* item(int id);
int maxStack(int id);
int maxDamage(int id);
int burnTime(int id, int damage);
float toolSpeed(int item, int block);      // ItemStack.getStrVsBlock (1 without a tool)
bool canHarvest(int item, int block);      // InventoryPlayer.canHarvestBlock
// Block.blockStrength: the fraction of the block broken per tick
float blockStrength(int block, int heldItem, bool inWater, bool onGround);

// CraftingManager.findMatchingRecipe. grid: w*w stacks (w = 2 inventory, 3 table), id 0 = empty.
Stack craft(const Stack* grid, int w);
// Does this stack fill that ingredient slot?
bool accepts(const Ingredient& want, int id, int damage);
// TileEntityFurnace.getCookResult; id 0 = none
Stack smelt(int id, int damage);
} // namespace survival
