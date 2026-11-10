#pragma once
// Using the held item: Item.onItemUse on a block (ItemBlock placement with the blocks' own
// onBlockPlaced / onBlockPlacedBy facing, hoes, seeds, sugar cane, doors, flint and steel,
// bone meal) and Item.onItemRightClick in the air (food, buckets).
#include <cstdint>

class World;
class Player;
class Inventory;
class Mobs;
struct RayHit;

namespace itemuse {

// side: Beta's face numbers (0 bottom, 1 top, 2 -Z, 3 +Z, 4 -X, 5 +X)
int sideOf(const RayHit& hit);

// Right-click on a block with the held item. hitY: where on the block the crosshair is, 0..1 from
// its bottom (modern top/bottom slab and stair placement). Returns true if the click was used.
bool useOnBlock(World& w, Player& p, Inventory& inv, const RayHit& hit, float hitY, bool creative);

// Right-click with the held item (everything that doesn't need a block): food and buckets.
// eye / look: where the player looks from and toward. Returns true if something happened.
bool useItem(World& w, Player& p, Inventory& inv, bool creative, double ex, double ey, double ez, double lx, double ly,
             double lz);

// ItemBow (uses an arrow and a point of wear), ItemSnowball, ItemEgg. True if something flew.
bool throwHeld(World& w, Player& p, Inventory& inv, bool creative, Mobs& mobs);

// A sign was just placed: the game opens the keyboard for its text (TileEntitySign at signX/Y/Z)
extern bool signEdit;
extern int signX, signY, signZ;

} // namespace itemuse
