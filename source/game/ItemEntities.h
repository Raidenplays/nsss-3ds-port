#pragma once
// EntityItem: dropped items lying in the world. They fall, slide, bounce, get pushed out of
// blocks, despawn after 5 minutes, and the player picks them up by walking over them.
// Saved in their chunk's "Entities" list exactly like the mod saves them.
#include <cstdint>
#include <vector>
#include "Inventory.h"
#include "world/JavaRandom.h"
#include "world/Physics.h"

class World;
class Renderer;
struct Chunk;

struct ItemEntity {
    double x, y, z, px, py, pz;  // position (centre of the bottom of the 0.25 box) and last tick's
    double mx, my, mz;
    float yaw;
    float spin;                  // field_804_d (bob phase)
    bool onGround = false;
    int age = 0;
    int pickupDelay = 0;
    int health = 5;
    ItemStack item;
};

class ItemEntities {
public:
    void spawn(double x, double y, double z, const ItemStack& s, int pickupDelay = 10);
    // EntityItem with a given motion (dropPlayerItemWithRandomChoice); y is the entity's posY
    void spawnMoving(double x, double y, double z, const ItemStack& s, double mx, double my, double mz, int pickupDelay);
    void spawnFromWorld(World& w); // World::drops (Block.dropBlockAsItem) -> entities
    // One tick for every item; picks up into inv when the player's box (expanded 1, 0, 1) touches
    void tick(World& w, const AABB& player, Inventory& inv, bool& picked);
    void render(World& w, Renderer& r, float partial, float camYaw, float camPitch);

    void chunkLoaded(Chunk& c);              // takes the "Item" entries out of the saved list
    void writeToChunk(Chunk& c) const;        // before saving: the saved list = these
    void chunkUnloaded(int cx, int cz);
    size_t count() const { return items.size(); }
    const std::vector<ItemEntity>& all() const { return items; }
    bool anyIn(const AABB& box) const; // for wooden pressure plates
    void launch(const AABB& box, double mx, double my, double mz);

private:
    std::vector<ItemEntity> items;
    JavaRandom rand{0x1D7EA5ULL};
    std::vector<AABB> boxes;
    void move(World& w, ItemEntity& e, double dx, double dy, double dz);
};
