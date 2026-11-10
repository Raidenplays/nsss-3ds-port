# NSSS 3DS

A C++ port of **Not So Seecret Saturday** (DirtPiper's fork of Minecraft Alpha 1.1.2_01, version
a1.1.12) to the Nintendo 3DS, using devkitPro + citro3d/citro2d, with the MCPE 0.5.0 look on top.
See **PORTING.md** for the roadmap and what NSSS adds.

Personal build, not affiliated with Mojang or the NSSS author. NSSS's source has no license, so
builds aren't shared without DirtPiper's permission.

**Target: 60 fps on an Old 3DS.**

## Status
- ✅ World generation (terrain, beaches, caves) matches the PC game block for block for the same seed
  (`tools/host/alphagen.cpp` vs `tools/nsss/AlphaGenDump.java`).
- ✅ Population, block behaviour (fluids, redstone, gears, random ticks) and chunk meshing match the
  jar byte for byte (`tools/host/parity.sh`).
- ⏳ Items, player, mobs, weather and the other world types: see PORTING.md.

## Files on the SD card
- Worlds: `sdmc:/3ds/nsss3ds/NSSSsaves/World1..5/` (the jar's own layout: copy worlds from the
  instance's `NSSSsaves` folder; keep a backup)
- Options: `sdmc:/3ds/nsss3ds/options.txt`

## Building
GitHub Actions builds `nsss3ds.3dsx` and `.cia` on every push, then runs it in the Azahar emulator
and keeps screenshots. Locally: `make` with devkitARM, libctru, citro3d and citro2d installed.
