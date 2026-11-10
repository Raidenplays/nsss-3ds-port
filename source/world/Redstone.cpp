#include "Redstone.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>
#include "BlockProps.h"
#include "Blocks.h"
#include "Physics.h"
#include "World.h"
#include "game/Survival.h"

namespace Redstone {

bool (*anythingOn)(const AABB& box, bool livingOnly) = nullptr;

namespace {
enum : int {
    WIRE = 55, TORCH_OFF = 75, TORCH_ON = 76, LEVER = 69, BUTTON = 77, PLATE_STONE = 70, PLATE_WOOD = 72,
    PLATE_CHARRED = 154, REPEATER = 93, REPEATER_LIT = 94, LAPIS_WIRE = 95, LAPIS_TORCH = 96, LAPIS_TORCH_OFF = 97,
    LAPIS_LAMP = 98, LAPIS_LAMP_LIT = 99, BLOCK_REDSTONE = 224, LAPIS_BRICKS = 225, REDSTONE_BRICKS = 226,
    BLOCK_LAPIS = 22, TNT = 46, NOTE = 25, DOOR_WOOD = 64, DOOR_IRON = 71,
};
// BlockModels kinds (AxeolonBlockAnims)
enum : int { ANIM_DOOR = 0, ANIM_LEVER = 1, ANIM_BUTTON = 2, ANIM_PLATE = 3, ANIM_NOTE = 8 };

bool wiresProvidePower = true; // BlockRedstoneWire.wiresProvidePower

void sound(World& w, const char* n, double x, double y, double z, float v, float p) {
    if (w.onSound) w.onSound(n, x, y, z, v, p);
}
void anim(World& w, int kind, int x, int y, int z, const char* name) {
    if (w.onBlockAnim) w.onBlockAnim(kind, x, y, z, name);
}
void particle(World& w, int type, double x, double y, double z, double a, double b, double c) {
    if (w.onParticle) w.onParticle(type, x, y, z, a, b, c);
}
bool opaque(World& w, int x, int y, int z) { return w.isBlockOpaqueCube(x, y, z); }
bool isPlate(int id) { return id == PLATE_STONE || id == PLATE_WOOD || id == PLATE_CHARRED; }
bool isDoor(int id) { return BP[id].cls == BC::Door; }

// ---------------------------------------------------------------- lapis (AxeolonLapis)
const int DX[6] = {0, 0, 0, 0, -1, 1}, DY[6] = {-1, 1, 0, 0, 0, 0}, DZ[6] = {0, 0, -1, 1, 0, 0};
const int WALL_DX[4] = {-1, 1, 0, 0}, WALL_DZ[4] = {0, 0, -1, 1};
bool lapisBusy = false, lapisHealing = false;

int wallOf(int id) { return id >= 234 && id <= 237 ? id - 234 : -1; }
int wallOfSide(int side) { return side == 4 ? 0 : side == 5 ? 1 : side == 2 ? 2 : 3; }
bool isDust(int id) { return id == LAPIS_WIRE || wallOf(id) >= 0; }
bool isLamp(int id) { return id == LAPIS_LAMP || id == LAPIS_LAMP_LIT; }
bool isLapisTorch(int id) { return id == LAPIS_TORCH || id == LAPIS_TORCH_OFF; }
bool isRepeater(int id) { return id == REPEATER || id == REPEATER_LIT; }

void front(int meta, int& fx, int& fz) { // BlockPowerRepeater.front
    switch (meta & 3) {
    case 0: fx = 0; fz = 1; break;
    case 1: fx = -1; fz = 0; break;
    case 2: fx = 0; fz = -1; break;
    default: fx = 1; fz = 0; break;
    }
}
int sideFor(int fx, int fz) { return fz == 1 ? 2 : fz == -1 ? 3 : fx == 1 ? 4 : 5; }
int repeaterDelay(int meta) { return (meta >> 2 & 3) + 1; }

bool sourceInto(World& w, int sx, int sy, int sz, int x, int y, int z) {
    int id = w.getBlockId(sx, sy, sz);
    if (id == LAPIS_BRICKS || id == BLOCK_LAPIS || id == LAPIS_TORCH) return true;
    if (id == REPEATER_LIT) {
        int fx, fz;
        front(w.getBlockMetadata(sx, sy, sz), fx, fz);
        return sx + fx == x && sy == y && sz + fz == z;
    }
    return false;
}
bool signalAt(World& w, int x, int y, int z) {
    for (int i = 0; i < 6; ++i) {
        int nx = x + DX[i], ny = y + DY[i], nz = z + DZ[i];
        if (isDust(w.getBlockId(nx, ny, nz)) && w.getBlockMetadata(nx, ny, nz) > 0) return true;
        if (sourceInto(w, nx, ny, nz, x, y, z)) return true;
    }
    return false;
}
bool blockLapisPowered(World& w, int x, int y, int z) {
    for (int i = 1; i < 6; ++i) {
        int nx = x + DX[i], ny = y + DY[i], nz = z + DZ[i];
        if (isDust(w.getBlockId(nx, ny, nz)) && w.getBlockMetadata(nx, ny, nz) > 0) return true;
    }
    int id = w.getBlockId(x, y, z);
    return id == LAPIS_BRICKS || id == BLOCK_LAPIS;
}
struct P3 { int x, y, z; };
int64_t key(int x, int y, int z) {
    return (int64_t(x) & 0x3FFFFFFLL) << 38 | (int64_t(z) & 0x3FFFFFFLL) << 12 | int64_t(y & 0xFFF);
}
void neighbours(World& w, int x, int y, int z, std::vector<P3>& out) {
    out.clear();
    for (int i = 0; i < 6; ++i) {
        int nx = x + DX[i], ny = y + DY[i], nz = z + DZ[i];
        if (isDust(w.getBlockId(nx, ny, nz))) out.push_back({nx, ny, nz});
    }
    bool openAbove = !opaque(w, x, y + 1, z);
    int wall = wallOf(w.getBlockId(x, y, z));
    for (int i = 2; i < 6; ++i) {
        int nx = x + DX[i], nz = z + DZ[i], wi = wallOfSide(i);
        if (openAbove && wall == wi && isDust(w.getBlockId(nx, y + 1, nz))) out.push_back({nx, y + 1, nz});
        if (!opaque(w, nx, y, nz) && wallOf(w.getBlockId(nx, y - 1, nz)) == (wi ^ 1)) out.push_back({nx, y - 1, nz});
    }
}
bool touchesSource(World& w, int x, int y, int z) {
    for (int i = 0; i < 6; ++i)
        if (sourceInto(w, x + DX[i], y + DY[i], z + DZ[i], x, y, z)) return true;
    return false;
}

// ---------------------------------------------------------------- redstone wire
std::vector<P3> wireNotify; // BlockRedstoneWire.field_21031_b
void addNotify(int x, int y, int z) {
    for (const P3& p : wireNotify)
        if (p.x == x && p.y == y && p.z == z) return;
    wireNotify.push_back({x, y, z});
}
int maxStrength(World& w, int x, int y, int z, int cur) {
    if (w.getBlockId(x, y, z) != WIRE) return cur;
    int m = w.getBlockMetadata(x, y, z);
    return m > cur ? m : cur;
}
void wireCalc(World& w, int x, int y, int z, int fx, int fy, int fz) { // func_21030_a
    int old = w.getBlockMetadata(x, y, z);
    int power = 0;
    wiresProvidePower = false;
    bool powered = isBlockIndirectlyGettingPowered(w, x, y, z);
    wiresProvidePower = true;
    if (powered) {
        power = 15;
    } else {
        for (int i = 0; i < 4; ++i) {
            int nx = x, nz = z;
            if (i == 0) nx = x - 1;
            if (i == 1) ++nx;
            if (i == 2) nz = z - 1;
            if (i == 3) ++nz;
            if (nx != fx || y != fy || nz != fz) power = maxStrength(w, nx, y, nz, power);
            if (opaque(w, nx, y, nz) && !opaque(w, x, y + 1, z)) {
                if (nx != fx || y + 1 != fy || nz != fz) power = maxStrength(w, nx, y + 1, nz, power);
            } else if (!opaque(w, nx, y, nz) && (nx != fx || y - 1 != fy || nz != fz)) {
                power = maxStrength(w, nx, y - 1, nz, power);
            }
        }
        power = power > 0 ? power - 1 : 0;
    }
    if (old == power) return;
    bool was = w.editingBlocks;
    w.editingBlocks = true;
    w.setBlockMetadataWithNotify(x, y, z, power);
    w.markBlockNeedsUpdate(x, y, z);
    w.editingBlocks = was;
    for (int i = 0; i < 4; ++i) {
        int nx = x, nz = z, ny = y - 1;
        if (i == 0) nx = x - 1;
        if (i == 1) ++nx;
        if (i == 2) nz = z - 1;
        if (i == 3) ++nz;
        if (opaque(w, nx, y, nz)) ny += 2;
        int s = maxStrength(w, nx, y, nz, -1);
        power = w.getBlockMetadata(x, y, z);
        if (power > 0) --power;
        if (s >= 0 && s != power) wireCalc(w, nx, y, nz, x, y, z);
        s = maxStrength(w, nx, ny, nz, -1);
        power = w.getBlockMetadata(x, y, z);
        if (power > 0) --power;
        if (s >= 0 && s != power) wireCalc(w, nx, ny, nz, x, y, z);
    }
    if (old == 0 || power == 0) {
        addNotify(x, y, z);
        addNotify(x - 1, y, z);
        addNotify(x + 1, y, z);
        addNotify(x, y - 1, z);
        addNotify(x, y + 1, z);
        addNotify(x, y, z - 1);
        addNotify(x, y, z + 1);
    }
}
void wireUpdate(World& w, int x, int y, int z) { // updateAndPropagateCurrentStrength
    wireCalc(w, x, y, z, x, y, z);
    std::vector<P3> list;
    list.swap(wireNotify);
    for (const P3& p : list) w.notifyBlocksOfNeighborChange(p.x, p.y, p.z, WIRE);
}
void notifyWireNeighbours(World& w, int x, int y, int z) {
    if (w.getBlockId(x, y, z) != WIRE) return;
    w.notifyBlocksOfNeighborChange(x, y, z, WIRE);
    w.notifyBlocksOfNeighborChange(x - 1, y, z, WIRE);
    w.notifyBlocksOfNeighborChange(x + 1, y, z, WIRE);
    w.notifyBlocksOfNeighborChange(x, y, z - 1, WIRE);
    w.notifyBlocksOfNeighborChange(x, y, z + 1, WIRE);
    w.notifyBlocksOfNeighborChange(x, y - 1, z, WIRE);
    w.notifyBlocksOfNeighborChange(x, y + 1, z, WIRE);
}
void wireAroundNotify(World& w, int x, int y, int z) {
    notifyWireNeighbours(w, x - 1, y, z);
    notifyWireNeighbours(w, x + 1, y, z);
    notifyWireNeighbours(w, x, y, z - 1);
    notifyWireNeighbours(w, x, y, z + 1);
    notifyWireNeighbours(w, x - 1, opaque(w, x - 1, y, z) ? y + 1 : y - 1, z);
    notifyWireNeighbours(w, x + 1, opaque(w, x + 1, y, z) ? y + 1 : y - 1, z);
    notifyWireNeighbours(w, x, opaque(w, x, y, z - 1) ? y + 1 : y - 1, z - 1);
    notifyWireNeighbours(w, x, opaque(w, x, y, z + 1) ? y + 1 : y - 1, z + 1);
}
bool providerOrWire(World& w, int x, int y, int z) { // isPowerProviderOrWire
    int id = w.getBlockId(x, y, z);
    return id == WIRE || (id != 0 && canProvidePower(id));
}
bool wirePoweringTo(World& w, int x, int y, int z, int side) {
    if (!wiresProvidePower || w.getBlockMetadata(x, y, z) == 0) return false;
    if (side == 1) return true;
    bool a = providerOrWire(w, x - 1, y, z) || (!opaque(w, x - 1, y, z) && providerOrWire(w, x - 1, y - 1, z));
    bool b = providerOrWire(w, x + 1, y, z) || (!opaque(w, x + 1, y, z) && providerOrWire(w, x + 1, y - 1, z));
    bool c = providerOrWire(w, x, y, z - 1) || (!opaque(w, x, y, z - 1) && providerOrWire(w, x, y - 1, z - 1));
    bool d = providerOrWire(w, x, y, z + 1) || (!opaque(w, x, y, z + 1) && providerOrWire(w, x, y - 1, z + 1));
    if (!opaque(w, x, y + 1, z)) {
        if (opaque(w, x - 1, y, z) && providerOrWire(w, x - 1, y + 1, z)) a = true;
        if (opaque(w, x + 1, y, z) && providerOrWire(w, x + 1, y + 1, z)) b = true;
        if (opaque(w, x, y, z - 1) && providerOrWire(w, x, y + 1, z - 1)) c = true;
        if (opaque(w, x, y, z + 1) && providerOrWire(w, x, y + 1, z + 1)) d = true;
    }
    if (!c && !b && !a && !d && side >= 2 && side <= 5) return true;
    if (side == 2 && c && !a && !b) return true;
    if (side == 3 && d && !a && !b) return true;
    if (side == 4 && a && !c && !d) return true;
    return side == 5 && b && !c && !d;
}

// ---------------------------------------------------------------- redstone torch
struct Burnout { int x, y, z; int64_t time; };
std::deque<Burnout> torchUpdates;
bool checkForBurnout(World& w, int x, int y, int z, bool add) {
    if (add) torchUpdates.push_back({x, y, z, w.info.time});
    int n = 0;
    for (const Burnout& b : torchUpdates)
        if (b.x == x && b.y == y && b.z == z && ++n >= 8) return true;
    return false;
}
bool torchPoweringTo(World& w, int x, int y, int z, int side) {
    int m = w.getBlockMetadata(x, y, z);
    if (m == 5 && side == 1) return false;
    if (m == 3 && side == 3) return false;
    if (m == 4 && side == 2) return false;
    if (m == 1 && side == 5) return false;
    return !(m == 2 && side == 4);
}
bool torchGettingPowered(World& w, int x, int y, int z) { // isGettingPoweredFromSide
    int m = w.getBlockMetadata(x, y, z);
    if (m == 5 && isBlockIndirectlyProvidingPowerTo(w, x, y - 1, z, 0)) return true;
    if (m == 3 && isBlockIndirectlyProvidingPowerTo(w, x, y, z - 1, 2)) return true;
    if (m == 4 && isBlockIndirectlyProvidingPowerTo(w, x, y, z + 1, 3)) return true;
    if (m == 1 && isBlockIndirectlyProvidingPowerTo(w, x - 1, y, z, 4)) return true;
    return m == 2 && isBlockIndirectlyProvidingPowerTo(w, x + 1, y, z, 5);
}
void notifyAll6(World& w, int x, int y, int z, int id) {
    w.notifyBlocksOfNeighborChange(x, y - 1, z, id);
    w.notifyBlocksOfNeighborChange(x, y + 1, z, id);
    w.notifyBlocksOfNeighborChange(x - 1, y, z, id);
    w.notifyBlocksOfNeighborChange(x + 1, y, z, id);
    w.notifyBlocksOfNeighborChange(x, y, z - 1, id);
    w.notifyBlocksOfNeighborChange(x, y, z + 1, id);
}

// lever and button: the block they're stuck to hears it too
void notifyAttached(World& w, int x, int y, int z, int id, int m) {
    w.notifyBlocksOfNeighborChange(x, y, z, id);
    if (m == 1) w.notifyBlocksOfNeighborChange(x - 1, y, z, id);
    else if (m == 2) w.notifyBlocksOfNeighborChange(x + 1, y, z, id);
    else if (m == 3) w.notifyBlocksOfNeighborChange(x, y, z - 1, id);
    else if (m == 4) w.notifyBlocksOfNeighborChange(x, y, z + 1, id);
    else w.notifyBlocksOfNeighborChange(x, y - 1, z, id);
}
bool attachedPowering(int meta, int side, int bit) {
    if ((meta & bit) == 0) return false;
    int m = meta & 7;
    return (m == 5 && side == 1) || (m == 4 && side == 2) || (m == 3 && side == 3) || (m == 2 && side == 4) || (m == 1 && side == 5);
}

// ---------------------------------------------------------------- pressure plates
void plateState(World& w, int x, int y, int z) { // setStateIfMobInteractsWithPlate
    int id = w.getBlockId(x, y, z), meta = w.getBlockMetadata(x, y, z);
    bool was = (meta & 1) == 1;
    int type = meta & 14;
    const double e = 2.0 / 16.0;
    AABB box{x + e, double(y), z + e, x + 1 - e, y + 0.25, z + 1 - e};
    bool on = anythingOn && anythingOn(box, id == PLATE_STONE);
    if (on && !was) {
        w.setBlockMetadataWithNotify(x, y, z, type | 1);
        anim(w, ANIM_PLATE, x, y, z, "press");
        w.notifyBlocksOfNeighborChange(x, y, z, id);
        w.notifyBlocksOfNeighborChange(x, y - 1, z, id);
        w.markBlockNeedsUpdate(x, y, z);
        sound(w, "random.click", x + 0.5, y + 0.1, z + 0.5, 0.3f, 0.6f);
    }
    if (!on && was) {
        w.setBlockMetadataWithNotify(x, y, z, type);
        anim(w, ANIM_PLATE, x, y, z, "release");
        w.notifyBlocksOfNeighborChange(x, y, z, id);
        w.notifyBlocksOfNeighborChange(x, y - 1, z, id);
        w.markBlockNeedsUpdate(x, y, z);
        sound(w, "random.click", x + 0.5, y + 0.1, z + 0.5, 0.3f, 0.5f);
    }
    if (on) w.scheduleBlockUpdate(x, y, z, id);
}

// ---------------------------------------------------------------- listeners
std::unordered_set<int64_t> notePowered; // TileEntityNote.previousRedstoneState (not saved, like Beta)

void playNote(World& w, int x, int y, int z) { // TileEntityNote.triggerNote + BlockNote.playBlock
    if (w.getBlockId(x, y + 1, z) != 0 && BP[w.getBlockId(x, y + 1, z)].mat != Mat::air) return;
    nbt::Tag* te = w.tileEntityAt(x, y, z);
    int note = te ? std::max(0, std::min(24, int(te->getByte("note")))) : 0;
    Mat below = BP[w.getBlockId(x, y - 1, z)].mat;
    int inst = below == Mat::rock ? 1 : below == Mat::sand ? 2 : below == Mat::glass ? 3 : below == Mat::wood ? 4 : 0;
    static const char* NAMES[5] = {"note.harp", "note.bd", "note.snare", "note.hat", "note.bassattack"};
    anim(w, ANIM_NOTE, x, y, z, "play");
    float pitch = float(std::pow(2.0, double(note - 12) / 12.0));
    sound(w, NAMES[inst], x + 0.5, y + 0.5, z + 0.5, 3.0f, pitch);
    particle(w, 100, x + 0.5, y + 1.2, z + 0.5, double(note) / 24.0, 0, 0); // "note"
}

void doorPower(World& w, int x, int y, int z, bool powered) { // BlockDoor.func_311_a
    int id = w.getBlockId(x, y, z), meta = w.getBlockMetadata(x, y, z);
    if (meta & 8) {
        if (w.getBlockId(x, y - 1, z) == id) doorPower(w, x, y - 1, z, powered);
        return;
    }
    bool open = (meta & 4) != 0;
    if (open == powered) return;
    if (w.onDoorSwing) w.onDoorSwing(x, y, z, meta);
    if (w.getBlockId(x, y + 1, z) == id) w.setBlockMetadataWithNotify(x, y + 1, z, (meta ^ 4) + 8);
    w.setBlockMetadataWithNotify(x, y, z, meta ^ 4);
    w.markBlocksDirty(x, y - 1, z, x, y, z);
    if (((meta ^ 4) & 4) != 0) sound(w, "random.door_open", x + 0.5, y + 0.5, z + 0.5, 1.0f, w.rand.nextFloat() * 0.1f + 0.9f);
    else sound(w, "random.door_close", x + 0.5, y + 0.5, z + 0.5, 1.0f, w.rand.nextFloat() * 0.1f + 0.9f);
}

// ---------------------------------------------------------------- pistons (BlockPistonBase)
const int FX[6] = {0, 0, 0, 0, -1, 1}, FY[6] = {-1, 1, 0, 0, 0, 0}, FZ[6] = {0, 0, -1, 1, 0, 0};
constexpr int PISTON = 238, STICKY = 239, HEAD = 240, ANIM_PISTON = 9;
bool pistonMoving = false;
int facingOf(int meta) { int f = meta & 7; return f > 5 ? 1 : f; }
bool pistonPowered(World& w, int x, int y, int z, int f) {
    for (int i = 0; i < 6; ++i) {
        if (i == f) continue;
        if (isBlockIndirectlyProvidingPowerTo(w, x + FX[i], y + FY[i], z + FZ[i], i)) return true;
    }
    return false;
}
bool pistonBreaks(int id) {
    if (!matSolid(BP[id].mat)) return true;
    switch (BP[id].cls) {
    case BC::Door: case BC::Sign: case BC::Ladder: case BC::MinecartTrack: case BC::Cactus: case BC::Cake: case BC::Reed:
    case BC::PressurePlate: case BC::Button: case BC::Lever: case BC::Torch: case BC::RedstoneTorch: case BC::LapisTorch:
    case BC::PowerRepeater: case BC::LapisWire: case BC::Icicle: case BC::FlowerPot:
        return true;
    default: return false;
    }
}
bool pistonPushable(World& w, int x, int y, int z, int id) {
    if (id == 49 || id == 7 || survival::HARDNESS[id] < 0.0f) return false;
    if (BP[id].mat == Mat::portal || id == HEAD) return false;
    if ((id == PISTON || id == STICKY) && (w.getBlockMetadata(x, y, z) & 8)) return false;
    return true;
}
int pistonPlan(World& w, int x, int y, int z, int f) {
    int dx = FX[f], dy = FY[f], dz = FZ[f], n = 0, cx = x + dx, cy = y + dy, cz = z + dz;
    for (;;) {
        if (cy < 0 || cy >= 256) return -1;
        int id = w.getBlockId(cx, cy, cz);
        if (id == 0 || pistonBreaks(id)) return n;
        if (!pistonPushable(w, cx, cy, cz, id)) return -1;
        if (++n > 12) return -1;
        cx += dx; cy += dy; cz += dz;
    }
}
void pistonTouch(World& w, int x, int y, int z) {
    w.markBlockNeedsUpdate(x, y, z);
    w.notifyBlocksOfNeighborChange(x, y, z, w.getBlockId(x, y, z));
}
void pistonMove(World& w, int x, int y, int z, int dx, int dy, int dz) { // move one block and what's in it
    int id = w.getBlockId(x, y, z), meta = w.getBlockMetadata(x, y, z);
    int tx = x + dx, ty = y + dy, tz = z + dz;
    nbt::TagPtr tag;
    if (nbt::Tag* te = w.tileEntityAt(x, y, z)) tag = nbt::clone(*te);
    Blocks::quietRemoval = true;
    w.setBlockAndMetadataJ(tx, ty, tz, id, meta);
    if (tag) {
        if (nbt::Tag* moved = w.tileEntityAt(tx, ty, tz) ? w.tileEntityAt(tx, ty, tz) : w.addTileEntity(tx, ty, tz, tag->getString("id").c_str())) {
            for (auto& kv : tag->entries)
                if (kv.first != "x" && kv.first != "y" && kv.first != "z") moved->set(kv.first.c_str(), nbt::clone(*kv.second));
        }
    }
    w.setBlockAndMetadataJ(x, y, z, 0, 0);
    Blocks::quietRemoval = false;
    w.markBlockNeedsUpdate(tx, ty, tz);
}
void pistonAnimate(World& w, int x, int y, int z, int f, bool out) {
    if (out && pistonPlan(w, x, y, z, f) < 0) return;
    anim(w, ANIM_PISTON, x, y, z, out ? "extend" : "retract");
}
void pistonCheck(World& w, int x, int y, int z, int id) {
    int meta = w.getBlockMetadata(x, y, z), f = facingOf(meta);
    bool ext = (meta & 8) != 0;
    if (ext && w.getBlockId(x + FX[f], y + FY[f], z + FZ[f]) != HEAD) { // lost its head
        w.setBlockMetadataWithNotify(x, y, z, f);
        ext = false;
    }
    bool p = pistonPowered(w, x, y, z, f);
    if (p != ext) {
        w.scheduleBlockUpdateIn(x, y, z, id, 4);
        pistonAnimate(w, x, y, z, f, p);
    }
}
void pistonExtend(World& w, int x, int y, int z, int f, bool sticky) {
    int dx = FX[f], dy = FY[f], dz = FZ[f];
    int n = pistonPlan(w, x, y, z, f);
    if (n < 0) return;
    int cx = x + dx * (n + 1), cy = y + dy * (n + 1), cz = z + dz * (n + 1);
    if (w.onLaunch) { // throw anything in the way
        double x0 = std::min(x + dx, x + dx * (n + 1)), x1 = std::max(x + dx, x + dx * (n + 1)) + 1;
        double y0 = std::min(y + dy, y + dy * (n + 1)), y1 = std::max(y + dy, y + dy * (n + 1)) + 1;
        double z0 = std::min(z + dz, z + dz * (n + 1)), z1 = std::max(z + dz, z + dz * (n + 1)) + 1;
        if (f == 1) y1 += 0.25;
        double s = sticky ? 0.8 : 1.3;
        w.onLaunch(AABB{x0, y0, z0, x1, y1, z1}, dx * s, dy * s + (dy == 0 ? 0.3 : 0.0), dz * s);
    }
    pistonMoving = true;
    int end = w.getBlockId(cx, cy, cz);
    if (end != 0) {
        Blocks::dropBlockAsItem(w, cx, cy, cz, end, w.getBlockMetadata(cx, cy, cz));
        w.setBlockWithNotify(cx, cy, cz, 0);
    }
    for (int k = n; k >= 1; --k) pistonMove(w, x + dx * k, y + dy * k, z + dz * k, dx, dy, dz);
    w.setBlockAndMetadataJ(x + dx, y + dy, z + dz, HEAD, f | (sticky ? 8 : 0));
    w.setBlockMetadata(x, y, z, f | 8);
    pistonMoving = false;
    for (int k = 0; k <= n + 1; ++k) pistonTouch(w, x + dx * k, y + dy * k, z + dz * k);
    sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.5f, 0.6f);
}
void pistonRetract(World& w, int x, int y, int z, int f, bool sticky) {
    int dx = FX[f], dy = FY[f], dz = FZ[f];
    int hx = x + dx, hy = y + dy, hz = z + dz;
    pistonMoving = true;
    if (w.getBlockId(hx, hy, hz) == HEAD) w.setBlockAndMetadataJ(hx, hy, hz, 0, 0);
    w.setBlockMetadata(x, y, z, f);
    if (sticky) {
        int px = hx + dx, py = hy + dy, pz = hz + dz;
        int id = w.getBlockId(px, py, pz);
        if (id != 0 && !pistonBreaks(id) && pistonPushable(w, px, py, pz, id)) pistonMove(w, px, py, pz, -dx, -dy, -dz);
    }
    pistonMoving = false;
    for (int k = 0; k <= 2; ++k) pistonTouch(w, x + dx * k, y + dy * k, z + dz * k);
    sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.5f, 0.5f);
}
} // namespace

