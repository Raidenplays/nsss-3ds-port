# Axeolon 3DS

A C++ port of Minecraft Beta 1.2_02 (and later Axeolon) to the Nintendo 3DS,
using devkitPro + citro3d/citro2d. See **PORTING.md** for the roadmap.

**Target: 60 fps on an Old 3DS.** New 3DS gets the same game with extra headroom.

## Status
The whole of Beta 1.2_02 plus Axeolon, playable start to finish. World generation (overworld,
the Peaks, cave biomes, structures and the Nether) matches the Java game block for block and light
for light; the host checks in `tools/host` compare it, and cart, boat and rail behaviour, tick by
tick against the Java classes.

- **Your Java worlds, both ways:** copy a save folder to `sdmc:/3ds/axeolon3ds/saves/<name>/`. Region
  and old chunk files, 128-tall chunks, level.dat recovery, the Nether, your player (inventory, charms,
  battle pairs, waypoints, achievements, mana) and the Almanac map files all load, and everything is
  written back in the same format for the PC.
- **Survival:** digging, crafting (Axeolon's recipe locks too), smelting, armour, food, health and
  mana, every block's behaviour (liquids, fire, redstone and Axeolon's lapis network, pistons,
  dispensers, note blocks, TNT, cake, jukeboxes and discs, doors, signs, paintings, rails), animated
  water, lava, fire and portals, a working compass and clock.
- **Mobs** with Blockbench models and every animation: Beta's mobs (ghasts and their fireballs,
  squid, spider jockeys, pigs to saddle and ride, sheep to dye) and Axeolon's (dire wolves to tame
  and ride, raccoons, owls, goats, ghouls, the Tabish boss fight), spawning by biome and time of day.
- **The Nether:** portals lit with fire, travel both ways with the Teleporter finding or building
  the portal on the other side.
- **Vehicles:** boats in every wood, minecarts (plain, chest, furnace) on rails.
- **Fishing**, spells, spawn crystals, achievements with toasts, the how-to-play hints.
- **Options** (title screen and Menu tab, saved to `options.txt`): music, sound, look speed, render
  distance, difficulty, view bobbing, invert look, dynamic lights.
- **Bottom screen:** hotbar along the bottom; tabs Map (the Almanac map, waypoints), Items (armour,
  charms, crafting or the battle page, backpack, chests), Almanac, Menu.

## Controls
| Old 3DS | New 3DS / Circle Pad Pro | Action |
|---|---|---|
| Circle Pad | Circle Pad | move |
| ABXY | C-stick | look |
| D-pad ↑ | A | jump (swim up) |
| D-pad ↓ | B | sneak (get out of a boat or cart, off a mount) |
| R | ZR | break / attack (hold to dig) |
| L | ZL | use / place (hold with a spell book for the spell picker) |
| D-pad ← → | L / R | hotbar slot (battle mode: weapon pair) |
| | X | Items tab |
| START | START | pause |
| SELECT | SELECT | skip the how-to-play hints |
| Touch | Touch | bottom screen; tap a hotbar slot to select it |

## Building (Windows)
1. Install devkitPro with the **3DS Development** component:
   https://devkitpro.org/wiki/Getting_Started
2. In **MSYS2 (devkitPro)**:
   ```sh
   cd /c/Dev/axeolon3ds
   make
   ```
   If citro2d/citro3d are missing: `dkp-pacman -S 3ds-dev`
   The game data (textures, models, sounds) goes in `romfs/`, already included. To rebuild it from
   your Axeolon install: `python tools/gen_assets.py <exported textures> <jars/assets> <minecraft/src/axeolon> romfs`
   (it also converts the .bbmodel files with `tools/bbmodel_convert.py`).
3. Copy your world to `sdmc:/3ds/axeolon3ds/saves/<name>/`. **Keep a backup on the PC.**
4. Run `axeolon3ds.3dsx` from the Homebrew Launcher, or in Azahar.
   **Test speed on real hardware.** Emulators don't reflect Old 3DS performance.

## Old 3DS performance budget (what's already done)
- 12-byte vertices (1/64-block positions relative to the section; the shader adds the section origin)
- Frustum culling plus render distance (adjustable in Options)
- Face groups: faces pointing away from the camera are skipped before the GPU transforms them
- Meshing reads from a padded local copy of the blocks; section rebuilds get a 5 ms budget per frame
- Model blocks are baked into the chunk mesh from the same atlas; only animating ones draw per frame
- Chunks keep 9x9 around you; the save copies of the big arrays are freed after loading
- Heightmap per chunk (like Beta's), so the map never scans columns
- Bottom screen redraws only when something changes; the map repaints at most 4×/sec, only while visible

The worst view from spawn is about 34k triangles per frame.

## If something's off
- **Looking around goes the wrong way:** Options > Invert look.
- **Too slow:** lower the render distance, and keep dynamic lights off.
- **Bottom screen flickers:** remove `gfxSetDoubleBuffering(GFX_BOTTOM, false)` and set `dirty = true` every frame in `BottomScreen::draw()`.
