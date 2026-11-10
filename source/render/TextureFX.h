#pragma once
// The terrain atlas's moving tiles, worked out every tick like Beta's TextureFX classes:
// still and flowing water and lava (TextureWaterFX, TexureWaterFlowFX, TextureLavaFX,
// TextureLavaFlowFX), fire (TextureFlamesFX 0 and 1) and the Nether portal (TexturePortalFX).
#include <cstdint>
#include <functional>

namespace texfx {
// one game tick; upload(tile, rgba) is called for each 16x16 tile that changed (RGBA bytes, rows top first)
void tick(const std::function<void(int tile, const uint8_t* rgba)>& upload);
// TextureCompassFX / TextureWatchFX on gui/items.png: the needle swings round to the spawn and
// the dial turns with the sun (both spin at random in the Nether)
struct ItemView { double px = 0, pz = 0; float yaw = 0; int spawnX = 0, spawnZ = 0; float celestial = 0; bool nether = false; bool inWorld = false; };
void tickItems(const ItemView& v, const std::function<void(int tile, const uint8_t* rgba)>& upload);
} // namespace texfx
