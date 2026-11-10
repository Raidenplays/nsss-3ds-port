#include "Minimap.h"
#include "world/Chunk.h"
#include "world/World.h"

namespace {
uint32_t baseColour(int id) {
    switch (id) {
    case 2: return 0x7FB238;                          // grass
    case 3: case 60: return 0x976D4D;                 // dirt, soil
    case 8: case 9: return 0x4060E0;                  // water
    case 10: case 11: return 0xE04010;                // lava
    case 12: return 0xE8DCA0;                         // sand
    case 13: return 0x8A8480;                         // gravel
    case 17: case 5: case 47: case 53: case 58: return 0x8F7748; // wood
    case 18: case 6: case 31: case 37: case 38: case 39: case 40: case 81: case 83: case 88: return 0x2E7D1E; // plants
    case 78: case 80: return 0xF4F8FF;                // snow
    case 79: return 0xA0B8FF;                         // ice
    case 82: return 0xA4A8B8;                         // clay
    default: return 0x707070;
    }
}
} // namespace

uint32_t Minimap::colourAt(int x, int z) const {
    const Chunk* c = world.getChunk(x >> 4, z >> 4);
    if (!c) return 0;
    int h = c->heightMap[(z & 15) << 4 | (x & 15)];
    int id = h > 0 ? world.getBlockId(x, h - 1, z) : 0;
    if (world.getBlockId(x, h, z) == 78) id = 78;
    const Chunk* n = world.getChunk(x >> 4, (z - 1) >> 4);
    float shade = 0.86f;
    if (n && id != 8 && id != 9) {
        int nh = n->heightMap[((z - 1) & 15) << 4 | (x & 15)];
        if (h > nh) shade = 1.0f;
        else if (h < nh) shade = 0.71f;
    }
    uint32_t b = baseColour(id);
    int r = int(((b >> 16) & 255) * shade), g = int(((b >> 8) & 255) * shade), bl = int((b & 255) * shade);
    return uint32_t(r << 16 | g << 8 | bl) | 0x01000000u;
}

void Minimap::tick(int px, int pz) {
    if (++timer >= 20) {
        timer = 0;
        dirty = true;
    }
    track(px, pz);
}

void Minimap::track(int px, int pz) {
    if (px == cx && pz == cz) return;
    cx = px;
    cz = pz;
    dirty = true;
}

bool Minimap::refresh(int zoom) {
    if (zoom != curZoom) { curZoom = zoom; dirty = true; }
    if (!dirty) return false;
    const int half = SIZE / 2;
    for (int py = 0; py < SIZE; py++) {
        int wz = cz + (py - half) * curZoom;
        for (int px = 0; px < SIZE; px++) {
            uint32_t c = colourAt(cx + (px - half) * curZoom, wz);
            if (c == 0) c = 0x202020;
            pix[py * SIZE + px] = (c & 0xFFFFFF) << 8 | 0xFF; // RGBA
        }
    }
    dirty = false;
    return true;
}
