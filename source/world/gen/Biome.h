#pragma once
// MobSpawnerBase: the biome list, the 64x64 climate lookup and each biome's surface blocks.
#include <cstdint>

enum class Biome : uint8_t {
    Rainforest, Swampland, SeasonalForest, Forest, Savanna, Shrubland, Taiga, Desert, Plains, IceDesert, Tundra,
    BirchForest, DenseForest, Thrams, Drogwoods, Peaks, Hell, Count
};

namespace Biomes {
const char* name(Biome b);
Biome byName(const char* name);       // MobSpawnerBase.byName (ignores case, spaces, a leading "the")
Biome getBiome(float temp, float hum); // MobSpawnerBase.getBiome
Biome lookup(double temp, double hum); // getBiomeFromLookup
uint8_t topBlock(Biome b);
uint8_t fillerBlock(Biome b);
void climateFor(Biome b, double& temp, double& hum); // BiomeOverride.climateFor
} // namespace Biomes

namespace BlockId {
enum : uint8_t {
    Air = 0, Stone = 1, Grass = 2, Dirt = 3, Cobble = 4, Planks = 5, Sapling = 6, Bedrock = 7,
    // MCP's Beta names are swapped: Block.waterStill is the flowing block (8), lavaStill is 10
    WaterStill = 8, WaterMoving = 9, LavaStill = 10, LavaMoving = 11, Sand = 12, Gravel = 13, GoldOre = 14, IronOre = 15,
    CoalOre = 16, Log = 17, Leaves = 18, Sandstone = 24, TallGrass = 31, DeadBush = 32, Dandelion = 37,
    Rose = 38, BrownMushroom = 39, RedMushroom = 40, MossyCobble = 48, MobSpawner = 52, Chest = 54,
    DiamondOre = 56, RedstoneOre = 73, Snow = 78, Ice = 79, SnowBlock = 80, Cactus = 81, Clay = 82, Reed = 83,
    Pumpkin = 86, LapisOre = 21,
    StoneTypes = 171, // + AxeolonStone type
};
}
// Block.blocksList[id].blockMaterial.isSolid(), dumped from the mod (gentest/DumpSolid)
bool materialSolid(uint8_t id);
