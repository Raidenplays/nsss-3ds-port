#pragma once
// The bottom screen's map (the New 3DS Edition home layout): a top-down view of the loaded chunks
// around the player, painted fresh from the world. Nothing is saved.
#include <cstdint>
#include <vector>

class World;

class Minimap {
public:
    static constexpr int SIZE = 128; // texture size in pixels

    explicit Minimap(const World& w) : world(w), pix(SIZE * SIZE, 0) {}

    // Every game tick: repaints once a second.
    void tick(int playerX, int playerZ);
    // Centre the view (cheap when unchanged).
    void track(int playerX, int playerZ);
    // Repaints the pixel buffer if anything changed. Returns true if it did.
    bool refresh(int zoom);
    void invalidate() { dirty = true; }

    const uint32_t* pixels() const { return pix.data(); }
    int centreX() const { return cx; }
    int centreZ() const { return cz; }
    int zoom() const { return curZoom; }

private:
    const World& world;
    std::vector<uint32_t> pix;
    int cx = 0, cz = 0, curZoom = 1, timer = 0;
    bool dirty = true;
    uint32_t colourAt(int x, int z) const; // 0xRRGGBB, or 0 where nothing's loaded
};
