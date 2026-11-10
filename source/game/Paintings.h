#pragma once
// EntityPainting / ItemPainting / RenderPainting. Paintings stay in their chunk's saved entity
// list (so they go back to the PC as they were); this keeps a copy of where they hang to draw
// them, and adds or removes the saved entry when one is hung or knocked down.
#include <vector>
#include "world/JavaRandom.h"
#include "world/Physics.h"

class World;
class Renderer;
class ItemEntities;
struct Chunk;

class Paintings {
public:
    void chunkLoaded(Chunk& c);
    void chunkUnloaded(int cx, int cz);
    void clear() { list.clear(); }
    // ItemPainting.onItemUse on a block's side; true if the click was used
    bool place(World& w, int x, int y, int z, int side, bool& hung);
    // The painting the ray hits first (within maxDist), or -1
    int pick(double ox, double oy, double oz, double dx, double dy, double dz, double maxDist, double& dist) const;
    // EntityPainting.attackEntityFrom: down it comes, as an item
    void knockDown(World& w, int index, ItemEntities& items);
    // EntityPainting.onUpdate: every 100 ticks, one with no wall behind it falls off
    void tick(World& w, ItemEntities& items);
    void render(const World& w, Renderer& r);

private:
    struct P { int tileX, tileY, tileZ, dir, art; double x, y, z; AABB box; };
    std::vector<P> list;
    JavaRandom rand{0x9A1A7ULL};
    int timer = 0;
    static void position(P& p);
    bool valid(World& w, const P& p, int ignore) const;
    void removeSaved(World& w, const P& p);
    void addSaved(World& w, const P& p);
};
