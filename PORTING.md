# Porting plan — Minecraft Beta 1.2_02 → 3DS (C++ / citro3d)

Reference source: your RetroMCP b1.2_02 workspace (`C:\Dev\...`).
Engine helpers worth borrowing (not copying wholesale): ReMinecraftPE (C++, Beta-era).
Target hardware: **Old 3DS at 60 fps** (268 MHz ARM11, 64 MB for the app, no C-stick/ZL/ZR).
New 3DS runs the same build with headroom. Every phase below has to fit the Old 3DS budget.

Rule of thumb: port **behaviour** faithfully, and **rewrite the platform layers** (rendering,
input, sound, files). Anything under `net.minecraft.src` that calls LWJGL/OpenGL/AWT gets rewritten.
Anything that is pure logic gets translated almost line by line.

---

## Phase 0 — Skeleton ✅ (this commit)
- devkitPro Makefile, PICA200 vertex shader, citro3d renderer
- 16×128×16 chunks with Beta's `x << 11 | z << 7 | y` layout
- Section mesher (16³) with Beta's face shading (1.0 / 0.5 / 0.8 / 0.6)
- `JavaRandom` — bit-exact `java.util.Random` (verified against the JVM)
- Placeholder terrain atlas in Beta's `terrain.png` layout
- Free-fly camera, debug console on the bottom screen

## Phase 0.5 — Two screens ✅
- Top: gameplay + crosshair. Bottom: hotbar + Map / Items / Almanac / Menu tabs (citro2d)
- Almanac map (explored-only, waypoints, zoom), inventory with Axeolon charm row
- Block breaking/placing, ray cast, frustum culling, face-group culling, compact vertices

## Old 3DS rules (apply to every phase)
- **One frame = 16.6 ms.** Anything slow is spread across frames (rebuild queue) or moved to
  the second CPU core (`APT_SetAppCpuTimeLimit` + `threadCreate` on core 1). Chunk generation,
  lighting and saving go there when they're ported.
- **Memory:** 64 MB total. A Beta chunk is 80 KB (blocks + 3 nibble arrays). Keep about 7×7
  chunks loaded (≈4 MB); Axeolon's 256 height doubles that.
