#pragma once
// AxeolonDynamicLights (Video Settings > Dynamic Lights: Off / Fast / Fancy, off by default):
// a torch (or anything that glows) in your hand lights up what's round you. Chunks bake their
// light when they're built, so the moving light is mixed in while a section is meshed, and only
// the sections round a light that moved get rebuilt (Fast: a few times a second; Fancy: every tick,
// and glowing items on the ground and burning things light up too).
#include <functional>
#include <vector>

namespace dynlight {
enum Mode { OFF = 0, FAST = 1, FANCY = 2 };
struct Emitter { const void* key; double x, y, z; int level; };

// once a game tick: what's glowing now (key: who it is, so a light can be followed as it moves);
// markDirty(x0, y0, z0, x1, y1, z1) is World.markBlocksDirty
void tick(int mode, const std::vector<Emitter>& now, const std::function<void(int, int, int, int, int, int)>& markDirty);
bool any();
int lightAt(int x, int y, int z); // the moving light at a block (0 if none): one level less per block stepped
int itemLight(int id);            // the light an item gives held or dropped
} // namespace dynlight
