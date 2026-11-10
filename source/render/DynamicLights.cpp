#include "DynamicLights.h"
#include <cmath>
#include <cstdlib>
#include "world/BlockTable.h"

namespace dynlight {
namespace {
struct Source { const void* key; int x, y, z, level, builtX, builtY, builtZ, builtLevel; };
std::vector<Source> sources;
int ticks = 0;
int fl(double v) { int i = int(v); return v < double(i) ? i - 1 : i; }

void rebuildAround(Source& s, const std::function<void(int, int, int, int, int, int)>& dirty) {
    int r = std::max(s.level, s.builtLevel);
    if (s.builtLevel > 0) dirty(s.builtX - r, s.builtY - r, s.builtZ - r, s.builtX + r, s.builtY + r, s.builtZ + r);
    if (s.level > 0) dirty(s.x - r, s.y - r, s.z - r, s.x + r, s.y + r, s.z + r);
    s.builtX = s.x;
    s.builtY = s.y;
    s.builtZ = s.z;
    s.builtLevel = s.level;
}
} // namespace

int itemLight(int id) {
    if (id > 0 && id < 256) return BLOCKS[id].light; // Block.lightValue
    if (id == 327) return 15;                         // a lava bucket
    return 0;
}

void tick(int mode, const std::vector<Emitter>& now, const std::function<void(int, int, int, int, int, int)>& dirty) {
    if (mode == OFF) {
        for (Source& s : sources) {
            s.level = 0;
            rebuildAround(s, dirty);
        }
        sources.clear();
        return;
    }
    ++ticks;
    if (mode == FAST && ticks % 5 != 0) return;
    std::vector<Source> next;
    for (const Emitter& e : now) {
        if (e.level <= 0) continue;
        Source s{e.key, fl(e.x), fl(e.y), fl(e.z), std::min(15, e.level), 0, 0, 0, 0};
        for (const Source& o : sources)
            if (o.key == e.key) { s.builtX = o.builtX; s.builtY = o.builtY; s.builtZ = o.builtZ; s.builtLevel = o.builtLevel; }
        next.push_back(s);
    }
    for (Source& o : sources) { // lights that went out
        bool still = false;
        for (const Source& s : next) still |= s.key == o.key;
        if (!still) {
            o.level = 0;
            rebuildAround(o, dirty);
        }
    }
    sources = next;
    for (Source& s : sources) // lights that moved or changed
        if (s.x != s.builtX || s.y != s.builtY || s.z != s.builtZ || s.level != s.builtLevel) rebuildAround(s, dirty);
}

bool any() { return !sources.empty(); }

int lightAt(int x, int y, int z) {
    int best = 0;
    for (const Source& s : sources) {
        int v = s.level - (std::abs(x - s.x) + std::abs(y - s.y) + std::abs(z - s.z));
        if (v > best) best = v;
    }
    return best;
}
} // namespace dynlight
