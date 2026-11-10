#include "AlmanacMap.h"
#include <cstdio>
#include <sys/stat.h>
#include "world/Nbt.h"
#include "world/World.h"

namespace {
// AxeolonMap.baseColour for every block id (dumped from the mod)
const uint32_t BASE_COLOUR[256] = {
0x707070,0x707070,0x7FB238,0x976D4D,0x707070,0x8F7748,0x2E7D1E,0x707070,
0x4060E0,0x4060E0,0xE04010,0xE04010,0xE8DCA0,0x8A8480,0x707070,0x707070,
0x707070,0x8F7748,0x2E7D1E,0x707070,0x707070,0x707070,0x707070,0x707070,
0xE8DCA0,0x8F7748,0x707070,0x707070,0x707070,0x707070,0x707070,0x2E7D1E,
0x2E7D1E,0x707070,0x707070,0xC7C7C7,0x707070,0x2E7D1E,0x2E7D1E,0x2E7D1E,
0x2E7D1E,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x8F7748,
0x707070,0x707070,0x707070,0x707070,0x707070,0x8F7748,0x8F7748,0x707070,
0x707070,0x707070,0x8F7748,0x2E7D1E,0x976D4D,0x707070,0x707070,0x8F7748,
0x8F7748,0x707070,0x707070,0x707070,0x8F7748,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0xF4F8FF,0xA0B8FF,
0xF4F8FF,0x3E8A2E,0xA4A8B8,0x2E7D1E,0x8F7748,0x8F7748,0xD87F33,0x700200,
0xE8DCA0,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x2E7D1E,0x2E7D1E,0x2E7D1E,0x2E7D1E,0x8F7748,0x8F7748,0x2E7D1E,
0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,
0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x707070,0x2E7D1E,0x2E7D1E,
0x2E7D1E,0x707070,0x707070,0x707070,0x707070,0x2E7D1E,0x8F7748,0x707070,
0xA0B8FF,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,0x8F7748,
0x8F7748,0x8F7748,0x707070,0xA0B8FF,0x707070,0xF4F8FF,0xA0B8FF,0xA0B8FF,
0x707070,0x707070,0xF4F8FF,0xF4F8FF,0xA0B8FF,0xA0B8FF,0x707070,0x707070,
0xF4F8FF,0xF4F8FF,0x2E7D1E,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,0x707070,
};
constexpr int SNOW = 78, WATER = 8, WATER_MOVING = 9;
constexpr uint32_t PARCHMENT = 0xFFD9C79A; // AxeolonMap.draw: unexplored
inline int64_t key(int tx, int tz) { return int64_t(tx) << 32 ^ (int64_t(tz) & 0xFFFFFFFFLL); }
inline int index(int x, int z) { return (z & 255) * 256 + (x & 255); }
} // namespace

AlmanacMap::AlmanacMap(const World& w, const std::string& dir) : world(w), folder(dir), pix(SIZE * SIZE, 0) {}

AlmanacMap::~AlmanacMap() {}

uint32_t AlmanacMap::woolColour(int i) {
    // AxeolonProgress.COLOURS
    static const uint32_t c[16] = {0xE4E4E4, 0xEA7E35, 0xBE49C9, 0x6387D2, 0xC2B51C, 0x39BA2E,
                                   0xD98199, 0x414141, 0xA0A7A7, 0x267191, 0x7E34BF, 0x253193,
                                   0x56331C, 0x364B18, 0x9E2B27, 0x181414};
    return c[i & 15];
}

bool AlmanacMap::load(int tx, int tz, Tile& t) {
    if (folder.empty()) return false;
    std::string p = folder + "/t_" + std::to_string(tx) + "_" + std::to_string(tz) + ".dat";
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> raw;
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) raw.insert(raw.end(), buf, buf + n);
    fclose(f);
    std::vector<uint8_t> out;
    if (!nbt::inflateAny(raw.data(), raw.size(), out) || out.size() < sizeof t.ids + sizeof t.heights) return false;
    std::copy(out.begin(), out.begin() + sizeof t.ids, t.ids);
    std::copy(out.begin() + sizeof t.ids, out.begin() + sizeof t.ids + sizeof t.heights, t.heights);
    return true;
}

AlmanacMap::Tile& AlmanacMap::tile(int x, int z) {
    int tx = x >> 8, tz = z >> 8;
    auto it = tiles.find(key(tx, tz));
    if (it != tiles.end()) return *it->second;
    std::unique_ptr<Tile> t(new Tile);
    if (!load(tx, tz, *t)) {
        std::fill(t->ids, t->ids + sizeof t->ids, 0);
        std::fill(t->heights, t->heights + sizeof t->heights, 0);
    }
    Tile& r = *t;
    tiles[key(tx, tz)] = std::move(t);
    return r;
}