// ================================================================ power

bool canProvidePower(int id) {
    switch (id) {
    case WIRE: return wiresProvidePower;
    case TORCH_OFF: case TORCH_ON: case LEVER: case BUTTON: case PLATE_STONE: case PLATE_WOOD: case PLATE_CHARRED:
    case REPEATER: case REPEATER_LIT: case BLOCK_REDSTONE: case REDSTONE_BRICKS:
        return true;
    default: return false;
    }
}

bool isPoweringTo(World& w, int x, int y, int z, int side) {
    int id = w.getBlockId(x, y, z);
    switch (id) {
    case WIRE: return wirePoweringTo(w, x, y, z, side);
    case TORCH_ON: return torchPoweringTo(w, x, y, z, side);
    case LEVER: case BUTTON: return (w.getBlockMetadata(x, y, z) & 8) > 0;
    case PLATE_STONE: case PLATE_WOOD: case PLATE_CHARRED: return (w.getBlockMetadata(x, y, z) & 1) > 0;
    case REPEATER_LIT: {
        int meta = w.getBlockMetadata(x, y, z), fx, fz;
        front(meta, fx, fz);
        int f = w.getBlockId(x + fx, y, z + fz);
        return side == sideFor(fx, fz) && !(isDust(f) || isLamp(f));
    }
    case BLOCK_REDSTONE: case REDSTONE_BRICKS: return true;
    default: return false;
    }
}

