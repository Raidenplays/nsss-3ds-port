#include "BlockProps.h"

const BlockProps BP[256] = {
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::Stone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 1 stone
    {BC::Grass, Mat::ground, 255, 0, 10, true, true, false, true, true}, // 2 grass
    {BC::Dirt, Mat::ground, 255, 0, 10, true, true, false, false, true}, // 3 dirt
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 4 cobblestone
    {BC::Planks, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 5 planks
    {BC::Sapling, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 6 sapling
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 7 bedrock
    {BC::Flowing, Mat::water, 3, 0, 5, false, false, false, true, false}, // 8 waterStill
    {BC::Stationary, Mat::water, 3, 0, 5, false, false, false, false, false}, // 9 waterMoving
    {BC::Flowing, Mat::lava, 255, 15, 30, false, false, false, true, false}, // 10 lavaStill
    {BC::Stationary, Mat::lava, 255, 15, 30, false, false, false, true, false}, // 11 lavaMoving
    {BC::Sand, Mat::sand, 255, 0, 3, true, true, false, false, true}, // 12 sand
    {BC::Gravel, Mat::sand, 255, 0, 3, true, true, false, false, true}, // 13 gravel
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 14 oreGold
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 15 oreIron
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 16 oreCoal
    {BC::Log, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 17 wood
    {BC::Leaves, Mat::leaves, 1, 0, 10, false, true, false, true, false}, // 18 leaves
    {BC::Sponge, Mat::sponge, 255, 0, 10, true, true, false, false, true}, // 19 sponge
    {BC::Glass, Mat::glass, 0, 0, 10, false, true, false, false, false}, // 20 glass
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 21 oreLapis
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 22 blockLapis
    {BC::Dispenser, Mat::rock, 255, 0, 4, true, true, true, false, true}, // 23 dispenser
    {BC::SandStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 24 sandStone
    {BC::Note, Mat::wood, 255, 0, 10, true, true, true, false, true}, // 25 musicBlock
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::TallGrass, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 31 tallGrass
    {BC::DeadBush, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 32 deadBush
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::Cloth, Mat::cloth, 255, 0, 10, true, true, false, false, true}, // 35 cloth
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::Flower, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 37 plantYellow
    {BC::Flower, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 38 plantRed
    {BC::Mushroom, Mat::plants, 0, 1, 10, false, false, false, true, false}, // 39 mushroomBrown
    {BC::Mushroom, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 40 mushroomRed
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 41 blockGold
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 42 blockSteel
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 43 stairDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 44 stairSingle
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 45 brick
    {BC::TNT, Mat::tnt, 255, 0, 10, true, true, false, false, true}, // 46 tnt
    {BC::Bookshelf, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 47 bookShelf
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 48 cobblestoneMossy
    {BC::Obsidian, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 49 obsidian
    {BC::Torch, Mat::circuits, 0, 14, 10, false, false, false, true, false}, // 50 torchWood
    {BC::Fire, Mat::fire, 0, 15, 10, false, false, false, true, false}, // 51 fire
    {BC::MobSpawner, Mat::rock, 0, 0, 10, false, true, true, false, false}, // 52 mobSpawner
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 53 stairCompactPlanks
    {BC::Chest, Mat::wood, 255, 0, 10, true, true, true, false, true}, // 54 crate
    {BC::RedstoneWire, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 55 redstoneWire
    {BC::Ore, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 56 oreDiamond
    {BC::OreBlock, Mat::iron, 255, 0, 10, true, true, false, false, true}, // 57 blockDiamond
    {BC::Workbench, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 58 workbench
    {BC::Crops, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 59 crops
    {BC::Soil, Mat::ground, 255, 0, 10, false, false, false, true, false}, // 60 tilledField
    {BC::Furnace, Mat::rock, 255, 0, 10, true, true, true, false, true}, // 61 stoneOvenIdle
    {BC::Furnace, Mat::rock, 255, 13, 10, true, true, true, false, true}, // 62 stoneOvenActive
    {BC::Sign, Mat::wood, 0, 0, 10, false, false, true, false, false}, // 63 signPost
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 64 doorWood
    {BC::Ladder, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 65 ladder
    {BC::MinecartTrack, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 66 minecartTrack
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 67 stairCompactCobblestone
    {BC::Sign, Mat::wood, 0, 0, 10, false, false, true, false, false}, // 68 signWall
    {BC::Lever, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 69 lever
    {BC::PressurePlate, Mat::rock, 0, 0, 20, false, false, false, true, false}, // 70 pressurePlateStone
    {BC::Door, Mat::iron, 0, 0, 10, false, false, false, false, false}, // 71 doorSteel
    {BC::PressurePlate, Mat::rock, 0, 0, 20, false, false, false, true, false}, // 72 pressurePlatePlanks
    {BC::RedstoneOre, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 73 oreRedstone
    {BC::RedstoneOre, Mat::rock, 255, 9, 30, true, true, false, true, true}, // 74 oreRedstoneGlowing
    {BC::RedstoneTorch, Mat::circuits, 0, 0, 2, false, false, false, true, false}, // 75 torchRedstoneIdle
    {BC::RedstoneTorch, Mat::circuits, 0, 7, 2, false, false, false, true, false}, // 76 torchRedstoneActive
    {BC::Button, Mat::circuits, 0, 0, 20, false, false, false, true, false}, // 77 button
    {BC::Snow, Mat::snow, 0, 0, 10, false, false, false, true, false}, // 78 snow
    {BC::Ice, Mat::ice, 3, 0, 10, false, true, false, true, false}, // 79 ice
    {BC::SnowBlock, Mat::builtSnow, 255, 0, 10, true, true, false, true, true}, // 80 blockSnow
    {BC::Cactus, Mat::cactus, 0, 0, 10, false, false, false, true, false}, // 81 cactus
    {BC::Clay, Mat::clay, 255, 0, 10, true, true, false, false, true}, // 82 blockClay
    {BC::Reed, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 83 reed
    {BC::JukeBox, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 84 jukebox
    {BC::Fence, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 85 fence
    {BC::Pumpkin, Mat::pumpkin, 255, 0, 10, true, true, false, true, true}, // 86 pumpkin
    {BC::BloodStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 87 bloodStone
    {BC::SlowSand, Mat::sand, 255, 0, 10, true, true, false, false, true}, // 88 slowSand
    {BC::LightStone, Mat::glass, 255, 15, 10, true, true, false, false, true}, // 89 lightStone
    {BC::Portal, Mat::portal, 0, 11, 10, false, false, false, false, false}, // 90 portal
    {BC::Pumpkin, Mat::pumpkin, 255, 15, 10, true, true, false, true, true}, // 91 pumpkinLantern
    {BC::Cake, Mat::cake, 0, 0, 10, false, false, false, true, false}, // 92 cake
    {BC::PowerRepeater, Mat::circuits, 0, 0, 10, false, false, false, false, false}, // 93 powerRepeater
    {BC::PowerRepeater, Mat::circuits, 0, 7, 10, false, false, false, false, false}, // 94 powerRepeaterLit
    {BC::LapisWire, Mat::circuits, 0, 0, 10, false, false, false, true, false}, // 95 lapisWire
    {BC::LapisTorch, Mat::circuits, 0, 7, 2, false, false, false, true, false}, // 96 lapisTorch
    {BC::LapisTorch, Mat::circuits, 0, 0, 2, false, false, false, true, false}, // 97 lapisTorchOff
    {BC::LapisLamp, Mat::glass, 255, 0, 10, true, true, false, false, true}, // 98 lapisLamp
    {BC::LapisLamp, Mat::glass, 255, 15, 10, true, true, false, false, true}, // 99 lapisLampLit
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 100 smoothStone
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 101 charredCobblestone
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 102 charredStone
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 103 charredSmoothStone
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 104 stoneStairs
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 105 charredStoneStairs
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 106 charredSmoothStoneStairs
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 107 charredCobblestoneStairs
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 108 smoothStoneStairs
    {BC::Plain, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 109 charredCobblestoneMossy
    {BC::Stonecutter, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 110 stonecutter
    {BC::MagicLight, Mat::circuits, 0, 15, 600, false, false, false, true, false}, // 111 magicLight
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 112 sandStoneStairs
    {BC::DoublePlant, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 113 doublePlant
    {BC::AxeolonFlower, Mat::plants, 0, 7, 10, false, false, false, true, false}, // 114 manabloom
    {BC::AxeolonFlower, Mat::plants, 0, 10, 10, false, false, false, true, false}, // 115 wispLily
    {BC::AxeolonFlower, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 116 axeolonFlowers
    {BC::CharredLog, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 117 charredLog
    {BC::Planks, Mat::wood, 255, 0, 10, true, true, false, false, true}, // 118 charredPlanks
    {BC::Sapling, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 119 charredSapling
    {BC::Step, Mat::wood, 255, 0, 10, true, true, false, false, false}, // 120 woodSlabDouble
    {BC::Step, Mat::wood, 255, 0, 10, false, false, false, false, false}, // 121 woodSlab
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 122 spruceStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 123 birchStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 124 drogwoodStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 125 charredOakStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 126 charredSpruceStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 127 charredBirchStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 128 charredDrogwoodStairs
    {BC::Step, Mat::wood, 255, 0, 10, true, true, false, false, false}, // 129 woodSlab2Double
    {BC::Step, Mat::wood, 255, 0, 10, false, false, false, false, false}, // 130 woodSlab2
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 131 thramwoodStairs
    {BC::Stairs, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 132 charredThramwoodStairs
    {BC::MixedSlab, Mat::rock, 255, 0, 10, true, false, true, false, true}, // 133 mixedSlab
    {BC::AxeolonFlower, Mat::plants, 0, 7, 10, false, false, false, true, false}, // 134 duskpetalOpen
    {BC::DoublePlant, Mat::plants, 0, 10, 10, false, false, false, true, false}, // 135 doublePlantGlow
    {BC::DoublePlant, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 136 doublePlant2
    {BC::FlowerPot, Mat::circuits, 0, 0, 10, false, false, true, false, false}, // 137 flowerPot
    {BC::FlowerPot, Mat::circuits, 0, 0, 10, false, false, true, false, false}, // 138 bigFlowerPot
    {BC::FlowerPot, Mat::circuits, 0, 7, 10, false, false, true, false, false}, // 139 flowerPotLit
    {BC::FlowerPot, Mat::circuits, 0, 10, 10, false, false, true, false, false}, // 140 bigFlowerPotLit
    {BC::Cattail, Mat::plants, 0, 0, 10, false, false, false, true, false}, // 141 cattail
    {BC::Crate, Mat::wood, 255, 0, 10, true, true, true, false, true}, // 142 storageCrate
    {BC::SpawnCrystal, Mat::rock, 255, 11, 10, true, true, true, false, true}, // 143 spawnCrystal
    {BC::Icicle, Mat::ice, 0, 0, 10, false, false, false, false, false}, // 144 icicle
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 145 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 146 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 147 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 148 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 149 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 150 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 151 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 152 ?
    {BC::Door, Mat::wood, 0, 0, 10, false, false, false, false, false}, // 153 ?
    {BC::PressurePlate, Mat::rock, 0, 0, 20, false, false, false, true, false}, // 154 charredPressurePlate
    {BC::IceBricks, Mat::ice, 3, 0, 10, false, true, false, false, false}, // 155 iceBricks
    {BC::Frozen, Mat::rock, 255, 0, 30, true, true, false, true, true}, // 156 compactedIce
    {BC::Frozen, Mat::builtSnow, 255, 0, 30, true, true, false, false, true}, // 157 snowBricks
    {BC::Step, Mat::ice, 3, 0, 10, false, true, false, false, false}, // 158 iceSlabDouble
    {BC::Step, Mat::ice, 3, 0, 10, false, false, false, false, false}, // 159 iceSlab
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 160 compactedIceSlabDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 161 compactedIceSlab
    {BC::Step, Mat::builtSnow, 255, 0, 10, true, true, false, false, false}, // 162 snowSlabDouble
    {BC::Step, Mat::builtSnow, 255, 0, 10, false, false, false, false, false}, // 163 snowSlab
    {BC::Stairs, Mat::ice, 3, 0, 10, false, false, false, false, false}, // 164 iceStairs
    {BC::Stairs, Mat::ice, 3, 0, 10, false, false, false, false, false}, // 165 iceBrickStairs
    {BC::Stairs, Mat::rock, 0, 0, 30, false, false, false, false, false}, // 166 compactedIceStairs
    {BC::Stairs, Mat::rock, 0, 0, 30, false, false, false, false, false}, // 167 compactedIceBrickStairs
    {BC::Stairs, Mat::builtSnow, 0, 0, 10, false, false, false, false, false}, // 168 snowStairs
    {BC::Stairs, Mat::builtSnow, 0, 0, 30, false, false, false, false, false}, // 169 snowBrickStairs
    {BC::BonePile, Mat::plants, 0, 0, 10, false, false, false, false, false}, // 170 bonePile
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 171 ?
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 172 ?
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 173 ?
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 174 ?
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 175 ?
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 176 ?
    {BC::AxeolonStone, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 177 ?
    {BC::StoneVariant, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 178 stoneCobble
    {BC::StoneVariant, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 179 stonePolished
    {BC::StoneVariant, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 180 stoneBricks
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 181 axStoneSlabDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 182 axStoneSlab
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 183 axCobbleSlabDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 184 axCobbleSlab
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 185 axPolishedSlabDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 186 axPolishedSlab
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 187 axBrickSlabDouble
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 188 axBrickSlab
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 189 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 190 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 191 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 192 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 193 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 194 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 195 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 196 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 197 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 198 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 199 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 200 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 201 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 202 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 203 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 204 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 205 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 206 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 207 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 208 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 209 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 210 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 211 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 212 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 213 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 214 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 215 ?
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 216 ?
    {BC::OreVariant, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 217 ?
    {BC::OreVariant, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 218 ?
    {BC::OreVariant, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 219 ?
    {BC::OreVariant, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 220 ?
    {BC::OreVariant, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 221 ?
    {BC::OreVariant, Mat::rock, 255, 0, 30, true, true, false, false, true}, // 222 ?
    {BC::OreVariant, Mat::rock, 255, 9, 30, true, true, false, true, true}, // 223 oreVariantRedstoneLit
    {BC::PowerBlock, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 224 blockRedstone
    {BC::PowerBlock, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 225 lapisBricks
    {BC::PowerBlock, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 226 redstoneBricks
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 227 lapisBrickStairs
    {BC::Stairs, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 228 redstoneBrickStairs
    {BC::Step, Mat::rock, 255, 0, 10, false, false, false, false, false}, // 229 powerBrickSlab
    {BC::Step, Mat::rock, 255, 0, 10, true, true, false, false, false}, // 230 powerBrickSlabDouble
    {BC::PowerBlock, Mat::rock, 255, 0, 10, true, true, false, false, true}, // 231 blockCoal
    {BC::Dyed, Mat::glass, 255, 15, 10, true, true, false, false, true}, // 232 colouredGlowstone
    {BC::TintedGlass, Mat::glass, 0, 0, 10, false, true, false, false, false}, // 233 tintedGlass
    {BC::LapisWire, Mat::circuits, 0, 0, 10, false, false, false, true, false}, // 234 ?
    {BC::LapisWire, Mat::circuits, 0, 0, 10, false, false, false, true, false}, // 235 ?
    {BC::LapisWire, Mat::circuits, 0, 0, 10, false, false, false, true, false}, // 236 ?
    {BC::LapisWire, Mat::circuits, 0, 0, 10, false, false, false, true, false}, // 237 ?
    {BC::PistonBase, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 238 piston
    {BC::PistonBase, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 239 pistonSticky
    {BC::PistonHead, Mat::rock, 0, 0, 10, false, false, false, false, false}, // 240 pistonHead
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::None, Mat::air, 0, 0, 0, false, false, false, false, false},
    {BC::Hopper, Mat::iron, 0, 0, 10, false, false, true, false, false}, // 243 hopper
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
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 8
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 9
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 10
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 11
    {"step.sand", "step.gravel", 1.0f, 1.0f}, // 12
    {"step.gravel", "step.gravel", 1.0f, 1.0f}, // 13
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 14
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 15
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 16
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 17
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 18
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 19
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 20
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 21
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 22
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 23
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 24
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 25
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 31
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 32
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 35
    {"step.stone", "step.stone", 1.0f, 1.0f},
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
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 86
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 87
    {"step.sand", "step.gravel", 1.0f, 1.0f}, // 88
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 89
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 90
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 91
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 92
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 93
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 94
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 95
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 96
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 97
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 98
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 99
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 100
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 101
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 102
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 103
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 104
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 105
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 106
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 107
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 108
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 109
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 110
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 111
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 112
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 113
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 114
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 115
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 116
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 117
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 118
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 119
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 120
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 121
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 122
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 123
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 124
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 125
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 126
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 127
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 128
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 129
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 130
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 131
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 132
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 133
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 134
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 135
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 136
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 137
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 138
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 139
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 140
    {"step.grass", "step.grass", 1.0f, 1.0f}, // 141
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 142
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 143
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 144
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 145
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 146
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 147
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 148
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 149
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 150
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 151
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 152
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 153
    {"step.wood", "step.wood", 1.0f, 1.0f}, // 154
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 155
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 156
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 157
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 158
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 159
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 160
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 161
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 162
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 163
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 164
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 165
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 166
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 167
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 168
    {"step.cloth", "step.cloth", 1.0f, 1.0f}, // 169
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 170
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 171
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 172
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 173
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 174
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 175
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 176
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 177
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 178
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 179
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 180
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 181
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 182
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 183
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 184
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 185
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 186
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 187
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 188
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 189
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 190
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 191
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 192
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 193
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 194
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 195
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 196
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 197
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 198
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 199
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 200
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 201
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 202
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 203
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 204
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 205
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 206
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 207
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 208
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 209
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 210
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 211
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 212
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 213
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 214
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 215
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 216
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 217
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 218
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 219
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 220
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 221
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 222
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 223
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 224
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 225
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 226
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 227
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 228
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 229
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 230
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 231
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 232
    {"step.stone", "random.glass", 1.0f, 1.0f}, // 233
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 234
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 235
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 236
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 237
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 238
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 239
    {"step.stone", "step.stone", 1.0f, 1.0f}, // 240
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.0f},
    {"step.stone", "step.stone", 1.0f, 1.5f}, // 243
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