void AlmanacMap::save() {
    if (folder.empty()) return;
    for (auto& kv : tiles) {
        Tile& t = *kv.second;
        if (!t.dirty) continue;
        int tx = int(kv.first >> 32), tz = int(int32_t(kv.first));
        mkdir(folder.c_str(), 0777);
        std::vector<uint8_t> raw(t.ids, t.ids + sizeof t.ids), gz;
        raw.insert(raw.end(), t.heights, t.heights + sizeof t.heights);
        if (!nbt::deflateGzip(raw.data(), raw.size(), gz)) continue;
        std::string p = folder + "/t_" + std::to_string(tx) + "_" + std::to_string(tz) + ".dat";
        FILE* f = fopen(p.c_str(), "wb");
        if (!f) continue;
        bool ok = fwrite(gz.data(), 1, gz.size(), f) == gz.size();
        fclose(f);
        if (ok) t.dirty = false;
    }
}

void AlmanacMap::scan(int px, int pz) {
    for (int x = px - SCAN_RADIUS; x <= px + SCAN_RADIUS; ++x)
        for (int z = pz - SCAN_RADIUS; z <= pz + SCAN_RADIUS; ++z) {
            const Chunk* c = world.getChunk(x >> 4, z >> 4);
            if (!c) continue; // blockExists
            int h = c->heightMap[(z & 15) << 4 | (x & 15)];
            int id = h > 0 ? world.getBlockId(x, h - 1, z) : 0;
            if (world.getBlockId(x, h, z) == SNOW) id = SNOW;
            if (id == 0) id = 1;
            Tile& t = tile(x, z);
            int i = index(x, z);
            if (t.ids[i] != uint8_t(id) || t.heights[i] != uint8_t(h)) {
                t.ids[i] = uint8_t(id);
                t.heights[i] = uint8_t(h);
                t.dirty = true;
                dirty = true;
            }
        }
}

void AlmanacMap::tick(int px, int pz) {
    if (++scanTimer >= 20) {
        scanTimer = 0;
        scan(px, pz);
    }
    if (++saveTimer >= 600) {
        saveTimer = 0;
        save();
    }
}

void AlmanacMap::track(int px, int pz) {
    if (px == cx && pz == cz) return;
    cx = px;
    cz = pz;
    dirty = true;
}

bool AlmanacMap::isExplored(int x, int z) { return tile(x, z).ids[index(x, z)] != 0; }

uint32_t AlmanacMap::colourAt(int x, int z) {
    Tile& t = tile(x, z);
    int i = index(x, z);
    int id = t.ids[i];
    if (id == 0) return 0;
    int h = t.heights[i];
    Tile& n = tile(x, z - 1);
    int nh = n.heights[index(x, z - 1)], nid = n.ids[index(x, z - 1)];
    float shade = 0.86f;
    if (nid != 0) {
        if (h > nh) shade = 1.0f;
        else if (h < nh) shade = 0.71f;
    }
    uint32_t base = BASE_COLOUR[id];
    if (id == WATER || id == WATER_MOVING) shade = 0.86f;
    int r = int(((base >> 16) & 255) * shade), g = int(((base >> 8) & 255) * shade), b = int((base & 255) * shade);
    return 0xFF000000u | r << 16 | g << 8 | b;
}

bool AlmanacMap::refresh(int zoom) {
    if (zoom != curZoom) { curZoom = zoom; dirty = true; }
    if (!dirty) return false;
    repaint();
    dirty = false;
    return true;
}

void AlmanacMap::repaint() {
    const int half = SIZE / 2;
    for (int py = 0; py < SIZE; py++) {
        int wz = cz + (py - half) * curZoom;
        for (int px = 0; px < SIZE; px++) {
            int wx = cx + (px - half) * curZoom;
            uint32_t c = colourAt(wx, wz);
            if (c == 0) c = PARCHMENT;
            pix[py * SIZE + px] = (c & 0xFFFFFF) << 8 | 0xFF; // ARGB -> RGBA
        }
    }
}

bool AlmanacMap::toggleWaypoint(int x, int z, int radius) {
    for (size_t i = 0; i < wps.size(); i++) {
        int dx = wps[i].x - x, dz = wps[i].z - z;
        if (dx * dx + dz * dz <= radius * radius) {
            wps.erase(wps.begin() + i);
            return true;
        }
    }
    if (int(wps.size()) >= MAX_WAYPOINTS) return false;
    for (int c = 0; c < 16; c++) {
        bool used = false;
        for (auto& w : wps) used |= w.colour == c;
        if (!used) {
            wps.push_back({x, z, uint8_t(c)});
            return true;
        }
    }
    return false;
}

void AlmanacMap::readWaypoints(const std::string& s) {
    wps.clear();
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find(';', start);
        if (end == std::string::npos) end = s.size();
        int x, z, c;
        char tail;
        std::string part = s.substr(start, end - start);
        if (sscanf(part.c_str(), "%d:%d:%d%c", &x, &z, &c, &tail) == 3) wps.push_back({x, z, uint8_t(c)});
        start = end + 1;
    }
    dirty = true;
}

std::string AlmanacMap::writeWaypoints() const {
    std::string b;
    for (auto& w : wps) {
        if (!b.empty()) b += ';';
        b += std::to_string(w.x) + ":" + std::to_string(w.z) + ":" + std::to_string(int(w.colour));
    }
    return b;
}