bool isIndirectlyPoweringTo(World& w, int x, int y, int z, int side) {
    int id = w.getBlockId(x, y, z);
    switch (id) {
    case WIRE: return wiresProvidePower && isPoweringTo(w, x, y, z, side);
    case TORCH_ON: return side == 0 && isPoweringTo(w, x, y, z, side);
    case LEVER: case BUTTON: return attachedPowering(w.getBlockMetadata(x, y, z), side, 8);
    case PLATE_STONE: case PLATE_WOOD: case PLATE_CHARRED: return (w.getBlockMetadata(x, y, z) & 1) != 0 && side == 1;
    case REPEATER_LIT: return isPoweringTo(w, x, y, z, side);
    default: return false;
    }
}

bool isBlockProvidingPowerTo(World& w, int x, int y, int z, int side) {
    return w.getBlockId(x, y, z) != 0 && isIndirectlyPoweringTo(w, x, y, z, side);
}
bool isBlockGettingPowered(World& w, int x, int y, int z) {
    return isBlockProvidingPowerTo(w, x, y - 1, z, 0) || isBlockProvidingPowerTo(w, x, y + 1, z, 1) ||
           isBlockProvidingPowerTo(w, x, y, z - 1, 2) || isBlockProvidingPowerTo(w, x, y, z + 1, 3) ||
           isBlockProvidingPowerTo(w, x - 1, y, z, 4) || isBlockProvidingPowerTo(w, x + 1, y, z, 5);
}
bool isBlockIndirectlyProvidingPowerTo(World& w, int x, int y, int z, int side) {
    int id = w.getBlockId(x, y, z);
    if (id > 0 && canProvidePower(id) && (id == BLOCK_REDSTONE || id == REDSTONE_BRICKS)) return true; // Axeolon power blocks
    if (opaque(w, x, y, z)) return isBlockGettingPowered(w, x, y, z);
    return id != 0 && isPoweringTo(w, x, y, z, side);
}
bool isBlockIndirectlyGettingPowered(World& w, int x, int y, int z) {
    return isBlockIndirectlyProvidingPowerTo(w, x, y - 1, z, 0) || isBlockIndirectlyProvidingPowerTo(w, x, y + 1, z, 1) ||
           isBlockIndirectlyProvidingPowerTo(w, x, y, z - 1, 2) || isBlockIndirectlyProvidingPowerTo(w, x, y, z + 1, 3) ||
           isBlockIndirectlyProvidingPowerTo(w, x - 1, y, z, 4) || isBlockIndirectlyProvidingPowerTo(w, x + 1, y, z, 5);
}

