#pragma once
// The Almanac's map: a port of AxeolonMap (the explored-world store) plus the bottom-screen view.
//
// Same data and files as the Java mod: 256x256-block tiles of (top block id, height) bytes,
// saved gzipped as <world>/axeolon_map[_nether]/t_<tx>_<tz>.dat, so the map you explored on PC
// shows up here and what you explore here shows up on PC. Columns within 48 blocks of the
// player are scanned once a second; tiles are saved every 30 seconds and on quit.
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class World;

struct Waypoint {
    int x, z;
    uint8_t colour; // wool colour index 0..15
};

class AlmanacMap {
public:
    static constexpr int SIZE = 128;        // texture size in pixels
    static constexpr int SCAN_RADIUS = 48;  // blocks, same as the mod
    static constexpr int MAX_WAYPOINTS = 16;

    // folder: "<world>/axeolon_map" (or "_nether"); empty = never saved (test world)
    AlmanacMap(const World& w, const std::string& folder);
    ~AlmanacMap();

    // Every game tick (AxeolonMap.tick): scans each second, saves every 600 ticks.
    void tick(int playerX, int playerZ);
    // Centre the view (cheap when unchanged).
    void track(int playerX, int playerZ);
    // Repaints the pixel buffer if anything changed. Returns true if it did.
    bool refresh(int zoom);
    void invalidate() { dirty = true; }
    void save();

    const uint32_t* pixels() const { return pix.data(); }
    int centreX() const { return cx; }
    int centreZ() const { return cz; }
    int zoom() const { return curZoom; }

    bool isExplored(int x, int z);
    uint32_t colourAt(int x, int z); // 0xAARRGGBB like the Java, 0 when unexplored

    const std::vector<Waypoint>& waypoints() const { return wps; }
    bool toggleWaypoint(int x, int z, int radius);
    // AxeolonProgress "AxWaypoints" string in the player's save: "x:z:colour;x:z:colour"
    void readWaypoints(const std::string& s);
    std::string writeWaypoints() const;
    static uint32_t woolColour(int i);

private:
    struct Tile {
        uint8_t ids[256 * 256];
        uint8_t heights[256 * 256];
        bool dirty = false;
    };
    const World& world;
    std::string folder;
    std::unordered_map<int64_t, std::unique_ptr<Tile>> tiles;
    std::vector<uint32_t> pix;
    std::vector<Waypoint> wps;
    int cx = 0, cz = 0, curZoom = 1;
    bool dirty = true;
    int scanTimer = 0, saveTimer = 0;

    Tile& tile(int x, int z);
    bool load(int tx, int tz, Tile& t);
    void scan(int px, int pz);
    void repaint();
};