- **Render distance** defaults to 56 blocks with fog to hide the edge (Beta's "Short" was 64).
- **No per-frame allocation** in the game loop (no `new`, no growing vectors).
- **Mobs:** cap the active count and update far mobs less often. Beta's `PathFinder` is
  expensive, so give it a per-frame budget.
- Measure with the Menu tab's fps/triangle readout **on real Old 3DS hardware**.

## Phase 1 — Core utilities (small, everything depends on them)
| Java class | Notes |
|---|---|
| `MathHelper` | **Port the 65536-entry sin table exactly** — worldgen and mob movement use it |
| `java.util.Random` | ✅ done (`JavaRandom.h`) |
| `AxisAlignedBB`, `Vec3D` | Beta pools these objects; use plain stack values instead |
| `MovingObjectPosition`, `EnumMovingObjectType` | raycast results |

## Phase 2 — World data and generation (makes it look like *Beta*)
| Java class | Notes |
|---|---|
| `Block` + subclasses | Start with a table of id → properties; add behaviours (`onBlockAdded`, `updateTick`…) as virtual methods |
| `Material`, `StepSound` | data only |
| `NibbleArray` | metadata + light; easy |
| `Chunk` | add metadata, skylight, blocklight, heightMap |
| `NoiseGeneratorPerlin`, `NoiseGeneratorOctaves` | must match **exactly** (seed parity) |
| `ChunkProviderGenerate` | terrain, `replaceBlocksForBiome`, caves |
| `MapGenCaves` | |
| `WorldChunkManager`, `BiomeGenBase`, `NoiseGeneratorOctaves2` | biomes + grass/foliage colours |
| `WorldGenTrees`, `WorldGenBigTree`, `WorldGenMinable`, `WorldGenFlowers`, `WorldGenReed`, `WorldGenClay`, `WorldGenDungeons`, `WorldGenLakes`, `WorldGenCactus`, `WorldGenLiquids` | populate step |
| `World` | getBlockId/setBlock, chunk provider, ticking |
| `ChunkProviderLoadOrGenerate` / `ChunkProvider` | streaming chunks around the player |

✔ Milestone: same seed as the Java jar produces the same world.

## Phase 3 — Lighting
| Java class | Notes |
|---|---|
| `EnumSkyBlock`, `MetadataChunkBlock` | Beta's light propagation queue |
| `World.updateLightByType`, `Chunk.generateSkylightMap` | |

Then pass light into the mesher (Beta uses `getLightBrightness` per face, no smooth lighting
in 1.2). This is the first big CPU cost — budget it per frame.

## Phase 4 — Rendering
| Java class | 3DS replacement |
|---|---|
| `Tessellator` | `MeshData` builder (already started) |
| `RenderBlocks` | port shape code: torches, crosses (flowers/saplings), stairs, fences, doors, rails, liquids, cactus, ladders |
| `WorldRenderer` | `GpuMesh` per section ✅; add 2 passes (opaque + water) |
| `RenderGlobal` | section dirty-marking + rebuild queue (rebuild a few per frame) |
| `Frustrum`, `ClippingHelperImplementation` | replace distance culling |
| `EntityRenderer` | fog (citro3d FogLut), FOV, view bobbing, sky + clouds |
| `RenderEngine`, `TextureFX` (water/lava/fire animation) | load `romfs:/textures/terrain.png` (from *your* jar), animated tiles via sub-uploads |
| `FontRenderer` | `default.png` bitmap font on the bottom screen |

## Phase 5 — Player, physics, interaction
| Java class | Notes |
|---|---|
| `Entity`, `EntityLiving`, `EntityPlayer`, `EntityPlayerSP` | `moveEntity` collision is the key part — port exactly |
| `MovementInput` | Circle Pad → `moveForward/moveStrafe`, B jump, sneak toggle |
| `PlayerController`, `PlayerControllerSP` | breaking times, placing |
| `Item`, `ItemBlock`, `ItemTool`…, `ItemStack` | |
| `InventoryPlayer` | hotbar on top screen, full inventory on bottom (touch) |

✔ Milestone: survival walking, mining and placing.

## Phase 6 — Saving ✅ (player, inventory incl. battle slots/charms, waypoints, chunks, Almanac map)
| Java class | Notes |
|---|---|
| `NBTBase` + tags, `CompressedStreamTools` | zlib/gzip from devkitPro portlibs (`3ds-zlib`) |
| `ChunkLoader`, `SaveHandler` | **b1.2 uses the old Alpha format** (one file per chunk, base36 folders). McRegion only arrived in b1.3. Keep the format so saves copy to/from PC |

Saves go to `sdmc:/3ds/axeolon3ds/saves/`.

## Phase 7 — GUI (bottom screen: framework already exists in `ui/BottomScreen`)
Crafting tables, furnaces and chests open as extra bottom-screen tabs (the top screen keeps
playing, as in Beta single-player). Port the logic of
`GuiInventory`, `GuiCrafting`, `GuiFurnace`, `GuiChest`,
`CraftingManager` + `Recipes*`, `FurnaceRecipes`, `TileEntity*` (chest, furnace, sign, mob spawner).

## Phase 8 — Mobs (models + animations ✅: source/model, game/Mobs; AI, spawning, combat to do)
`EntityAnimal`, `EntityMob`, `EntityPig/Cow/Sheep/Chicken/Zombie/Skeleton/Creeper/Spider/Slime`,
`PathFinder` + `PathEntity`, `SpawnerAnimals`. Rendering: `ModelBase`/`ModelRenderer` →
pre-built per-part meshes animated with a matrix per part (the 3DS handles this well).

## Phase 9 — Sound
`SoundManager` → `ndsp` + Tremor (`3ds-libvorbisidec`) for the .ogg files. Music streams from romfs.

## Phase 10 — Axeolon
Apply Axeolon's changes **on top of the vanilla port**, in this order:
1. World height 256 (change `Chunk::HEIGHT`, the `<< 7` shift becomes `<< 8`, and the save format to match)
2. Charm slot row (UI slots already exist) + loot-only charms + spawn-setting item
   → also port the Almanac's Achievements / The Hunt / Recipes pages into the Almanac tab
3. New structures and loot tables
4. New mobs, bosses (Tabish) and Blockbench keyframed animations
   → write an exporter that converts `.bbmodel` animations to a compact binary in romfs

Performance watch: 256 height doubles chunk memory and meshing time.

---

## Java → C++ gotchas
- **Integer overflow:** Java wraps; signed overflow in C++ is undefined. For hash/seed math, do it in
  `uint32_t`/`uint64_t` and cast back (see `JavaRandom.h`).
- **`>>>`** → shift on the unsigned type. **`>>`** on negatives is arithmetic in both (GCC).
- **`long` is 64-bit** in Java; use `int64_t`, never `long` (32-bit on ARM).
- **`(int)` casts of floats** truncate toward zero in both; Beta often uses `MathHelper.floor_double` — keep it.
- **`char`** in Java is 16-bit.
- **Float maths:** keep `float` vs `double` exactly as Java does, or worldgen drifts. The 3DS has
  hardware float but doubles are slower — profile before changing anything.
- **Garbage collection:** Beta allocates constantly (`Vec3D`, `AxisAlignedBB`, `ArrayList`). Use
  stack values and reuse vectors.
- **`HashMap<Long,...>` chunk maps** → `std::unordered_map<int64_t,...>` or a fixed ring buffer.

## Legal
Code translated from decompiled Mojang code is a derivative work. Fine for personal use;
**keep the repo private** and never ship Mojang textures or sounds. Users supply their own
`minecraft.jar` assets. The Axeolon-only code and art are yours.

## Blockbench models (done)
- `tools/bbmodel_convert.py` reads every /axeolon/{mob,block}/*.bbmodel like AxeolonBBModel/AxeolonBlockRig
  and writes romfs `.axm`. `source/model/AxmModel` ports sampling, blending, Box-UV cubes, bone transforms
  and the block rig maths; `Animator` ports AxeolonAnimator; `BlockModels` ports AxeolonBlockAnims
  (describe / drawStill / play / tick / hides / render); `game/Mobs` ports AxeolonMobAnims + RenderLiving.
- Verify after changing models: `java ... net.minecraft.src.CheckModels <axeolon dir> java.txt`, the host
  tool `hosttest/models.cpp` writes the same dump from the .axm files; compare (0 mismatches today).
- Block model textures are packed into terrain.rgba rows 512+ (RigAtlas.cpp, generated).