// ================================================================ lapis network

bool isLapisDust(int id) { return isDust(id); }

void lapisUpdate(World& w, int x, int y, int z) { // AxeolonLapis.update
    if (lapisBusy || !isDust(w.getBlockId(x, y, z))) return;
    std::unordered_map<int64_t, int> power;
    std::vector<P3> all, near;
    std::deque<P3> open;
    power[key(x, y, z)] = -1;
    open.push_back({x, y, z});
    while (!open.empty() && all.size() < 1024) {
        P3 p = open.front();
        open.pop_front();
        all.push_back(p);
        neighbours(w, p.x, p.y, p.z, near);
        for (const P3& n : near) {
            int64_t k = key(n.x, n.y, n.z);
            if (!power.count(k)) {
                power[k] = -1;
                open.push_back(n);
            }
        }
    }
    std::deque<P3> q;
    for (const P3& p : all)
        if (touchesSource(w, p.x, p.y, p.z)) {
            power[key(p.x, p.y, p.z)] = 15;
            q.push_back(p);
        }
    while (!q.empty()) {
        P3 p = q.front();
        q.pop_front();
        int pw = power[key(p.x, p.y, p.z)];
        if (pw <= 1) continue;
        neighbours(w, p.x, p.y, p.z, near);
        for (const P3& n : near) {
            auto it = power.find(key(n.x, n.y, n.z));
            if (it != power.end() && it->second < pw - 1) {
                it->second = pw - 1;
                q.push_back(n);
            }
        }
    }
    std::vector<P3> changed;
    for (const P3& p : all) {
        int pw = std::max(0, power[key(p.x, p.y, p.z)]);
        if (w.getBlockMetadata(p.x, p.y, p.z) != pw) {
            w.setBlockMetadata(p.x, p.y, p.z, pw);
            w.markBlockNeedsUpdate(p.x, p.y, p.z);
            changed.push_back(p);
        }
    }
    lapisBusy = true;
    for (const P3& p : changed) w.notifyBlocksOfNeighborChange(p.x, p.y, p.z, LAPIS_WIRE);
    lapisBusy = false;
    if (!lapisHealing) {
        lapisHealing = true;
        for (const P3& p : all)
            for (int dy = -1; dy <= 1; dy += 2)
                for (int j = 2; j < 6; ++j) {
                    int nx = p.x + DX[j], ny = p.y + dy, nz = p.z + DZ[j];
                    if (isDust(w.getBlockId(nx, ny, nz)) && !power.count(key(nx, ny, nz)) && w.getBlockMetadata(nx, ny, nz) > 0)
                        lapisUpdate(w, nx, ny, nz);
                }
        lapisHealing = false;
    }
}

