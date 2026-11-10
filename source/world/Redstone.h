#pragma once
// Redstone and Axeolon's lapis power: World.isBlock(Indirectly)ProvidingPowerTo / GettingPowered,
// and the blocks that make, carry or listen to power - BlockRedstoneWire, BlockRedstoneTorch,
// BlockLever, BlockButton, BlockPressurePlate, BlockPowerBlock, BlockPowerRepeater (redstone and
// lapis in, either out), AxeolonLapis (lapis dust, torches, lamps), and the listeners: doors,
// TNT and note blocks. Blocks.cpp hands these blocks' events here.
#include <cstdint>
#include "JavaRandom.h"

class World;
struct AABB;

namespace Redstone {

// Block.canProvidePower / isPoweringTo / isIndirectlyPoweringTo for the block at (x, y, z)
bool canProvidePower(int id);
bool isPoweringTo(World& w, int x, int y, int z, int side);
bool isIndirectlyPoweringTo(World& w, int x, int y, int z, int side);
// World
bool isBlockProvidingPowerTo(World& w, int x, int y, int z, int side);
bool isBlockGettingPowered(World& w, int x, int y, int z);
bool isBlockIndirectlyProvidingPowerTo(World& w, int x, int y, int z, int side);
bool isBlockIndirectlyGettingPowered(World& w, int x, int y, int z);

// Does this module handle the block's events? (then Blocks.cpp calls the ones below)
bool handles(int id);
void onBlockAdded(World& w, int x, int y, int z, int id);
void onBlockRemoval(World& w, int x, int y, int z, int id, int meta);
void onNeighborBlockChange(World& w, int x, int y, int z, int id, int neighbour);
void updateTick(World& w, int x, int y, int z, int id, JavaRandom& r);

// Right-click: lever, button, repeater delay, note block pitch. True if it was one of those.
bool blockActivated(World& w, int x, int y, int z, bool emptyHand = true);
// BlockPistonBase.orient + onBlockPlacedBy: facing the player (or up / down from above / below)
void pistonPlaced(World& w, int x, int y, int z, double px, double py, double pz, float yaw);
// Left-click: a note block plays its note
void blockClicked(World& w, int x, int y, int z);
// Entity.doBlockCollisions for pressure plates: something with this box touched them.
// living: a mob or the player (stone plates only notice those).
void entityTouching(World& w, const AABB& box, bool living);
// BlockPressurePlate needs to know what stands on it now (set by the game)
extern bool (*anythingOn)(const AABB& box, bool livingOnly);

// AxeolonLapis
bool isLapisDust(int id);
void lapisUpdate(World& w, int x, int y, int z);
void lapisAround(World& w, int x, int y, int z);

} // namespace Redstone
