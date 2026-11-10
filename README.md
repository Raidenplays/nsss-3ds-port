# NSSS 3DS

A C++ port of **Not So Seecret Saturday** (DirtPiper's fork of Minecraft Alpha 1.1.2_01, version
a1.1.12) to the Nintendo 3DS, using devkitPro + citro3d/citro2d. Built on the Axeolon 3DS engine.
See **PORTING.md** for the roadmap and what NSSS adds.

Personal build, not affiliated with Mojang or the NSSS author. NSSS's source has no license, so
builds aren't shared without DirtPiper's permission.

**Target: 60 fps on an Old 3DS.**

## Status
- ✅ World generation (terrain, beaches, caves) matches the PC game block for block for the same seed
  (`tools/host/alphagen.cpp` vs `tools/nsss/AlphaGenDump.java`).
- ⏳ Being converted from the Axeolon engine: right now a build still runs Axeolon content.

## Files on the SD card
- Worlds: `sdmc:/3ds/nsss3ds/saves/<name>/` (copy NSSS worlds from `.minecraft/saves`; keep a backup)
- Options: `sdmc:/3ds/nsss3ds/options.txt`

## Building
GitHub Actions builds `nsss3ds.3dsx` and `.cia` on every push, then runs it in the Azahar emulator
and keeps screenshots. Locally: `make` with devkitARM, libctru, citro3d and citro2d installed.