void lapisAround(World& w, int x, int y, int z) { // AxeolonLapis.around
    if (lapisBusy) return;
    for (int i = 0; i < 6; ++i) {
        int nx = x + DX[i], ny = y + DY[i], nz = z + DZ[i];
        if (isDust(w.getBlockId(nx, ny, nz))) lapisUpdate(w, nx, ny, nz);
    }
}

// ================================================================ block events

bool handles(int id) {
    switch (BP[id].cls) {
    case BC::RedstoneWire: case BC::RedstoneTorch: case BC::Lever: case BC::Button: case BC::PressurePlate:
    case BC::PowerRepeater: case BC::LapisWire: case BC::LapisTorch: case BC::LapisLamp: case BC::Door:
    case BC::TNT: case BC::Note: case BC::PistonBase: case BC::PistonHead: case BC::Dispenser:
        return true;
    case BC::PowerBlock: return id == LAPIS_BRICKS;
    default: return false;
    }
}

void onBlockAdded(World& w, int x, int y, int z, int id) {
    switch (BP[id].cls) {
    case BC::RedstoneWire:
        wireUpdate(w, x, y, z);
        w.notifyBlocksOfNeighborChange(x, y + 1, z, WIRE);
        w.notifyBlocksOfNeighborChange(x, y - 1, z, WIRE);
        wireAroundNotify(w, x, y, z);
        break;
    case BC::RedstoneTorch:
        if (id == TORCH_ON) notifyAll6(w, x, y, z, id);
        break;
    case BC::PowerRepeater: w.notifyBlocksOfNeighborChange(x, y, z, id); break;
    case BC::LapisWire: lapisUpdate(w, x, y, z); break;
    case BC::LapisTorch:
        lapisAround(w, x, y, z);
        w.scheduleBlockUpdate(x, y, z, id);
        break;
    case BC::LapisLamp: {
        bool on = signalAt(w, x, y, z);
        if (on && id == LAPIS_LAMP) w.setBlockWithNotify(x, y, z, LAPIS_LAMP_LIT);
        else if (!on && id == LAPIS_LAMP_LIT) w.scheduleBlockUpdateIn(x, y, z, id, 4);
        break;
    }
    case BC::PowerBlock: lapisAround(w, x, y, z); break;
    default: break;
    }
}

