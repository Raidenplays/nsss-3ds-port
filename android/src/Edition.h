#pragma once
// Which edition this APK is. Axeolon Lite is a taste of the mod in Pocket Edition Lite's clothes:
// the old Lite title, buttons and pad, one world, and the first part of Axeolon - the cold north,
// the boss and the magic are what the full game is for.
//
//   In Lite                                       Full game only
//   ---------------------------------------------  ------------------------------------------
//   one world (the bundled World1, or a new one)   up to five worlds
//   the overworld, all of Axeolon's terrain:       the Nether (portals stay unlit)
//     stone types, drogwood and thramwood,
//     the Axeolon flowers and plants
//   Axeolon's animals: raccoons, owls, goats,      Tabish, the Bear Queen, and The Hunt
//     dire wolves (tame one with the Wolf Charm)
//   Pocket crafting, the stonecutter, battle mode  spell books and spells, lapis power
//   the Almanac: map, achievements up to           Lucky Mining, Silk Touch, Bear and
//     "Ready for the Cold", recipes                  Spawn charms
//   Glide and Swift charms (dungeon chests)
//
// Locked recipes show in the crafting list with Pocket Edition Lite's demo overlay; the Buy button
// on the title screen points at the full game, which is free.
#include <cstdint>

namespace edition {

#ifdef AXEOLON_LITE
constexpr bool LITE = true;
#else
constexpr bool LITE = false;
#endif

constexpr int MAX_WORLDS = LITE ? 1 : 5;
constexpr bool NETHER = !LITE;
constexpr bool BOSSES = !LITE;
constexpr bool MAGIC = !LITE;       // spell books, spell pages, lapis power
constexpr const char* NAME = LITE ? "Axeolon Lite" : "Minecraft Axeolon";

// Items and blocks only the full game has (shown greyed out in Lite's crafting list)
inline bool fullGameOnly(int id) {
    if (!LITE) return false;
    switch (id) {
    case 1001:                                    // spell book
    case 1010: case 1011: case 1012: case 1013:   // Lucky Mining II-V
    case 1014: case 1039: case 1045:              // Silk Touch, Spawn and Bear charms
    case 96: case 98: case 225: case 226:         // lapis torch, lamp, lapis and redstone bricks
    case 227: case 228: case 229:                 // ... their stairs and slabs
    case 93:                                      // the power repeater (lapis and redstone)
        return true;
    default:
        return false;
    }
}

} // namespace edition
