#pragma once
// AxeolonBlockAnims + AxeolonBlockRig on the 3DS: every "model block" (doors, chests, crates,
// furnaces, dispensers, note blocks, levers, buttons, pressure plates, pistons, hoppers) is drawn
// from its Blockbench model, still or animating, exactly as the Java mod decides:
//   describe()   which model(s), facing (yaw/pitch) and still pose a block gets
//   drawStill()  the still block, baked into its chunk section (terrain atlas rows 512+)
//   play()...    a keyframed animation; while it plays the block is left out of the chunk
//                (hides()) and drawn every frame by render() instead
#include <cstdint>
#include <string>
#include <vector>
#include "AxmModel.h"
#include "RigAtlas.h"

class World;

namespace BlockModels {

enum Kind { DOOR = 0, LEVER, BUTTON, PLATE, CHEST, CRATE, FURNACE, DISPENSER, NOTE, PISTON };

// Loads romfs:/axeolon/block/<name>.axm on first use. Null if missing.
const axm::Model* rig(const std::string& name);
const RigTexture* rigTexture(const std::string& name); // texture rect in the atlas (christmas aware)

bool isModelBlock(int id);

struct Pose {
    const axm::Model* rigs[2] = {nullptr, nullptr};
    std::string names[2];
    int halves[2] = {0, 0};
    int count = 0;
    float yaw = 0, pitch = 0;
    std::string still; // animation held at its end ("" = the model as made)
    void add(const std::string& name, int half);
};
// false if not a model block. A door's top half / piston head / the far half of a double chest
// give a Pose with count 0 (drawn with the other block).
bool describe(const World& w, int x, int y, int z, Pose& p);

// A quad batch for the still block at (x, y, z): vertices relative to (ox, oy, oz) = where the
// block's corner goes, each run tagged with its texture. Returns false if nothing to draw.
struct Batch { const RigTexture* tex; size_t start, count; };
bool drawStill(const World& w, int x, int y, int z, double ox, double oy, double oz,
               std::vector<axm::ModelVertex>& quads, std::vector<Batch>& batches);

// Light for AxeolonBlockRig.faceLight (World.getLightBrightness)
float lightBrightness(const World& w, int x, int y, int z);

// ---- animations (client side, cosmetic) ----
// Hooks set by the game: mark blocks for re-meshing, ask whether a block's section still waits
// to be rebuilt, and play a sound.
extern void (*markDirty)(int x0, int y0, int z0, int x1, int y1, int z1);
extern bool (*chunkPending)(int x, int y, int z);
extern void (*playSound)(double x, double y, double z, const char* name, float volume, float pitch);

void play(World& w, int kind, int x, int y, int z, const std::string& name);
void door(World& w, int x, int y, int z, int metaBefore);   // a door is about to toggle
void openContainer(World& w, int x, int y, int z);           // chest/crate GUI opened
void holdOpen(World& w, int x, int y, int z);                // a mob pries a lid open
void release(World& w, int x, int y, int z);
void piston(World& w, int x, int y, int z, const std::string& name);
void tick(World& w, bool chestGuiOpen); // every game tick (20/s)
void clear();                            // world changed
bool hides(int x, int y, int z);
bool anyActive();
// Dynamic quads for everything animating this frame, in world coordinates.
void render(const World& w, float partial, std::vector<axm::ModelVertex>& quads, std::vector<Batch>& batches);
int chestFront(const World& w, int x, int y, int z);

} // namespace BlockModels
