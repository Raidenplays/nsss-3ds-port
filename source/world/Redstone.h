#pragma once
// Redstone, as the NSSS jar has it: World.isBlock(Indirectly)ProvidingPowerTo / GettingPowered (with
// the motor, which always counts as powering), and the blocks that make, carry or listen to power -
// BlockRedDustWire, BlockRedTorch, BlockLever, BlockButton, BlockPressurePlate (stone: mobs, wood:
// everything, iron: players), BlockDoor and BlockTNT. Blocks.cpp hands these blocks' events here.
#include <cstdint>
#include "JavaRandom.h"

class World;
struct AABB;

namespace Redstone {

// Block.canProvidePower / isPoweringTo / isIndirectlyPoweringTo for the block at (x, y, z)
bool canProvidePower(int id);
bool isPoweringTo(World& w, int x, int y, int z, int side);
bool isIndirectlyPoweringTo(World& w, int x, int y, int z, int side);
// World: func_668_j, func_646_n, isBlockIndirectlyProvidingPowerTo, isBlockIndirectlyGettingPowered
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

// blockActivated (right-click) and onBlockClicked (left-click, the same for these): levers, buttons
// and wooden doors. True if the block took it.
bool blockActivated(World& w, int x, int y, int z);
// Entity.doBlockCollisions -> BlockPressurePlate.onEntityCollidedWithBlock: something with this box
// touched the plates it overlaps
void entityTouching(World& w, const AABB& box);
// what stands on a plate now (set by the game): kind 0 any entity, 1 living things, 2 players
extern bool (*anythingOn)(const AABB& box, int kind);

} // namespace Redstone