void onBlockRemoval(World& w, int x, int y, int z, int id, int meta) {
    switch (BP[id].cls) {
    case BC::RedstoneWire:
        w.notifyBlocksOfNeighborChange(x, y + 1, z, WIRE);
        w.notifyBlocksOfNeighborChange(x, y - 1, z, WIRE);
        wireUpdate(w, x, y, z);
        wireAroundNotify(w, x, y, z);
        break;
    case BC::RedstoneTorch:
        if (id == TORCH_ON) notifyAll6(w, x, y, z, id);
        break;
    case BC::Lever:
        if (meta & 8) notifyAttached(w, x, y, z, id, meta & 7);
        break;
    case BC::Button:
        if (meta & 8) notifyAttached(w, x, y, z, id, meta & 7);
        break;
    case BC::PressurePlate:
        if (meta & 1) {
            w.notifyBlocksOfNeighborChange(x, y, z, id);
            w.notifyBlocksOfNeighborChange(x, y - 1, z, id);
        }
        break;
    case BC::PowerRepeater: {
        int fx, fz;
        front(meta, fx, fz);
        w.notifyBlocksOfNeighborChange(x + fx, y, z + fz, id);
        lapisAround(w, x, y, z);
        break;
    }
    case BC::LapisWire:
        lapisAround(w, x, y, z);
        w.notifyBlocksOfNeighborChange(x, y, z, id);
        break;
    case BC::LapisTorch:
        lapisAround(w, x, y, z);
        w.notifyBlocksOfNeighborChange(x, y, z, id);
        break;
    case BC::PowerBlock: lapisAround(w, x, y, z); break;
    case BC::PistonBase: // take the head with it
        if (pistonMoving) break;
        for (int i = 0; i < 6; ++i) {
            int hx = x + FX[i], hy = y + FY[i], hz = z + FZ[i];
            if (w.getBlockId(hx, hy, hz) == HEAD && facingOf(w.getBlockMetadata(hx, hy, hz)) == i) {
                pistonMoving = true;
                w.setBlockWithNotify(hx, hy, hz, 0);
                pistonMoving = false;
            }
        }
        break;
    case BC::PistonHead: // broken by hand: the piston behind it goes too (and drops)
        if (pistonMoving) break;
        for (int i = 0; i < 6; ++i) {
            int bx = x - FX[i], by = y - FY[i], bz = z - FZ[i];
            int bid = w.getBlockId(bx, by, bz), bm = w.getBlockMetadata(bx, by, bz);
            if ((bid == PISTON || bid == STICKY) && (bm & 8) && facingOf(bm) == i) {
                Blocks::dropBlockAsItem(w, bx, by, bz, bid, 0);
                w.setBlockWithNotify(bx, by, bz, 0);
            }
        }
        break;
    default: break;
    }
}

