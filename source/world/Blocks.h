#pragma once
// Block behaviour: the onBlockAdded / onBlockRemoval / onNeighborBlockChange / updateTick /
// canPlace / canBlockStay methods of the NSSS jar's Block classes, ported as one dispatcher.
// Blocks with nothing of their own act like the jar's plain Block (all no-ops).
#include <cstdint>
#include "JavaRandom.h"

class World;

namespace Blocks {
extern bool sandFallInstantly; // BlockSand.fallInstantly
extern void (*onTntIgnited)(int x, int y, int z); // fire or power reached TNT: prime it (EntityTNTPrimed)
extern void (*onPyramidionBroken)(int x, int y, int z); // BlockPyramidion: spawn the giant (peaceful: put it back)

void onBlockAdded(World& w, int x, int y, int z, int id);
void onBlockRemoval(World& w, int x, int y, int z, int id);
void onNeighborBlockChange(World& w, int x, int y, int z, int id, int neighbour);
void updateTick(World& w, int x, int y, int z, int id, JavaRandom& r);
bool canPlace(World& w, int id, int x, int y, int z); // Block.canPlace (World.func_695_a)
bool canBlockStay(World& w, int id, int x, int y, int z);

// Block.onBlockDestroyedByPlayer: after a player broke it (the block is gone already)
void onBlockDestroyedByPlayer(World& w, int x, int y, int z, int id, int meta);

// Block.dropBlockAsItem (same World.rand use as Java; the items land in World::drops)
void dropBlockAsItem(World& w, int x, int y, int z, int id, int meta);
// Block.dropBlockGoldTouch: what a gold tool gets - the block itself
void dropBlockGoldTouch(World& w, int x, int y, int z, int id, int meta);

void torchAdded(World& w, int i, int j, int k, int id);           // BlockTorch.onBlockAdded
void torchNeighbourChanged(World& w, int x, int y, int z, int id); // BlockTorch.onNeighborBlockChange
// BlockBurntCloth.getSurroundHotBlock: hot burnt cloth within two blocks (no ice or snow there)
bool hotClothNearby(World& w, int x, int y, int z);
// BlockFire.chanceToEncourageFire (> 0: fire spreads to it; RenderBlocks.renderFire leans flames on it)
int fireEncourage(int id);
} // namespace Blocks
