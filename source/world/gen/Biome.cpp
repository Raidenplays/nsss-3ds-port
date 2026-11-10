#include "Biome.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace {
const char* NAMES[] = {"Rainforest", "Swampland", "Seasonal Forest", "Forest", "Savanna", "Shrubland", "Taiga",
                       "Desert", "Plains", "Ice Desert", "Tundra", "Birch Forest", "Dense Forest", "The Thrams",
                       "The Drogwoods", "The Peaks", "Hell"};
const char SOLID[] =
    "0111110100001111111111111100000000010000011111111100111011101111100110111110000111101111111110000011111111111110"
    "1000011011111111111111000000001111111111111111111111111111011111111111111111111111111111111111111111111111111111"
    "11111111110000111001000000000000";

Biome table[4096];
bool tableBuilt = false;
void build() {
    for (int t = 0; t < 64; ++t)
        for (int h = 0; h < 64; ++h) table[t + h * 64] = Biomes::getBiome(float(t) / 63.0f, float(h) / 63.0f);
    tableBuilt = true;
}
std::string simplify(const char* n) {
    std::string s;
    for (const char* p = n; *p; ++p)
        if (*p != ' ' && *p != '_') s += char(std::tolower((unsigned char)*p));
    return s.compare(0, 3, "the") == 0 ? s.substr(3) : s;
}
} // namespace

bool materialSolid(uint8_t id) { return SOLID[id] == '1'; }

namespace Biomes {
const char* name(Biome b) { return NAMES[int(b)]; }

Biome byName(const char* n) {
    static const Biome OVERWORLD[] = {Biome::Rainforest, Biome::Swampland, Biome::SeasonalForest, Biome::Forest,
                                      Biome::BirchForest, Biome::DenseForest, Biome::Thrams, Biome::Drogwoods,
                                      Biome::Savanna, Biome::Shrubland, Biome::Taiga, Biome::Desert,
                                      Biome::Plains, Biome::Tundra, Biome::Peaks};
    std::string w = simplify(n);
    for (Biome b : OVERWORLD)
        if (simplify(name(b)) == w) return b;
    return Biome::Count;
}

Biome getBiome(float t, float h) {
    h *= t;
    using B = Biome;
    return t < 0.1f ? B::Tundra
           : (h < 0.2f ? (t < 0.5f ? B::Tundra : (t < 0.95f ? B::Savanna : B::Desert))
              : (h > 0.5f && t < 0.7f ? B::Swampland
                 : (t < 0.5f ? (t > 0.33f && h > 0.24f ? B::Thrams : B::Taiga)
                    : (t < 0.65f && h > 0.25f ? B::Thrams
                       : (t >= 0.7f && t < 0.97f && h < 0.28f ? B::Drogwoods
                          : (t < 0.97f ? (h < 0.35f ? B::Shrubland
                                          : (h < 0.42f ? B::BirchForest : (h > 0.75f ? B::DenseForest : B::Forest)))
                                       : (h < 0.45f ? B::Plains : (h < 0.9f ? B::SeasonalForest : B::Rainforest))))))));
}

Biome lookup(double t, double h) {
    if (!tableBuilt) build();
    return table[int(t * 63.0) + int(h * 63.0) * 64];
}

uint8_t topBlock(Biome b) { return b == Biome::Desert || b == Biome::IceDesert ? BlockId::Sand : BlockId::Grass; }
uint8_t fillerBlock(Biome b) { return b == Biome::Desert || b == Biome::IceDesert ? BlockId::Sand : BlockId::Dirt; }

void climateFor(Biome biome, double& temp, double& hum) {
    if (!tableBuilt) build();
    // the cell the biome is furthest inside (same grid as the lookup table)
    int best = -1, bestScore = -1;
    for (int i = 0; i < 64; ++i)
        for (int j = 0; j < 64; ++j) {
            if (table[i + j * 64] != biome) continue;
            int score = 0;
            for (int r = 1; r < 32; ++r) {
                bool same = true;
                for (int a = -r; a <= r && same; ++a)
                    for (int b = -r; b <= r && same; ++b) {
                        int ii = std::max(0, std::min(63, i + a)), jj = std::max(0, std::min(63, j + b));
                        same = table[ii + jj * 64] == biome;
                    }
                if (!same) break;
                score = r;
            }
            if (score > bestScore) { bestScore = score; best = i + j * 64; }
        }
    if (best < 0) { temp = hum = 0.5; return; }
    temp = double(best % 64) / 63.0;
    hum = double(best / 64) / 63.0;
}
} // namespace Biomes