void onNeighborBlockChange(World& w, int x, int y, int z, int id, int neighbour) {
    switch (BP[id].cls) {
    case BC::RedstoneWire:
        if (!opaque(w, x, y - 1, z)) {
            Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        } else {
            wireUpdate(w, x, y, z);
        }
        break;
    case BC::RedstoneTorch: case BC::LapisTorch: {
        // BlockTorch.onNeighborBlockChange: falls off without its block
        int m = w.getBlockMetadata(x, y, z);
        bool drop = (m == 1 && !opaque(w, x - 1, y, z)) || (m == 2 && !opaque(w, x + 1, y, z)) ||
                    (m == 3 && !opaque(w, x, y, z - 1)) || (m == 4 && !opaque(w, x, y, z + 1)) ||
                    (m == 5 && !opaque(w, x, y - 1, z));
        if (drop) {
            Blocks::dropBlockAsItem(w, x, y, z, id, m);
            w.setBlockWithNotify(x, y, z, 0);
            break;
        }
        if (w.getBlockId(x, y, z) == id) w.scheduleBlockUpdate(x, y, z, id);
        break;
    }
    case BC::Lever: case BC::Button: {
        int m = w.getBlockMetadata(x, y, z) & 7;
        bool drop = (m == 1 && !opaque(w, x - 1, y, z)) || (m == 2 && !opaque(w, x + 1, y, z)) ||
                    (m == 3 && !opaque(w, x, y, z - 1)) || (m == 4 && !opaque(w, x, y, z + 1)) ||
                    ((m == 5 || m == 6) && !opaque(w, x, y - 1, z));
        if (drop) {
            Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    }
    case BC::PressurePlate:
        if (!opaque(w, x, y - 1, z)) {
            Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::PowerRepeater: {
        if (!opaque(w, x, y - 1, z)) {
            Blocks::dropBlockAsItem(w, x, y, z, REPEATER, 0);
            w.setBlockWithNotify(x, y, z, 0);
            break;
        }
        int meta = w.getBlockMetadata(x, y, z), fx, fz;
        front(meta, fx, fz);
        int bx = x - fx, bz = z - fz;
        bool in = isBlockIndirectlyProvidingPowerTo(w, bx, y, bz, sideFor(fx, fz)) ||
                  (isDust(w.getBlockId(bx, y, bz)) && w.getBlockMetadata(bx, y, bz) > 0) || sourceInto(w, bx, y, bz, x, y, z);
        if (in != (id == REPEATER_LIT)) w.scheduleBlockUpdateIn(x, y, z, id, repeaterDelay(meta) * 2);
        break;
    }
    case BC::LapisWire: {
        int wall = wallOf(id);
        auto supported = [&] {
            return opaque(w, x, y - 1, z) || opaque(w, x - 1, y, z) || opaque(w, x + 1, y, z) || opaque(w, x, y, z - 1) ||
                   opaque(w, x, y, z + 1);
        };
        if (wall >= 0 && !opaque(w, x + WALL_DX[wall], y, z + WALL_DZ[wall])) {
            int meta = w.getBlockMetadata(x, y, z);
            if (supported()) w.setBlockAndMetadataWithNotify(x, y, z, LAPIS_WIRE, meta);
            else {
                Blocks::dropBlockAsItem(w, x, y, z, id, meta);
                w.setBlockWithNotify(x, y, z, 0);
            }
            break;
        }
        if (!supported()) {
            Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
            break;
        }
        lapisUpdate(w, x, y, z);
        break;
    }
    case BC::LapisLamp: {
        bool on = signalAt(w, x, y, z);
        if (on && id == LAPIS_LAMP) w.setBlockWithNotify(x, y, z, LAPIS_LAMP_LIT);
        else if (!on && id == LAPIS_LAMP_LIT) w.scheduleBlockUpdateIn(x, y, z, id, 4);
        break;
    }
    case BC::Door: { // BlockDoor.onNeighborBlockChange
        int meta = w.getBlockMetadata(x, y, z);
        if (meta & 8) {
            if (w.getBlockId(x, y - 1, z) != id) w.setBlockWithNotify(x, y, z, 0);
            if (neighbour > 0 && canProvidePower(neighbour)) onNeighborBlockChange(w, x, y - 1, z, id, neighbour);
        } else {
            bool broke = false;
            if (w.getBlockId(x, y + 1, z) != id) {
                w.setBlockWithNotify(x, y, z, 0);
                broke = true;
            }
            if (!opaque(w, x, y - 1, z)) {
                w.setBlockWithNotify(x, y, z, 0);
                broke = true;
                if (w.getBlockId(x, y + 1, z) == id) w.setBlockWithNotify(x, y + 1, z, 0);
            }
            if (broke) Blocks::dropBlockAsItem(w, x, y, z, id, meta);
            else if (neighbour > 0 && canProvidePower(neighbour))
                doorPower(w, x, y, z, isBlockIndirectlyGettingPowered(w, x, y, z) || isBlockIndirectlyGettingPowered(w, x, y + 1, z));
        }
        break;
    }
    case BC::PistonBase:
        if (!pistonMoving) pistonCheck(w, x, y, z, id);
        break;
    case BC::Dispenser: // redstone only: lapis goes through a power repeater first
        if (neighbour > 0 && canProvidePower(neighbour) &&
            (isBlockIndirectlyGettingPowered(w, x, y, z) || isBlockIndirectlyGettingPowered(w, x, y + 1, z)))
            w.scheduleBlockUpdate(x, y, z, id);
        break;
    case BC::PistonHead: {
        if (pistonMoving) break;
        int f = facingOf(w.getBlockMetadata(x, y, z));
        int bx = x - FX[f], by = y - FY[f], bz = z - FZ[f];
        int bid = w.getBlockId(bx, by, bz);
        if (!((bid == PISTON || bid == STICKY) && (w.getBlockMetadata(bx, by, bz) & 8))) w.setBlockWithNotify(x, y, z, 0);
        break;
    }
    case BC::TNT:
        if (neighbour > 0 && canProvidePower(neighbour) && isBlockIndirectlyGettingPowered(w, x, y, z)) {
            if (Blocks::onTntIgnited) Blocks::onTntIgnited(x, y, z);
            w.setBlockWithNotify(x, y, z, 0);
        }
        break;
    case BC::Note:
        if (neighbour > 0 && canProvidePower(neighbour)) {
            bool on = isBlockGettingPowered(w, x, y, z);
            int64_t k = key(x, y, z);
            bool was = notePowered.count(k) != 0;
            if (was != on) {
                if (on) {
                    playNote(w, x, y, z);
                    notePowered.insert(k);
                } else {
                    notePowered.erase(k);
                }
            }
        }
        break;
    default: break;
    }
}

void updateTick(World& w, int x, int y, int z, int id, JavaRandom& r) {
    switch (BP[id].cls) {
    case BC::RedstoneTorch: {
        bool powered = torchGettingPowered(w, x, y, z);
        while (!torchUpdates.empty() && w.info.time - torchUpdates.front().time > 100) torchUpdates.pop_front();
        if (id == TORCH_ON) {
            if (powered) {
                w.setBlockAndMetadataWithNotify(x, y, z, TORCH_OFF, w.getBlockMetadata(x, y, z));
                if (checkForBurnout(w, x, y, z, true)) {
                    sound(w, "random.fizz", x + 0.5, y + 0.5, z + 0.5, 0.5f, 2.6f + (w.rand.nextFloat() - w.rand.nextFloat()) * 0.8f);
                    for (int i = 0; i < 5; ++i)
                        particle(w, 1, x + r.nextDouble() * 0.6 + 0.2, y + r.nextDouble() * 0.6 + 0.2, z + r.nextDouble() * 0.6 + 0.2, 0, 0, 0);
                }
            }
        } else if (!powered && !checkForBurnout(w, x, y, z, false)) {
            w.setBlockAndMetadataWithNotify(x, y, z, TORCH_ON, w.getBlockMetadata(x, y, z));
        }
        break;
    }
    case BC::Button: { // pops back out
        int meta = w.getBlockMetadata(x, y, z);
        if (meta & 8) {
            w.setBlockMetadataWithNotify(x, y, z, meta & 7);
            anim(w, ANIM_BUTTON, x, y, z, "release");
            notifyAttached(w, x, y, z, id, meta & 7);
            sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.3f, 0.5f);
            w.markBlockNeedsUpdate(x, y, z);
        }
        break;
    }
    case BC::PressurePlate:
        if (w.getBlockMetadata(x, y, z) & 1) plateState(w, x, y, z);
        break;
    case BC::PowerRepeater: {
        int meta = w.getBlockMetadata(x, y, z), fx, fz;
        front(meta, fx, fz);
        int bx = x - fx, bz = z - fz;
        bool in = isBlockIndirectlyProvidingPowerTo(w, bx, y, bz, sideFor(fx, fz)) ||
                  (isDust(w.getBlockId(bx, y, bz)) && w.getBlockMetadata(bx, y, bz) > 0) || sourceInto(w, bx, y, bz, x, y, z);
        if (in && id == REPEATER) w.setBlockAndMetadataWithNotify(x, y, z, REPEATER_LIT, meta);
        else if (!in && id == REPEATER_LIT) w.setBlockAndMetadataWithNotify(x, y, z, REPEATER, meta);
        else break;
        w.notifyBlocksOfNeighborChange(x + fx, y, z + fz, REPEATER);
        lapisAround(w, x, y, z);
        break;
    }
    case BC::LapisWire:
        if (w.getBlockMetadata(x, y, z) > 0) lapisUpdate(w, x, y, z);
        break;
    case BC::LapisTorch: {
        int m = w.getBlockMetadata(x, y, z);
        int ax = 0, ay = -1, az = 0;
        if (m == 1) { ax = -1; ay = 0; } else if (m == 2) { ax = 1; ay = 0; }
        else if (m == 3) { az = -1; ay = 0; } else if (m == 4) { az = 1; ay = 0; }
        bool off = blockLapisPowered(w, x + ax, y + ay, z + az);
        if (id == LAPIS_TORCH && off) {
            w.setBlockAndMetadataWithNotify(x, y, z, LAPIS_TORCH_OFF, m);
            lapisAround(w, x, y, z);
        } else if (id == LAPIS_TORCH_OFF && !off) {
            w.setBlockAndMetadataWithNotify(x, y, z, LAPIS_TORCH, m);
            lapisAround(w, x, y, z);
        }
        break;
    }
    case BC::LapisLamp:
        if (id == LAPIS_LAMP_LIT && !signalAt(w, x, y, z)) w.setBlockWithNotify(x, y, z, LAPIS_LAMP);
        break;
    case BC::Dispenser:
        if ((isBlockIndirectlyGettingPowered(w, x, y, z) || isBlockIndirectlyGettingPowered(w, x, y + 1, z)) && w.onDispense) {
            anim(w, 7, x, y, z, "fire"); // AxeolonBlockAnims.DISPENSER
            w.onDispense(x, y, z);
        }
        break;
    case BC::PistonBase: {
        int meta = w.getBlockMetadata(x, y, z), f = facingOf(meta);
        bool p = pistonPowered(w, x, y, z, f);
        if (p && !(meta & 8)) pistonExtend(w, x, y, z, f, id == STICKY);
        else if (!p && (meta & 8)) pistonRetract(w, x, y, z, f, id == STICKY);
        break;
    }
    default: break;
    }
}

void pistonPlaced(World& w, int x, int y, int z, double px, double py, double pz, float yaw) {
    int f;
    double eye = py + 1.82 - 1.62; // posY + 1.82 - yOffset
    if (std::fabs(px - (x + 0.5)) < 2.0 && std::fabs(pz - (z + 0.5)) < 2.0 && eye - y > 2.0) f = 1;
    else if (std::fabs(px - (x + 0.5)) < 2.0 && std::fabs(pz - (z + 0.5)) < 2.0 && y - eye > 0.0) f = 0;
    else {
        int l = int(std::floor(double(yaw * 4.0f / 360.0f) + 0.5)) & 3;
        f = l == 0 ? 2 : l == 1 ? 5 : l == 2 ? 3 : 4;
    }
    w.setBlockMetadataWithNotify(x, y, z, f);
    pistonCheck(w, x, y, z, w.getBlockId(x, y, z));
}

bool blockActivated(World& w, int x, int y, int z, bool emptyHand) {
    int id = w.getBlockId(x, y, z), meta = w.getBlockMetadata(x, y, z);
    switch (BP[id].cls) {
    case BC::PistonBase: // an empty hand turns it to the next way
        if (!emptyHand || (meta & 8)) return false;
        w.setBlockMetadataWithNotify(x, y, z, (facingOf(meta) + 1) % 6);
        sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.3f, 0.8f);
        pistonCheck(w, x, y, z, id);
        return true;
    case BC::Lever: { // BlockLever.blockActivated
        int on = 8 - (meta & 8);
        w.setBlockMetadataWithNotify(x, y, z, (meta & 7) + on);
        anim(w, ANIM_LEVER, x, y, z, on ? "on" : "off");
        w.markBlockNeedsUpdate(x, y, z);
        sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.3f, on ? 0.6f : 0.5f);
        notifyAttached(w, x, y, z, id, meta & 7);
        return true;
    }
    case BC::Button: { // BlockButton.blockActivated
        if (meta & 8) return true;
        w.setBlockMetadataWithNotify(x, y, z, (meta & 7) | 8);
        anim(w, ANIM_BUTTON, x, y, z, "press");
        w.markBlockNeedsUpdate(x, y, z);
        sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.3f, 0.6f);
        notifyAttached(w, x, y, z, id, meta & 7);
        w.scheduleBlockUpdate(x, y, z, id);
        return true;
    }
    case BC::PowerRepeater: { // a click turns the delay up
        int d = ((meta >> 2 & 3) + 1) & 3;
        w.setBlockMetadataWithNotify(x, y, z, (meta & 3) | d << 2);
        sound(w, "random.click", x + 0.5, y + 0.5, z + 0.5, 0.3f, 0.6f);
        return true;
    }
    case BC::Note: { // TileEntityNote.changePitch + triggerNote
        nbt::Tag* te = w.tileEntityAt(x, y, z);
        if (!te) te = w.addTileEntity(x, y, z, "Music");
        if (te) te->setByte("note", int8_t((te->getByte("note") + 1) % 25));
        playNote(w, x, y, z);
        return true;
    }
    default: return false;
    }
}

void blockClicked(World& w, int x, int y, int z) {
    if (w.getBlockId(x, y, z) == NOTE) playNote(w, x, y, z);
}

void entityTouching(World& w, const AABB& box, bool living) {
    int x0 = int(std::floor(box.minX + 0.001)), y0 = int(std::floor(box.minY + 0.001)), z0 = int(std::floor(box.minZ + 0.001));
    int x1 = int(std::floor(box.maxX - 0.001)), y1 = int(std::floor(box.maxY - 0.001)), z1 = int(std::floor(box.maxZ - 0.001));
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z) {
                int id = w.getBlockId(x, y, z);
                if (!isPlate(id) || (w.getBlockMetadata(x, y, z) & 1)) continue;
                if (id == PLATE_STONE && !living) continue;
                plateState(w, x, y, z);
            }
}

} // namespace Redstone
