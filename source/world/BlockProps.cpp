#include "BlockProps.h"

const BlockProps BP[256] = {
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::Stone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 1 stone
    {BC::Grass, Mat::ground, 255, 0, 10, true, true, false, true, true}, // 2 grass
    {BC::Dirt, Mat::ground, 255, 0, 10, true, true, false, false, true}, // 3 dirt
    {BC::Stone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 4 cobblestone
    {BC::Plain, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 5 planks
    {BC::Sapling, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 6 sapling
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 7 bedrock
    {BC::Flowing, Mat::water, 3, 0, 5, false, true, false, true, false}, // 8 waterMoving
    {BC::Stationary, Mat::water, 3, 0, 5, false, true, false, false, false}, // 9 waterStill
    {BC::Flowing, Mat::lava, 255, 15, 30, false, true, false, true, false}, // 10 lavaMoving
    {BC::Stationary, Mat::lava, 255, 15, 30, false, true, false, true, false}, // 11 lavaStill
    {BC::Sand, Mat::sand, 255, 0, 3, true, true, false, false, true}, // 12 sand
    {BC::Gravel, Mat::sand, 255, 0, 3, true, true, false, false, true}, // 13 gravel
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 14 oreGold
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 15 oreIron
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 16 oreCoal
    {BC::Log, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 17 wood
    {BC::Leaves, Mat::leaves, 1, 0, 10, false, true, false, true, false}, // 18 leaves
    {BC::Sponge, Mat::sponge, 255, 0, 10, true, true, false, false, true}, // 19 sponge
    {BC::Glass, Mat::glass, 0, 0, 10, false, true, false, false, false}, // 20 glass
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 21 cloth_red
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 22 cloth_orange
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 23 cloth_yellow
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 24 cloth_lime
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 25 cloth_green
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 26 cloth_teal
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 27 cloth_cyan
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 28 cloth_blue
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 29 cloth_indigo
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 30 cloth_violet
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 31 cloth_lilac
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 32 cloth_pink
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 33 cloth_magenta
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 34 cloth_darkgray
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 35 cloth
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 36 cloth_lightgray
    {BC::Flower, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 37 plantYellow
    {BC::Flower, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 38 plantRed
    {BC::Mushroom, Mat::plants, 0, 1, 10, false, false, false, true, false}, // 39 mushroomBrown
    {BC::Mushroom, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 40 mushroomRed
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 41 blockGold
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 42 blockSteel
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 43 stepDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 44 stepSingle
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 45 brick
    {BC::TNT, Mat::tnt, 255, 0, 10, true, true, false, false, true}, // 46 tnt
    {BC::Bookshelf, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 47 bookShelf
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 48 cobblestoneMossy
    {BC::Obsidian, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 49 obsidian
    {BC::Torch, Mat::circuits, 0, 14, 10, false, false, false, true, false}, // 50 torchWood
    {BC::Fire, Mat::fire, 0, 15, 10, false, false, false, true, false}, // 51 fire
    {BC::MobSpawner, Mat::rock, 0, 0, 10, false, true, true, false, false}, // 52 mobSpawner
    {BC::Stairs, Mat::wood, 0, 0, 10, false, true, false, false, false}, // 53 stairCompact_Wood
    {BC::Chest, Mat::wood, 255, 0, 10, true, true, true, false, true}, // 54 crate
    {BC::RedstoneWire, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 55 redWire
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 56 oreDiamond
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 57 blockDiamond
    {BC::Workbench, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 58 workbench
    {BC::Crops, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 59 crops
    {BC::Soil, Mat::ground, 255, 0, 10, false, true, false, true, false}, // 60 tilledField
    {BC::Furnace, Mat::rock, 255, 0, 10, true, true, true, false, true}, // 61 stoneOvenIdle
    {BC::Furnace, Mat::rock, 255, 13, 10, true, true, true, false, true}, // 62 stoneOvenActive
    {BC::Sign, Mat::wood, 0, 0, 10, false, false, true, false, false}, // 63 signPost
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 64 doorWood
    {BC::Ladder, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 65 ladder
    {BC::MinecartTrack, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 66 minecartTrack
    {BC::Stairs, Mat::rock, 0, 0, 10, false, true, false, false, false}, // 67 stairCompactStone
    {BC::Sign, Mat::wood, 0, 0, 10, false, false, true, false, false}, // 68 pressurePlateWoodIdle
    {BC::Lever, Mat::circuits, 0, 0, 10, false, true, false, false, false}, // 69 lever
    {BC::PressurePlate, Mat::rock, 0, 0, 20, false, true, false, true, false}, // 70 pressurePlateStone
    {BC::Door, Mat::iron, 0, 0, 10, false, false, false, false, false}, // 71 doorSteel
    {BC::PressurePlate, Mat::rock, 0, 0, 20, false, true, false, true, false}, // 72 pressurePlateWood
    {BC::RedstoneOre, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 73 oreRed
    {BC::RedstoneOre, Mat::rock, 255, 9, 30, true, true, false, true, true}, // 74 oreRedGlowing
    {BC::RedstoneTorch, Mat::circuits, 0, 0, 2, false, false, false, true, false}, // 75 torchRedIdle
    {BC::RedstoneTorch, Mat::circuits, 0, 7, 2, false, false, false, true, false}, // 76 torchRedActive
    {BC::Button, Mat::circuits, 0, 0, 20, false, false, false, true, false}, // 77 button
    {BC::Snow, Mat::snow, 0, 0, 10, false, true, false, true, false}, // 78 snow
    {BC::Ice, Mat::ice, 3, 0, 10, false, true, false, true, false}, // 79 ice
    {BC::SnowBlock, Mat::builtSnow, 255, 0, 10, true, true, false, true, true}, // 80 blockSnow
    {BC::Cactus, Mat::cactus, 0, 0, 10, false, false, false, true, false}, // 81 cactus
    {BC::Clay, Mat::clay, 255, 0, 10, true, true, false, false, true}, // 82 blockClay
    {BC::Reed, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 83 reed
    {BC::JukeBox, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 84 jukebox
    {BC::Fence, Mat::wood, 0, 0, 10, false, true, false, false, false}, // 85 fence
    {BC::Gear, Mat::circuits, 0, 0, 1, false, false, true, false, false}, // 86 gear
    {BC::SpongeInactive, Mat::sponge, 255, 0, 10, true, true, false, false, true}, // 87 spongeInactive
    {BC::Flower, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 88 plantBlue
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 89 oreCopper
    {BC::Motor, Mat::iron, 255, 13, 10, true, true, false, false, true}, // 90 motor
    {BC::Motor, Mat::iron, 255, 13, 10, true, true, false, false, true}, // 91 generator
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 92 blockCopper
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 93 brickMossy
    {BC::BleedingObsidian, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 94 bleedingObsidian
    {BC::Web, Mat::web, 0, 0, 10, false, false, false, false, false}, // 95 web
    {BC::PressurePlate, Mat::rock, 0, 0, 20, false, true, false, true, false}, // 96 pressurePlateSteel
    {BC::LightBulb, Mat::glass, 255, 0, 10, true, true, false, false, true}, // 97 lightBulb
    {BC::LightBulb, Mat::glass, 255, 15, 10, true, true, false, false, true}, // 98 lightBulbOn
    {BC::Pyramidion, Mat::iron, 255, 15, 10, true, true, false, false, true}, // 99 pyramidion
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 100 stoneSand
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 101 brickBlue
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 102 brickFlint
    {BC::Clay, Mat::clay, 255, 0, 10, true, true, false, false, true}, // 103 blockClayRed
    {BC::Plain, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 104 cloth_tekheles
    {BC::BurntCloth, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 105 cloth_burnt
    {BC::BurntCloth, Mat::cloth, 255, 3, 10, true, true, false, false, true}, // 106 cloth_burnt_hot
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
};

const BlockSound BS[256] = {
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 1
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 2
    {"step.gravel", "step.gravel", 1.0f, 1.0f}, // 3
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 4
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 5
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 6
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 7
    {"step.stone", "step.stone", 0.0f, 0.0f}, // 8
    {"step.stone", "step.stone", 0.0f, 0.0f}, // 9
    {"step.stone", "step.stone", 0.0f, 0.0f}, // 10
    {"step.stone", "step.stone", 0.0f, 0.0f}, // 11
    {"step.sand", "step.gravel", 1.0f, 1.0f}, // 12
    {"step.gravel", "step.gravel", 1.0f, 1.0f}, // 13
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 14
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 15
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 16
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 17
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 18
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 19
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 20
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 21
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 22
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 23
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 24
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 25
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 26
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 27
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 28
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 29
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 30
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 31
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 32
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 33
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 34
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 35
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 36
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 37
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 38
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 39
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 40
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 41
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 42
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 43
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 44
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 45
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 46
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 47
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 48
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 49
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 50
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 51
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 52
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 53
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 54
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 55
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 56
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 57
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 58
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 59
    {"step.gravel", "step.gravel", 1.0f, 1.0f}, // 60
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 61
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 62
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 63
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 64
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 65
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 66
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 67
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 68
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 69
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 70
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 71
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 72
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 73
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 74
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 75
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 76
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 77
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 78
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 79
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 80
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 81
    {"step.gravel", "step.gravel", 1.0f, 1.0f}, // 82
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 83
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 84
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 85
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 86
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 87
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 88
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 89
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 90
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 91
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 92
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 93
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 94
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 95
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 96
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 97
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 98
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 99
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 100
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 101
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 102
    {"step.gravel", "step.gravel", 1.0f, 1.0f}, // 103
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 104
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 105
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 106
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
};
