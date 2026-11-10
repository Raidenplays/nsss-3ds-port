#include "PlaceholderAtlas.h"

namespace {

inline uint32_t hash(int x, int y, int salt) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(salt) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

inline uint32_t rgba(int r, int g, int b, int a = 255) {
    auto c = [](int v) { return static_cast<uint32_t>(v < 0 ? 0 : v > 255 ? 255 : v); };
    return c(r) << 24 | c(g) << 16 | c(b) << 8 | c(a);
}

// Base colour with per-pixel noise of +/- amount
inline uint32_t noisy(int r, int g, int b, int px, int py, int tile, int amount) {
    int n = static_cast<int>(hash(px, py, tile) % (2 * amount + 1)) - amount;
    return rgba(r + n, g + n, b + n);
}

uint32_t tilePixel(int tile, int px, int py) {
    switch (tile) {
    case 0:  return noisy(96, 160, 64, px, py, tile, 18);                       // grass top
    case 1:  return noisy(125, 125, 125, px, py, tile, 14);                     // stone
    case 2:  return noisy(134, 96, 67, px, py, tile, 16);                       // dirt
    case 3:  // grass side: dirt with a green lip
        if (py < 3 || (py == 3 && (hash(px, 0, 3) & 1))) return noisy(96, 160, 64, px, py, 0, 18);
        return noisy(134, 96, 67, px, py, 2, 16);
    case 4: { // planks
        bool seam = (py % 4) == 3 || (px == ((py / 4) * 5) % 16);
        return seam ? rgba(110, 85, 50) : noisy(160, 130, 80, px, py, tile, 10);
    }
    case 16: { // cobblestone
        bool crack = (hash(px / 3, py / 3, tile) % 5) == 0;
        return crack ? rgba(70, 70, 70) : noisy(115, 115, 115, px, py, tile, 22);
    }
    case 17: return noisy(70, 70, 70, px, py, tile, 40);                        // bedrock
    case 18: return noisy(219, 211, 160, px, py, tile, 10);                     // sand
    case 19: return noisy(130, 124, 122, px, py, tile, 30);                     // gravel
    case 20: return (px % 4 == 0) ? rgba(80, 62, 38) : noisy(104, 82, 50, px, py, tile, 8); // log side
    case 21: { // log top: rings
        int dx = px - 8, dy = py - 8, d = dx * dx + dy * dy;
        if (d > 49) return rgba(104, 82, 50);
        return ((d / 9) & 1) ? rgba(170, 140, 90) : rgba(150, 120, 75);
    }
    case 32: case 33: case 34: { // gold / iron / coal ore
        bool fleck = (hash(px / 2, py / 2, tile) % 6) == 0;
        if (!fleck) return noisy(125, 125, 125, px, py, 1, 14);
        if (tile == 32) return rgba(250, 238, 77);
        if (tile == 33) return rgba(216, 175, 147);
        return rgba(30, 30, 30);
    }
    case 52: { // fancy leaves: holes are transparent
        if (hash(px, py, tile) % 5 == 0) return rgba(0, 0, 0, 0);
        return noisy(60, 130, 40, px, py, tile, 22);
    }
    default: // unused: magenta/black "missing texture" checker
        return ((px / 8 + py / 8) & 1) ? rgba(248, 0, 248) : rgba(0, 0, 0);
    }
}

} // namespace

void PlaceholderAtlas::generate(uint32_t* pixels) {
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            int tile = (y / 16) * 16 + (x / 16);
            pixels[y * SIZE + x] = tilePixel(tile, x % 16, y % 16);
        }
}
