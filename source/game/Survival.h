#pragma once
// Survival rules from the mod: block hardness and tool speeds (Block.blockStrength), what a
// block drops, item stack sizes / durability / fuel, crafting (CraftingManager with Axeolon's
// cobblestone stand-ins and achievement locks) and smelting (FurnaceRecipes).
// Data in SurvivalData.cpp, generated from the mod itself (tools/gen_survival.py).
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
struct Lock { uint16_t out; const char* achievement; };

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
extern const Lock LOCKS[];
extern const int LOCK_COUNT;

const ItemData* item(int id);
int maxStack(int id);
int maxDamage(int id);
int burnTime(int id, int damage);
float toolSpeed(int item, int block);      // ItemStack.getStrVsBlock (1 without a tool)
bool canHarvest(int item, int block);      // InventoryPlayer.canHarvestBlock
// Block.blockStrength: the fraction of the block broken per tick
float blockStrength(int block, int heldItem, bool inWater, bool onGround);

// CraftingManager.findMatchingRecipe. grid: w*w stacks (w = 2 inventory, 3 table), id 0 = empty.
// hasAchievement: for locked recipes (null = everything unlocked).
Stack craft(const Stack* grid, int w, bool (*hasAchievement)(const char*) = nullptr);
// Does this stack fill that ingredient slot of a recipe for `out`? (AxeolonRecipeSubs included)
bool accepts(const Ingredient& want, int id, int damage, int out);
// AxeolonRecipeLocks: is this output locked behind an achievement the player hasn't got?
bool locked(int out, bool (*hasAchievement)(const char*));
// FurnaceRecipes.getSmeltingResult(ItemStack); id 0 = none
Stack smelt(int id, int damage);
} // namespace survival
