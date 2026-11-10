#pragma once
// BlockMinecartTrack: rails join up with their neighbours (MinecartTrackLogic)
class World;
namespace rails {
bool canPlace(World& w, int x, int y, int z);
void onBlockAdded(World& w, int x, int y, int z);
void onNeighborChange(World& w, int x, int y, int z, int neighbour);
} // namespace rails
