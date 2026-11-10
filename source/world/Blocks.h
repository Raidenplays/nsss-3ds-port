#pragma once
// Block behaviour: the onBlockAdded / onBlockRemoval / onNeighborBlockChange / updateTick /
// canPlaceBlockAt / canBlockStay methods of Axeolon's Block classes, ported as one dispatcher.
// Blocks with nothing ported yet act like Beta's plain Block (all no-ops).
#include <cstdint>
#include "JavaRandom.h"

class World;

namespace Blocks {
extern bool sandFallInstantly; // BlockSand.fallInstantly
extern bool flowerGenerating;  // BlockFlower.generating (Axeolon: plants only generate in lit spots)
extern bool iceCollecting;     // BlockIce.collecting
extern void (*onTntIgnited)(int x, int y, int z);
extern bool quietRemoval; // a piston is moving the block: nothing spills, crates don't drop // fire reached TNT: prime it (EntityTNTPrimed)

void onBlockAdded(World& w, int x, int y, int z, int id);
void onBlockRemoval(World& w, int x, int y, int z, int id);
void onNeighborBlockChange(World& w, int x, int y, int z, int id, int neighbour);
void updateTick(World& w, int x, int y, int z, int id, JavaRandom& r);
bool canPlaceBlockAt(World& w, int id, int x, int y, int z);
bool canBlockStay(World& w, int id, int x, int y, int z);

// Block.dropBlockAsItem (same World.rand use as Java; the items land in World::drops)
void dropBlockAsItem(World& w, int x, int y, int z, int id, int meta);

// small helpers the generators share
bool needsLight(World& w, int x, int y, int z); // BlockFlower.needsLight
bool placeCattail(World& w, int x, int y, int z, bool notify); // BlockCattail.place
bool tryToCreatePortal(World& w, int x, int y, int z);      // BlockPortal (fire on an obsidian frame)
bool isNaturalStone(int id);                                  // AxeolonStone.isNatural
int oreKind(int id);
void torchAdded(World& w, int i, int j, int k, int id); // BlockTorch.onBlockAdded (NSSS)                                          // BlockOreVariant.kindOf
} // namespace Blocks
