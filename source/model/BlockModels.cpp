#include "BlockModels.h"
#include "Paths.h"
#include "render/DynamicLights.h"
#include "render/ChunkMesh.h"
#include "world/BlockTable.h"
#include "world/World.h"
#include <cmath>
#include <ctime>
#include <map>
#include <memory>

namespace BlockModels {

void (*markDirty)(int, int, int, int, int, int) = nullptr;
bool (*chunkPending)(int, int, int) = nullptr;
void (*playSound)(double, double, double, const char*, float, float) = nullptr;

namespace {

// AxeolonWood.FILES
const char* const WOOD[10] = {"oak", "spruce", "birch", "drogwood", "thramwood",
                              "charred_oak", "charred_spruce", "charred_birch", "charred_drogwood", "charred_thramwood"};
inline int clampWood(int t) { return t < 0 || t >= 10 ? 0 : t; }
std::string chestRig(int t) { return clampWood(t) == 0 ? "chest" : std::string("chest_") + WOOD[t]; }
std::string doubleChestRig(int t) { return clampWood(t) == 0 ? "chest_double" : std::string("chest_double_") + WOOD[t]; }
std::string doorRig(int t) { return clampWood(t) == 0 ? "door" : std::string("door_") + WOOD[t]; }

// Block ids (dumped from the Axeolon build by tools/java/DumpModelBlocks)
constexpr int DISPENSER_ID = 23, NOTE_ID = 25, CHEST_ID = 54, OVEN_IDLE = 61, OVEN_ACTIVE = 62, LEVER_ID = 69,
              BUTTON_ID = 77, CRATE_ID = 142, PISTON_ID = 238, STICKY_PISTON_ID = 239, PISTON_HEAD_ID = 240,
              HOPPER_ID = 243;
int doorWood(int id) { return id >= 145 && id <= 153 ? id - 144 : 0; }
bool isDoor(int id) { return id == 64 || id == 71 || (id >= 145 && id <= 153); }
int plateBase(int id) { return id == 70 ? -1 : id == 72 ? 0 : id == 154 ? 5 : -2; } // -2: not a plate

std::map<std::string, std::unique_ptr<axm::Model>> cache;

bool christmas() {
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    if (!lt) return false;
    int m = lt->tm_mon + 1, d = lt->tm_mday;
    return m == 12 && d >= 24 && d <= 26;
}

bool opaque(const World& w, int x, int y, int z) { return BLOCKS[w.getBlockId(x, y, z)].flags & BF_OPAQUE; }

float faceYaw(int side) { return side == 3 ? 180.0f : side == 4 ? 90.0f : side == 5 ? -90.0f : 0.0f; }

// BlockChest.vanillaChestTexture (+ AxeolonWood.chest: another wood's textures never match below)
int chestTexture(const World& w, int x, int y, int z, int side) {
    const int idx = 26, me = CHEST_ID;
    if (clampWood(w.getBlockMetadata(x, y, z)) != 0) return 256 + side; // a terrain2 slot: never 27/41/42
    auto O = [&](int id) { return bool(BLOCKS[id & 255].flags & BF_OPAQUE); };
    if (side == 1 || side == 0) return idx - 1;
    int v6 = w.getBlockId(x, y, z - 1), v7 = w.getBlockId(x, y, z + 1);
    int v8 = w.getBlockId(x - 1, y, z), v9 = w.getBlockId(x + 1, y, z);
    if (v6 != me && v7 != me) {
        if (v8 != me && v9 != me) {
            int f = 3;
            if (O(v6) && !O(v7)) f = 3;
            if (O(v7) && !O(v6)) f = 2;
            if (O(v8) && !O(v9)) f = 5;
            if (O(v9) && !O(v8)) f = 4;
            return side == f ? idx + 1 : idx;
        } else if (side != 4 && side != 5) {
            int v10 = v8 == me ? -1 : 0;
            int v11 = w.getBlockId(v8 == me ? x - 1 : x + 1, y, z - 1);
            int v12 = w.getBlockId(v8 == me ? x - 1 : x + 1, y, z + 1);
            if (side == 3) v10 = -1 - v10;
            int f = 3;
            if ((O(v6) || O(v11)) && !O(v7) && !O(v12)) f = 3;
            if ((O(v7) || O(v12)) && !O(v6) && !O(v11)) f = 2;
            return (side == f ? idx + 16 : idx + 32) + v10;
        }
        return idx;
    } else if (side != 2 && side != 3) {
        int v10 = v6 == me ? -1 : 0;
        int v11 = w.getBlockId(x - 1, y, v6 == me ? z - 1 : z + 1);
        int v12 = w.getBlockId(x + 1, y, v6 == me ? z - 1 : z + 1);
        if (side == 4) v10 = -1 - v10;
        int f = 5;
        if ((O(v8) || O(v11)) && !O(v9) && !O(v12)) f = 5;
        if ((O(v9) || O(v12)) && !O(v8) && !O(v11)) f = 4;
        return (side == f ? idx + 16 : idx + 32) + v10;
    }
    return idx;
}

void chestRight(int front, int& rx, int& rz) {
    if (front == 3) { rx = -1; rz = 0; } else if (front == 4) { rx = 0; rz = -1; }
    else if (front == 5) { rx = 0; rz = 1; } else { rx = 1; rz = 0; }
}

// {anchorX, anchorZ, partnerDx, partnerDz} for a double chest; false for a single one
bool doubleChest(const World& w, int x, int y, int z, int out[4]) {
    int id = w.getBlockId(x, y, z);
    const int n[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 4; ++i) {
        if (w.getBlockId(x + n[i][0], y, z + n[i][1]) != id) continue;
        int rx, rz;
        chestRight(chestFront(w, x, y, z), rx, rz);
        if (n[i][0] == rx && n[i][1] == rz) { out[0] = x; out[1] = z; out[2] = rx; out[3] = rz; return true; }
        if (n[i][0] == -rx && n[i][1] == -rz) { out[0] = x - rx; out[1] = z - rz; out[2] = rx; out[3] = rz; return true; }
    }
    return false;
}

struct LightCtx { const World* w; int bx, by, bz; };

float faceLight(double cx, double cy, double cz, double nx, double ny, double nz, void* user) {
    const LightCtx& c = *static_cast<LightCtx*>(user);
    const World& w = *c.w;
    int x = c.bx + int(std::floor(cx + nx * 0.5)), y = c.by + int(std::floor(cy + ny * 0.5)),
        z = c.bz + int(std::floor(cz + nz * 0.5));
    if (!opaque(w, x, y, z) && w.getBlockId(x, y, z) != w.getBlockId(c.bx, c.by, c.bz)) return lightBrightness(w, x, y, z);
    float m = 0.0f;
    const int d[6][3] = {{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int i = 0; i < 6; ++i) {
        int ax = c.bx + d[i][0], ay = c.by + d[i][1], az = c.bz + d[i][2];
        if (!opaque(w, ax, ay, az)) m = std::fmax(m, lightBrightness(w, ax, ay, az));
    }
    return m;
}

struct Anim {
    int kind, x, y, z, meta;
    std::string name;
    float start;
    bool holding = false, mobHold = false, ended = false;
    int pdx = 0, pdz = 0;
    int linger = 0, calm = 0;
};
std::vector<Anim> anims;
float now = 0.0f;

bool covers(const Anim& a, int x, int z) {
    return (a.x == x && a.z == z) || ((a.pdx != 0 || a.pdz != 0) && a.x + a.pdx == x && a.z + a.pdz == z);
}

void dirty(int x0, int y0, int z0, int x1, int y1, int z1) {
    if (markDirty) markDirty(x0, y0, z0, x1, y1, z1);
}

float animLength(const World& w, const Anim& a) {
    Pose p;
    if (!describe(w, a.x, a.y, a.z, p) || p.count == 0 || !p.rigs[0]) return 0.0f;
    return p.rigs[0]->animationLength(a.name);
}

void sound(int x, int y, int z, const char* name, float vol, float pitch) {
    if (playSound) playSound(x + 0.5, y + 0.5, z + 0.5, name, vol, pitch);
}

} // namespace

const axm::Model* rig(const std::string& name) {
    auto it = cache.find(name);
    if (it != cache.end()) return it->second.get();
    std::unique_ptr<axm::Model> m(new axm::Model);
    if (!m->load((romfsRoot() + "axeolon/block/" + name + ".axm").c_str())) m.reset();
    const axm::Model* r = m.get();
    cache[name] = std::move(m);
    return r;
}

const RigTexture* rigTexture(const std::string& name) {
    std::string n = name;
    if (name.rfind("chest", 0) == 0 && christmas()) // Christmas: every chest is a wrapped present
        n = name.rfind("chest_double", 0) == 0 ? "chest_double_christmas" : "chest_christmas";
    for (int i = 0; i < RIG_TEXTURE_COUNT; i++)
        if (n == RIG_TEXTURES[i].name) return &RIG_TEXTURES[i];
    return nullptr;
}

bool isModelBlock(int id) {
    return isDoor(id) || plateBase(id) != -2 || id == PISTON_ID || id == STICKY_PISTON_ID || id == PISTON_HEAD_ID ||
           id == HOPPER_ID || id == CHEST_ID || id == CRATE_ID || id == LEVER_ID || id == BUTTON_ID ||
           id == OVEN_IDLE || id == OVEN_ACTIVE || id == DISPENSER_ID || id == NOTE_ID;
}

void Pose::add(const std::string& name, int half) {
    rigs[count] = rig(name);
    names[count] = name;
    halves[count++] = half;
}

int chestFront(const World& w, int x, int y, int z) {
    if (w.getBlockId(x, y, z) != CHEST_ID) return 3; // AxeolonBlockAnims only asks chests
    for (int s = 2; s <= 5; ++s) {
        int tex = chestTexture(w, x, y, z, s);
        if (tex == 27 || tex == 41 || tex == 42) return s;
    }
    return 3;
}

bool describe(const World& w, int x, int y, int z, Pose& p) {
    int id = w.getBlockId(x, y, z);
    if (!isModelBlock(id)) return false;
    int meta = w.getBlockMetadata(x, y, z);
    p = Pose();
    if (isDoor(id)) {
        if (meta & 8) return true; // the top half is part of the bottom half's model
        p.add(id == 71 ? "door_iron" : doorRig(doorWood(id)), axm::Model::HALF_ALL);
        int k = ((meta & 3) - 1) & 3; // the edge the shut door lies on: 0 N, 1 E, 2 S, 3 W
        p.yaw = k == 1 ? -90.0f : k == 2 ? 180.0f : k == 3 ? 90.0f : 0.0f;
        p.still = (meta & 4) ? "open" : "";
    } else if (id == HOPPER_ID) {
        int f = meta & 7;
        f = (f == 1 || f > 5) ? 0 : f; // BlockHopper.facing
        p.add(f == 0 ? "hopper" : "hopper_side", axm::Model::HALF_ALL);
        if (f != 0) p.yaw = faceYaw(f);
    } else if (id == PISTON_HEAD_ID) {
        return true; // drawn as part of the piston behind it
    } else if (id == PISTON_ID || id == STICKY_PISTON_ID) {
        p.add(id == STICKY_PISTON_ID ? "piston_sticky" : "piston", axm::Model::HALF_ALL);
        int f = meta & 7;
        if (f > 5) f = 1; // BlockPistonBase.facing
        if (f == 1) p.pitch = 90.0f;
        else if (f == 0) p.pitch = -90.0f;
        else p.yaw = faceYaw(f);
        p.still = (meta & 8) ? "extend" : "";
    } else if (id == CHEST_ID) {
        int d[4];
        p.yaw = faceYaw(chestFront(w, x, y, z));
        if (!doubleChest(w, x, y, z, d)) {
            p.add(chestRig(meta), axm::Model::HALF_ALL);
        } else {
            if (d[0] != x || d[1] != z) return true; // drawn with the other half
            int other = w.getBlockMetadata(x + d[2], y, z + d[3]);
            if (other == meta) {
                p.add(doubleChestRig(meta), axm::Model::HALF_ALL);
            } else { // two woods: each half from its own model
                p.add(doubleChestRig(meta), axm::Model::HALF_THIS);
                p.add(doubleChestRig(other), axm::Model::HALF_OTHER);
            }
        }
    } else if (id == CRATE_ID) {
        p.add("crate", axm::Model::HALF_ALL);
    } else if (id == LEVER_ID) {
        p.add("lever", axm::Model::HALF_ALL);
        int m = meta & 7;
        if (m >= 1 && m <= 4) {
            p.pitch = 90.0f;
            p.yaw = m == 1 ? 90.0f : m == 2 ? -90.0f : m == 4 ? 180.0f : 0.0f;
        } else {
            p.yaw = m == 6 ? 90.0f : 0.0f;
        }
        p.still = (meta & 8) ? "on" : "off";
    } else if (id == BUTTON_ID) {
        p.add("button", axm::Model::HALF_ALL);
        int m = meta & 7;
        p.yaw = m == 1 ? 90.0f : m == 2 ? -90.0f : m == 4 ? 180.0f : 0.0f;
        p.still = (meta & 8) ? "press" : "";
    } else if (plateBase(id) != -2) {
        int base = plateBase(id);
        p.add(base < 0 ? "pressure_plate_stone" : std::string("pressure_plate_") + WOOD[clampWood(base + (meta >> 1 & 7))],
              axm::Model::HALF_ALL);
        p.still = (meta & 1) ? "press" : "";
    } else if (id == OVEN_IDLE || id == OVEN_ACTIVE) {
        p.add(id == OVEN_ACTIVE ? "furnace_lit" : "furnace", axm::Model::HALF_ALL);
        p.yaw = faceYaw(meta);
    } else if (id == DISPENSER_ID) {
        p.add("dispenser", axm::Model::HALF_ALL);
        p.yaw = faceYaw(meta);
    } else {
        p.add("note_block", axm::Model::HALF_ALL);
    }
    return true;
}

float lightBrightness(const World& w, int x, int y, int z) {
    int id = w.getBlockId(x, y, z);
    int l = w.getLightValue(x, y, z);
    if ((BLOCKS[id].flags & BF_SLAB) || id == 60) { // getBlockLightValue_do: brightest neighbour
        int n[5][3] = {{0, 1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
        l = w.getLightValue(x, y + 1, z);
        for (int i = 1; i < 5; i++) l = std::max(l, w.getLightValue(x + n[i][0], y + n[i][1], z + n[i][2]));
    }
    if (dynlight::any()) l = std::max(l & 15, dynlight::lightAt(x, y, z)); // a light carried past
    return ChunkMesher::LIGHT_BRIGHTNESS[l & 15];
}

bool drawStill(const World& w, int x, int y, int z, double ox, double oy, double oz,
               std::vector<axm::ModelVertex>& quads, std::vector<Batch>& batches) {
    if (hides(x, y, z)) return false;
    Pose p;
    if (!describe(w, x, y, z, p) || p.count == 0) return false;
    LightCtx ctx{&w, x, y, z};
    bool drew = false;
    for (int k = 0; k < p.count; ++k) {
        const axm::Model* r = p.rigs[k];
        const RigTexture* tex = rigTexture(p.names[k]);
        if (!r || !tex) continue;
        size_t start = quads.size();
        const char* anim = p.still.empty() ? nullptr : p.still.c_str();
        r->emitRig(quads, anim, anim ? r->animationLength(p.still) : 0.0f, p.yaw, p.pitch, p.halves[k], ox, oy, oz,
                   faceLight, &ctx);
        batches.push_back({tex, start, quads.size() - start});
        drew = true;
    }
    return drew;
}

// ---- starting ----

void play(World& w, int kind, int x, int y, int z, const std::string& name) {
    int pdx = 0, pdz = 0;
    if (kind == CHEST) {
        int d[4];
        if (doubleChest(w, x, y, z, d)) { x = d[0]; z = d[1]; pdx = d[2]; pdz = d[3]; }
    }
    for (int i = int(anims.size()) - 1; i >= 0; --i) {
        const Anim& a = anims[i];
        if (a.y == y && (covers(a, x, z) || covers(a, x + pdx, z + pdz))) anims.erase(anims.begin() + i);
    }
    Anim a;
    a.kind = kind; a.x = x; a.y = y; a.z = z; a.name = name; a.start = now;
    a.pdx = pdx; a.pdz = pdz;
    a.meta = w.getBlockMetadata(x, y, z);
    a.holding = (kind == CHEST || kind == CRATE) && name == "open";
    anims.push_back(a);
    dirty(std::min(x, x + pdx) - 1, y - 1, std::min(z, z + pdz) - 1, std::max(x, x + pdx) + 1, y + 2,
          std::max(z, z + pdz) + 1);
}

void piston(World& w, int x, int y, int z, const std::string& name) {
    for (auto& a : anims)
        if (a.x == x && a.y == y && a.z == z && !a.ended && a.name == name) return;
    play(w, PISTON, x, y, z, name);
}

void door(World& w, int x, int y, int z, int metaBefore) {
    bool opening = (metaBefore & 4) == 0;
    play(w, DOOR, x, y, z, opening ? "open" : "close");
    if (!anims.empty()) anims.back().meta = metaBefore & 3;
}

void openContainer(World& w, int x, int y, int z) {
    int id = w.getBlockId(x, y, z);
    if (id != CHEST_ID && id != CRATE_ID) return;
    // a double chest is one model, kept on its anchor half (play() finds it)
    play(w, id == CRATE_ID ? CRATE : CHEST, x, y, z, "open");
    sound(x, y, z, "random.door_open", 0.35f, 1.4f);
}

void holdOpen(World& w, int x, int y, int z) {
    int id = w.getBlockId(x, y, z);
    if (id != CHEST_ID && id != CRATE_ID) return;
    play(w, id == CRATE_ID ? CRATE : CHEST, x, y, z, "open");
    if (!anims.empty()) anims.back().mobHold = true;
    sound(x, y, z, "random.door_open", 0.35f, 1.6f);
}

void release(World&, int x, int y, int z) {
    for (int i = int(anims.size()) - 1; i >= 0; --i) {
        Anim& a = anims[i];
        if (a.y == y && covers(a, x, z) && a.mobHold && a.holding) {
            a.holding = false;
            a.mobHold = false;
            a.name = "close";
            a.start = now;
            sound(x, y, z, "random.door_close", 0.45f, 1.3f);
            break;
        }
    }
}

// ---- per tick ----

void clear() { anims.clear(); }

void tick(World& w, bool guiOpen) {
    now += 1.0f;
    for (int i = int(anims.size()) - 1; i >= 0; --i) {
        Anim& a = anims[i];
        if (a.holding && !a.mobHold && !guiOpen) { // GUI closed: lid comes down
            a.holding = false;
            a.name = "close";
            a.start = now;
            sound(a.x, a.y, a.z, "random.door_close", 0.35f, 1.4f);
        }
        if (a.holding) continue;
        float len = animLength(w, a);
        if ((now - a.start) / 20.0f > len) {
            if (!a.ended) {
                // stop hiding the block and ask for a redraw, but keep drawing the final pose
                // until the section has rebuilt, so there's never a frame with no block
                a.ended = true;
                dirty(a.x - 1, a.y - 1, a.z - 1, a.x + 1, a.y + 2, a.z + 1);
            } else {
                bool pending = false;
                if (chunkPending)
                    for (int dx = std::min(0, a.pdx) - 1; dx <= std::max(0, a.pdx) + 1 && !pending; ++dx)
                        for (int dy = -1; dy <= 2 && !pending; ++dy)
                            for (int dz = std::min(0, a.pdz) - 1; dz <= std::max(0, a.pdz) + 1 && !pending; ++dz)
                                pending = chunkPending(a.x + dx, a.y + dy, a.z + dz);
                ++a.linger;
                a.calm = pending ? 0 : a.calm + 1;
                if (a.calm >= 2 || a.linger > 100) anims.erase(anims.begin() + i);
            }
        }
    }
}

bool anyActive() { return !anims.empty(); }

bool hides(int x, int y, int z) {
    for (const Anim& a : anims) {
        if (a.ended) continue;
        if (covers(a, x, z) && (a.y == y || (a.kind == DOOR && a.y + 1 == y))) return true;
    }
    return false;
}

void render(const World& w, float partial, std::vector<axm::ModelVertex>& quads, std::vector<Batch>& batches) {
    for (const Anim& a : anims) {
        Pose p;
        if (!describe(w, a.x, a.y, a.z, p)) continue;
        float sec = (now - a.start + partial) / 20.0f;
        LightCtx ctx{&w, a.x, a.y, a.z};
        for (int k = 0; k < p.count; ++k) {
            const axm::Model* r = p.rigs[k];
            const RigTexture* tex = rigTexture(p.names[k]);
            if (!r || !tex) continue;
            size_t start = quads.size();
            r->emitRig(quads, a.name.c_str(), sec, p.yaw, p.pitch, p.halves[k], a.x, a.y, a.z, faceLight, &ctx);
            batches.push_back({tex, start, quads.size() - start});
        }
    }
}

} // namespace BlockModels
