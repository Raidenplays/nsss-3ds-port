#pragma once
// Beta's collision and liquid queries (AxisAlignedBB, World.getCollidingBoundingBoxes,
// handleMaterialAcceleration, isMaterialInBB, getIsAnyLiquid), ported line by line.
#include <vector>

class World;

struct AABB {
    double minX, minY, minZ, maxX, maxY, maxZ;

    AABB offset(double x, double y, double z) const { return {minX + x, minY + y, minZ + z, maxX + x, maxY + y, maxZ + z}; }
    void move(double x, double y, double z) { minX += x; minY += y; minZ += z; maxX += x; maxY += y; maxZ += z; }
    AABB addCoord(double x, double y, double z) const {
        AABB r = *this;
        if (x < 0) r.minX += x;
        if (x > 0) r.maxX += x;
        if (y < 0) r.minY += y;
        if (y > 0) r.maxY += y;
        if (z < 0) r.minZ += z;
        if (z > 0) r.maxZ += z;
        return r;
    }
    AABB expand(double x, double y, double z) const { return {minX - x, minY - y, minZ - z, maxX + x, maxY + y, maxZ + z}; }
    bool intersects(const AABB& o) const {
        return o.maxX > minX && o.minX < maxX && o.maxY > minY && o.minY < maxY && o.maxZ > minZ && o.minZ < maxZ;
    }
    // the ray o + d*t enters the box for some t in [0, maxT] (AxisAlignedBB.func_1169_a, as a distance
    // along d): t is where it enters (0 if it starts inside)
    bool rayHit(double ox, double oy, double oz, double dx, double dy, double dz, double maxT, double& t) const {
        double t0 = 0.0, t1 = maxT;
        const double o[3] = {ox, oy, oz}, d[3] = {dx, dy, dz};
        const double lo[3] = {minX, minY, minZ}, hi[3] = {maxX, maxY, maxZ};
        for (int a = 0; a < 3; ++a) {
            if (d[a] > -1e-12 && d[a] < 1e-12) {
                if (o[a] < lo[a] || o[a] > hi[a]) return false;
                continue;
            }
            double ta = (lo[a] - o[a]) / d[a], tb = (hi[a] - o[a]) / d[a];
            if (ta > tb) { double s = ta; ta = tb; tb = s; }
            if (ta > t0) t0 = ta;
            if (tb < t1) t1 = tb;
            if (t0 > t1) return false;
        }
        t = t0;
        return true;
    }
    // this = a block's box, b = the moving entity's box
    double calculateXOffset(const AABB& b, double d) const;
    double calculateYOffset(const AABB& b, double d) const;
    double calculateZOffset(const AABB& b, double d) const;
};

namespace Physics {
// World.getCollidingBoundingBoxes for an entity box (blocks only: Beta's living mobs have no
// collision boxes for the player)
void collidingBoxes(const World& w, const AABB& area, std::vector<AABB>& out);
// ... and the solid entities' boxes (EntityBoat.getBoundingBox): the game sets this
extern void (*entityBoxes)(const AABB& area, std::vector<AABB>& out);
// Block.getCollidingBoundingBoxes for one block
void blockBoxes(const World& w, int x, int y, int z, const AABB& area, std::vector<AABB>& out);

bool isMaterialInBB(const World& w, const AABB& bb, bool lava);       // water when lava == false
bool isAnyLiquid(const World& w, const AABB& bb);
// World.handleMaterialAcceleration for water: true if in water; pushes motion with the current
bool waterAcceleration(const World& w, const AABB& bb, double& mx, double& my, double& mz);
float slipperiness(int id);
}
