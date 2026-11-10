#pragma once
// The atlas's moving tiles, worked out every tick like the jar's TextureFX classes: still and flowing
// water and lava (TextureWaterFX, TexureWaterFlowFX, TextureLavaFX, TextureLavaFlowFX), fire
// (TextureFlamesFX 0 and 1), gears (TextureGearFX, TextureGearIdleFX) and the season's leaves
// (TextureLeavesFX), and on gui/items.png the compass (TextureCompassFX).
#include <cstdint>
#include <functional>

namespace texfx {
// one game tick; upload(tile, rgba) is called for each 16x16 tile that changed (RGBA bytes, rows top
// first). season: World.getCurrentSeason (0 summer, 1 fall, 2 winter, 3 spring; -1 no world),
// seasonProgress: World.getSeasonProgress.
void tick(int season, float seasonProgress, const std::function<void(int tile, const uint8_t* rgba)>& upload);
struct ItemView {
    double px = 0, pz = 0;
    float yaw = 0;
    int spawnX = 0, spawnZ = 0;
    bool measuring = false; // EntityPlayer.isMeasuringBlock: the needle points at measureX, measureZ
    int measureX = 0, measureZ = 0;
    bool inWorld = false;
};
void tickItems(const ItemView& v, const std::function<void(int tile, const uint8_t* rgba)>& upload);
} // namespace texfx
