#include "Redstone.h"
#include <cmath>
#include <deque>
#include "BlockProps.h"
#include "Blocks.h"
#include "Physics.h"
#include "World.h"

namespace Redstone {

bool (*anythingOn)(const AABB& box, int kind) = nullptr;

namespace {
enum : int {
    WIRE = 55, TORCH_IDLE = 75, TORCH_ACTIVE = 76, LEVER = 69, BUTTON = 77, PLATE_STONE = 70, PLATE_WOOD = 72,
    PLATE_IRON = 96, DOOR_WOOD = 64, DOOR_IRON = 71, TNT = 46, MOTOR = 90,
};

JavaRandom mathRandom{0x5EED0D00LL}; // Math.random() (the doors' sound)
bool wiresProvidePower = true;       // BlockRedDustWire.wiresProvidePower

void sound(World& w, const char* n, double x, double y, double z, float v, float p) {
    if (w.onSound) w.onSound(n, x, y, z, v, p);
}
void particle(World& w, int type, double x, double y, double z) {
    if (w.onParticle) w.onParticle(type, x, y, z, 0, 0, 0);
}
bool normalCube(World& w, int x, int y, int z) { return w.isBlockOpaqueCube(x, y, z); } // World.isBlockNormalCube
void drop(World& w, int x, int y, int z, int id) {
    Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
    w.setBlockWithNotify(x, y, z, 0);
}
void notifyAll6(World& w, int x, int y, int z, int id) { // the order BlockRedTorch uses
    w.notifyBlocksOfNeighborChange(x, y - 1, z, id);
    w.notifyBlocksOfNeighborChange(x, y + 1, z, id);
    w.notifyBlocksOfNeighborChange(x - 1, y, z, id);
    w.notifyBlocksOfNeighborChange(x + 1, y, z, id);
    w.notifyBlocksOfNeighborChange(x, y, z - 1, id);
    w.notifyBlocksOfNeighborChange(x, y, z + 1, id);
}

// ---------------------------------------------------------------- BlockRedDustWire

bool providerOrWire(World& w, int x, int y, int z) { // isPowerProviderOrWire
    int id = w.getBlockId(x, y, z);
    return id == WIRE || (id != 0 && canProvidePower(id));
}
int maxCurrentStrength(World& w, int x, int y, int z, int l) { // getMaxCurrentStrength
    if (w.getBlockId(x, y, z) != WIRE) return l;
    int m = w.getBlockMetadata(x, y, z);
    return m > l ? m : l;
}
void notifyWire(World& w, int x, int y, int z) { // the wire and its six neighbours
    w.notifyBlocksOfNeighborChange(x, y, z, WIRE);
    w.notifyBlocksOfNeighborChange(x - 1, y, z, WIRE);
    w.notifyBlocksOfNeighborChange(x + 1, y, z, WIRE);
    w.notifyBlocksOfNeighborChange(x, y, z - 1, WIRE);
    w.notifyBlocksOfNeighborChange(x, y, z + 1, WIRE);
    w.notifyBlocksOfNeighborChange(x, y - 1, z, WIRE);
    w.notifyBlocksOfNeighborChange(x, y + 1, z, WIRE);
}
void wireUpdate(World& w, int i, int j, int k) { // func_280_h
    int l = w.getBlockMetadata(i, j, k);
    int i1 = 0;
    wiresProvidePower = false;
    bool flag = isBlockIndirectlyGettingPowered(w, i, j, k);
    wiresProvidePower = true;
    if (flag) {
        i1 = 15;
    } else {
        for (int j1 = 0; j1 < 4; ++j1) {
            int l1 = i, j2 = k;
            if (j1 == 0) l1 = i - 1;
            if (j1 == 1) ++l1;
            if (j1 == 2) j2 = k - 1;
            if (j1 == 3) ++j2;
            i1 = maxCurrentStrength(w, l1, j, j2, i1);
            if (normalCube(w, l1, j, j2) && !normalCube(w, i, j + 1, k)) i1 = maxCurrentStrength(w, l1, j + 1, j2, i1);
            else if (!normalCube(w, l1, j, j2)) i1 = maxCurrentStrength(w, l1, j - 1, j2, i1);
        }
        i1 = i1 > 0 ? i1 - 1 : 0;
    }
    if (l == i1) return;
    w.setBlockMetadataWithNotify(i, j, k, i1);
    w.markBlocksDirty(i, j, k, i, j, k);
    if (i1 > 0) --i1;
    for (int k1 = 0; k1 < 4; ++k1) {
        int i2 = i, k2 = k, l2 = j - 1;
        if (k1 == 0) i2 = i - 1;
        if (k1 == 1) ++i2;
        if (k1 == 2) k2 = k - 1;
        if (k1 == 3) ++k2;
        if (normalCube(w, i2, j, k2)) l2 += 2;
        int i3 = maxCurrentStrength(w, i2, j, k2, -1);
        if (i3 >= 0 && i3 != i1) wireUpdate(w, i2, j, k2);
        i3 = maxCurrentStrength(w, i2, l2, k2, -1);
        if (i3 >= 0 && i3 != i1) wireUpdate(w, i2, l2, k2);
    }
    if (l == 0 || i1 == 0) notifyWire(w, i, j, k);
}
void notifyWireNeighborsOfNeighborChange(World& w, int x, int y, int z) {
    if (w.getBlockId(x, y, z) == WIRE) notifyWire(w, x, y, z);
}
void wireAround(World& w, int i, int j, int k) { // the tail of onBlockAdded / onBlockRemoval
    notifyWireNeighborsOfNeighborChange(w, i - 1, j, k);
    notifyWireNeighborsOfNeighborChange(w, i + 1, j, k);
    notifyWireNeighborsOfNeighborChange(w, i, j, k - 1);
    notifyWireNeighborsOfNeighborChange(w, i, j, k + 1);
    notifyWireNeighborsOfNeighborChange(w, i - 1, normalCube(w, i - 1, j, k) ? j + 1 : j - 1, k);
    notifyWireNeighborsOfNeighborChange(w, i + 1, normalCube(w, i + 1, j, k) ? j + 1 : j - 1, k);
    notifyWireNeighborsOfNeighborChange(w, i, normalCube(w, i, j, k - 1) ? j + 1 : j - 1, k - 1);
    notifyWireNeighborsOfNeighborChange(w, i, normalCube(w, i, j, k + 1) ? j + 1 : j - 1, k + 1);
}
bool wirePoweringTo(World& w, int i, int j, int k, int l) {
    if (!wiresProvidePower) return false;
    if (w.getBlockMetadata(i, j, k) == 0) return false;
    if (l == 1) return true;
    bool flag = providerOrWire(w, i - 1, j, k) || (!normalCube(w, i - 1, j, k) && providerOrWire(w, i - 1, j - 1, k));
    bool flag1 = providerOrWire(w, i + 1, j, k) || (!normalCube(w, i + 1, j, k) && providerOrWire(w, i + 1, j - 1, k));
    bool flag2 = providerOrWire(w, i, j, k - 1) || (!normalCube(w, i, j, k - 1) && providerOrWire(w, i, j - 1, k - 1));
    bool flag3 = providerOrWire(w, i, j, k + 1) || (!normalCube(w, i, j, k + 1) && providerOrWire(w, i, j - 1, k + 1));
    if (!normalCube(w, i, j + 1, k)) {
        if (normalCube(w, i - 1, j, k) && providerOrWire(w, i - 1, j + 1, k)) flag = true;
        if (normalCube(w, i + 1, j, k) && providerOrWire(w, i + 1, j + 1, k)) flag1 = true;
        if (normalCube(w, i, j, k - 1) && providerOrWire(w, i, j + 1, k - 1)) flag2 = true;
        if (normalCube(w, i, j, k + 1) && providerOrWire(w, i, j + 1, k + 1)) flag3 = true;
    }
    if (!flag2 && !flag1 && !flag && !flag3 && l >= 2 && l <= 5) return true;
    if (l == 2 && flag2 && !flag && !flag1) return true;
    if (l == 3 && flag3 && !flag && !flag1) return true;
    return (l == 4 && flag && !flag2 && !flag3) || (l == 5 && flag1 && !flag2 && !flag3);
}

// ---------------------------------------------------------------- BlockRedTorch

struct RedPowerUpdateInfo { int x, y, z; int64_t time; };
std::deque<RedPowerUpdateInfo> torchUpdates; // static in the jar: shared by every world
bool checkForBurnout(World& w, int x, int y, int z, bool add) {
    if (add) torchUpdates.push_back({x, y, z, w.info.time});
    int n = 0;
    for (const RedPowerUpdateInfo& u : torchUpdates)
        if (u.x == x && u.y == y && u.z == z && ++n >= 8) return true;
    return false;
}
bool torchPoweringTo(World& w, int x, int y, int z, int l) { // (the lit torch)
    int i1 = w.getBlockMetadata(x, y, z);
    if (i1 == 5 && l == 1) return false;
    if (i1 == 3 && l == 3) return false;
    if (i1 == 4 && l == 2) return false;
    if (i1 == 1 && l == 5) return false;
    return i1 != 2 || l != 4;
}
bool torchPowered(World& w, int i, int j, int k) { // func_272_h: the block it stands on is powered
    int l = w.getBlockMetadata(i, j, k);
    if (l == 5 && isBlockIndirectlyProvidingPowerTo(w, i, j - 1, k, 0)) return true;
    if (l == 3 && isBlockIndirectlyProvidingPowerTo(w, i, j, k - 1, 2)) return true;
    if (l == 4 && isBlockIndirectlyProvidingPowerTo(w, i, j, k + 1, 3)) return true;
    return (l == 1 && isBlockIndirectlyProvidingPowerTo(w, i - 1, j, k, 4)) ||
           (l == 2 && isBlockIndirectlyProvidingPowerTo(w, i + 1, j, k, 5));
}

// ---------------------------------------------------------------- BlockLever / BlockButton

// the block it's on hears it too (blockActivated, onBlockRemoval, the button's updateTick)
void notifyAttached(World& w, int i, int j, int k, int id, int i1) {
    w.notifyBlocksOfNeighborChange(i, j, k, id);
    if (i1 == 1) w.notifyBlocksOfNeighborChange(i - 1, j, k, id);
    else if (i1 == 2) w.notifyBlocksOfNeighborChange(i + 1, j, k, id);
    else if (i1 == 3) w.notifyBlocksOfNeighborChange(i, j, k - 1, id);
    else if (i1 == 4) w.notifyBlocksOfNeighborChange(i, j, k + 1, id);
    else w.notifyBlocksOfNeighborChange(i, j - 1, k, id);
}
bool leverCanPlaceAt(World& w, int i, int j, int k) { // canPlaceBlockAt
    return normalCube(w, i - 1, j, k) || normalCube(w, i + 1, j, k) || normalCube(w, i, j, k - 1) ||
           normalCube(w, i, j, k + 1) || normalCube(w, i, j - 1, k);
}
bool buttonCanPlace(World& w, int i, int j, int k) { // canPlace
    return normalCube(w, i - 1, j, k) || normalCube(w, i + 1, j, k) || normalCube(w, i, j, k - 1) || normalCube(w, i, j, k + 1);
}

// ---------------------------------------------------------------- BlockPressurePlate

int plateKind(int id) { return id == PLATE_WOOD ? 0 : id == PLATE_STONE ? 1 : 2; } // EnumMobType everything, mobs, players
void plateCheck(World& w, int i, int j, int k) { // func_318_h
    int id = w.getBlockId(i, j, k);
    bool flag = w.getBlockMetadata(i, j, k) == 1;
    const float f = 0.125f;
    AABB box{i + double(f), double(j), k + double(f), i + 1 - double(f), j + 0.25, k + 1 - double(f)};
    bool flag1 = anythingOn && anythingOn(box, plateKind(id));
    if (flag1 && !flag) {
        w.setBlockMetadataWithNotify(i, j, k, 1);
        w.notifyBlocksOfNeighborChange(i, j, k, id);
        w.notifyBlocksOfNeighborChange(i, j - 1, k, id);
        w.markBlocksDirty(i, j, k, i, j, k);
        sound(w, "random.click", i + 0.5, j + 0.1, k + 0.5, 0.3f, 0.6f);
    }
    if (!flag1 && flag) {
        w.setBlockMetadataWithNotify(i, j, k, 0);
        w.notifyBlocksOfNeighborChange(i, j, k, id);
        w.notifyBlocksOfNeighborChange(i, j - 1, k, id);
        w.markBlocksDirty(i, j, k, i, j, k);
        sound(w, "random.click", i + 0.5, j + 0.1, k + 0.5, 0.3f, 0.5f);
    }
    if (flag1) w.scheduleBlockUpdate(i, j, k, id);
}

// ---------------------------------------------------------------- BlockDoor

void doorSound(World& w, int i, int j, int k) {
    const char* n = mathRandom.nextDouble() < 0.5 ? "random.door_open" : "random.door_close";
    sound(w, n, i + 0.5, j + 0.5, k + 0.5, 1.0f, w.rand.nextFloat() * 0.1f + 0.9f);
}
void doorToggle(World& w, int i, int j, int k, int id, int l) {
    if (w.getBlockId(i, j + 1, k) == id) w.setBlockMetadataWithNotify(i, j + 1, k, (l ^ 4) + 8);
    w.setBlockMetadataWithNotify(i, j, k, l ^ 4);
    w.markBlocksDirty(i, j - 1, k, i, j, k);
    doorSound(w, i, j, k);
}
bool doorActivated(World& w, int i, int j, int k, int id) {
    if (id == DOOR_IRON) return true;
    int l = w.getBlockMetadata(i, j, k);
    if (l & 8) {
        if (w.getBlockId(i, j - 1, k) == id) doorActivated(w, i, j - 1, k, id);
        return true;
    }
    doorToggle(w, i, j, k, id, l);
    return true;
}
void doorPower(World& w, int i, int j, int k, int id, bool flag) { // func_311_a
    int l = w.getBlockMetadata(i, j, k);
    if (l & 8) {
        if (w.getBlockId(i, j - 1, k) == id) doorPower(w, i, j - 1, k, id, flag);
        return;
    }
    bool open = (l & 4) > 0;
    if (open != flag) doorToggle(w, i, j, k, id, l);
}
} // namespace

// ================================================================ power

bool canProvidePower(int id) {
    switch (id) {
    case WIRE: return wiresProvidePower;
    case TORCH_IDLE: case TORCH_ACTIVE: case LEVER: case BUTTON: case PLATE_STONE: case PLATE_WOOD: case PLATE_IRON:
        return true;
    default: return false;
    }
}

bool isPoweringTo(World& w, int x, int y, int z, int side) {
    switch (w.getBlockId(x, y, z)) {
    case WIRE: return wirePoweringTo(w, x, y, z, side);
    case TORCH_ACTIVE: return torchPoweringTo(w, x, y, z, side);
    case LEVER: case BUTTON: return (w.getBlockMetadata(x, y, z) & 8) > 0;
    case PLATE_STONE: case PLATE_WOOD: case PLATE_IRON: return w.getBlockMetadata(x, y, z) > 0;
    default: return false;
    }
}

bool isIndirectlyPoweringTo(World& w, int x, int y, int z, int l) {
    switch (w.getBlockId(x, y, z)) {
    case WIRE: return wiresProvidePower && isPoweringTo(w, x, y, z, l);
    case TORCH_ACTIVE: return l == 0 && isPoweringTo(w, x, y, z, l);
    case LEVER: {
        int i1 = w.getBlockMetadata(x, y, z);
        if ((i1 & 8) == 0) return false;
        int j1 = i1 & 7;
        return (j1 == 6 && l == 1) || (j1 == 5 && l == 1) || (j1 == 4 && l == 2) || (j1 == 3 && l == 3) ||
               (j1 == 2 && l == 4) || (j1 == 1 && l == 5);
    }
    case BUTTON: {
        int i1 = w.getBlockMetadata(x, y, z);
        if ((i1 & 8) == 0) return false;
        int j1 = i1 & 7;
        return (j1 == 5 && l == 1) || (j1 == 4 && l == 2) || (j1 == 3 && l == 3) || (j1 == 2 && l == 4) || (j1 == 1 && l == 5);
    }
    case PLATE_STONE: case PLATE_WOOD: case PLATE_IRON: return w.getBlockMetadata(x, y, z) != 0 && l == 1;
    default: return false;
    }
}

bool isBlockProvidingPowerTo(World& w, int x, int y, int z, int side) { // func_668_j
    return w.getBlockId(x, y, z) != 0 && isIndirectlyPoweringTo(w, x, y, z, side);
}
bool isBlockGettingPowered(World& w, int x, int y, int z) { // func_646_n
    return isBlockProvidingPowerTo(w, x, y - 1, z, 0) || isBlockProvidingPowerTo(w, x, y + 1, z, 1) ||
           isBlockProvidingPowerTo(w, x, y, z - 1, 2) || isBlockProvidingPowerTo(w, x, y, z + 1, 3) ||
           isBlockProvidingPowerTo(w, x - 1, y, z, 4) || isBlockProvidingPowerTo(w, x + 1, y, z, 5);
}
bool isBlockIndirectlyProvidingPowerTo(World& w, int x, int y, int z, int side) {
    int id = w.getBlockId(x, y, z);
    if (id == MOTOR) return true;
    if (normalCube(w, x, y, z)) return isBlockGettingPowered(w, x, y, z);
    return id != 0 && isPoweringTo(w, x, y, z, side);
}
bool isBlockIndirectlyGettingPowered(World& w, int x, int y, int z) {
    return isBlockIndirectlyProvidingPowerTo(w, x, y - 1, z, 0) || isBlockIndirectlyProvidingPowerTo(w, x, y + 1, z, 1) ||
           isBlockIndirectlyProvidingPowerTo(w, x, y, z - 1, 2) || isBlockIndirectlyProvidingPowerTo(w, x, y, z + 1, 3) ||
           isBlockIndirectlyProvidingPowerTo(w, x - 1, y, z, 4) || isBlockIndirectlyProvidingPowerTo(w, x + 1, y, z, 5);
}

// ================================================================ block events

bool handles(int id) {
    switch (id) {
    case WIRE: case TORCH_IDLE: case TORCH_ACTIVE: case LEVER: case BUTTON: case PLATE_STONE: case PLATE_WOOD:
    case PLATE_IRON: case DOOR_WOOD: case DOOR_IRON: case TNT:
        return true;
    default: return false;
    }
}

void onBlockAdded(World& w, int i, int j, int k, int id) {
    switch (id) {
    case WIRE:
        wireUpdate(w, i, j, k);
        w.notifyBlocksOfNeighborChange(i, j + 1, k, WIRE);
        w.notifyBlocksOfNeighborChange(i, j - 1, k, WIRE);
        wireAround(w, i, j, k);
        break;
    case TORCH_IDLE: case TORCH_ACTIVE:
        if (w.getBlockMetadata(i, j, k) == 0) Blocks::torchAdded(w, i, j, k, id);
        if (id == TORCH_ACTIVE) notifyAll6(w, i, j, k, id);
        break;
    case BUTTON:
        if (normalCube(w, i - 1, j, k)) w.setBlockMetadataWithNotify(i, j, k, 1);
        else if (normalCube(w, i + 1, j, k)) w.setBlockMetadataWithNotify(i, j, k, 2);
        else if (normalCube(w, i, j, k - 1)) w.setBlockMetadataWithNotify(i, j, k, 3);
        else if (normalCube(w, i, j, k + 1)) w.setBlockMetadataWithNotify(i, j, k, 4);
        if (!buttonCanPlace(w, i, j, k)) drop(w, i, j, k, id); // func_305_h
        break;
    default: break; // the lever, plates, doors and TNT do nothing when added
    }
}

void onBlockRemoval(World& w, int i, int j, int k, int id, int meta) {
    switch (id) {
    case WIRE:
        w.notifyBlocksOfNeighborChange(i, j + 1, k, WIRE);
        w.notifyBlocksOfNeighborChange(i, j - 1, k, WIRE);
        wireUpdate(w, i, j, k);
        wireAround(w, i, j, k);
        break;
    case TORCH_ACTIVE: notifyAll6(w, i, j, k, id); break;
    case LEVER: case BUTTON:
        if ((meta & 8) > 0) notifyAttached(w, i, j, k, id, meta & 7);
        break;
    case PLATE_STONE: case PLATE_WOOD: case PLATE_IRON:
        if (meta > 0) {
            w.notifyBlocksOfNeighborChange(i, j, k, id);
            w.notifyBlocksOfNeighborChange(i, j - 1, k, id);
        }
        break;
    default: break;
    }
}

void onNeighborBlockChange(World& w, int i, int j, int k, int id, int l) {
    switch (id) {
    case WIRE:
        if (!normalCube(w, i, j - 1, k)) drop(w, i, j, k, id); // canPlace
        else wireUpdate(w, i, j, k);
        break;
    case TORCH_IDLE: case TORCH_ACTIVE:
        Blocks::torchNeighbourChanged(w, i, j, k, id);
        w.scheduleBlockUpdate(i, j, k, id);
        break;
    case LEVER: {
        if (!leverCanPlaceAt(w, i, j, k)) { // checkIfAttachedToBlock
            drop(w, i, j, k, id);
            break;
        }
        int i1 = w.getBlockMetadata(i, j, k) & 7;
        bool flag = (!normalCube(w, i - 1, j, k) && i1 == 1) || (!normalCube(w, i + 1, j, k) && i1 == 2) ||
                    (!normalCube(w, i, j, k - 1) && i1 == 3) || (!normalCube(w, i, j, k + 1) && i1 == 4) ||
                    (!normalCube(w, i, j - 1, k) && i1 == 5) || (!normalCube(w, i, j - 1, k) && i1 == 6);
        if (flag) drop(w, i, j, k, id);
        break;
    }
    case BUTTON: {
        if (!buttonCanPlace(w, i, j, k)) { // func_305_h
            drop(w, i, j, k, id);
            break;
        }
        int i1 = w.getBlockMetadata(i, j, k) & 7;
        bool flag = (!normalCube(w, i - 1, j, k) && i1 == 1) || (!normalCube(w, i + 1, j, k) && i1 == 2) ||
                    (!normalCube(w, i, j, k - 1) && i1 == 3) || (!normalCube(w, i, j, k + 1) && i1 == 4);
        if (flag) drop(w, i, j, k, id);
        break;
    }
    case PLATE_STONE: case PLATE_WOOD: case PLATE_IRON: // on a solid block or a fence
        if (!normalCube(w, i, j - 1, k) && w.getBlockId(i, j - 1, k) != 85) drop(w, i, j, k, id);
        break;
    case DOOR_WOOD: case DOOR_IRON: {
        int i1 = w.getBlockMetadata(i, j, k);
        if (i1 & 8) {
            if (w.getBlockId(i, j - 1, k) != id) w.setBlockWithNotify(i, j, k, 0);
            if (l > 0 && canProvidePower(l)) onNeighborBlockChange(w, i, j - 1, k, id, l);
        } else {
            bool flag = false;
            if (w.getBlockId(i, j + 1, k) != id) {
                w.setBlockWithNotify(i, j, k, 0);
                flag = true;
            }
            if (!normalCube(w, i, j - 1, k)) {
                w.setBlockWithNotify(i, j, k, 0);
                flag = true;
                if (w.getBlockId(i, j + 1, k) == id) w.setBlockWithNotify(i, j + 1, k, 0);
            }
            if (flag) {
                Blocks::dropBlockAsItem(w, i, j, k, id, i1);
            } else if (l > 0 && canProvidePower(l)) {
                bool flag1 = isBlockIndirectlyGettingPowered(w, i, j, k) || isBlockIndirectlyGettingPowered(w, i, j + 1, k);
                doorPower(w, i, j, k, id, flag1);
            }
        }
        break;
    }
    case TNT:
        if (l > 0 && canProvidePower(l) && isBlockIndirectlyGettingPowered(w, i, j, k)) {
            if (Blocks::onTntIgnited) Blocks::onTntIgnited(i, j, k); // onBlockDestroyedByPlayer: primed, with its fuse sound
            w.setBlockWithNotify(i, j, k, 0);
        }
        break;
    default: break;
    }
}

void updateTick(World& w, int i, int j, int k, int id, JavaRandom& random) {
    switch (id) {
    case TORCH_IDLE: case TORCH_ACTIVE: {
        bool flag = torchPowered(w, i, j, k);
        while (!torchUpdates.empty() && w.info.time - torchUpdates.front().time > 100) torchUpdates.pop_front();
        if (id == TORCH_ACTIVE) {
            if (flag) {
                w.setBlockAndMetadataWithNotify(i, j, k, TORCH_IDLE, w.getBlockMetadata(i, j, k));
                if (checkForBurnout(w, i, j, k, true)) {
                    sound(w, "random.fizz", i + 0.5, j + 0.5, k + 0.5, 0.5f, 2.6f + (w.rand.nextFloat() - w.rand.nextFloat()) * 0.8f);
                    for (int l = 0; l < 5; ++l) {
                        double d = i + random.nextDouble() * 0.6 + 0.2;
                        double d1 = j + random.nextDouble() * 0.6 + 0.2;
                        double d2 = k + random.nextDouble() * 0.6 + 0.2;
                        particle(w, 1, d, d1, d2); // "smoke"
                    }
                }
            }
        } else if (!flag && !checkForBurnout(w, i, j, k, false)) {
            w.setBlockAndMetadataWithNotify(i, j, k, TORCH_ACTIVE, w.getBlockMetadata(i, j, k));
        }
        break;
    }
    case BUTTON: { // pops back out
        int l = w.getBlockMetadata(i, j, k);
        if ((l & 8) != 0) {
            w.setBlockMetadataWithNotify(i, j, k, l & 7);
            notifyAttached(w, i, j, k, id, l & 7);
            sound(w, "random.click", i + 0.5, j + 0.5, k + 0.5, 0.3f, 0.5f);
            w.markBlocksDirty(i, j, k, i, j, k);
        }
        break;
    }
    case PLATE_STONE: case PLATE_WOOD: case PLATE_IRON:
        if (w.getBlockMetadata(i, j, k) != 0) plateCheck(w, i, j, k);
        break;
    default: break;
    }
}

bool blockActivated(World& w, int i, int j, int k) {
    int id = w.getBlockId(i, j, k);
    switch (id) {
    case LEVER: {
        int l = w.getBlockMetadata(i, j, k), i1 = l & 7, j1 = 8 - (l & 8);
        w.setBlockMetadataWithNotify(i, j, k, i1 + j1);
        w.markBlocksDirty(i, j, k, i, j, k);
        sound(w, "random.click", i + 0.5, j + 0.5, k + 0.5, 0.3f, j1 <= 0 ? 0.5f : 0.6f);
        notifyAttached(w, i, j, k, id, i1);
        return true;
    }
    case BUTTON: {
        int l = w.getBlockMetadata(i, j, k), i1 = l & 7, j1 = 8 - (l & 8);
        if (j1 == 0) return true;
        w.setBlockMetadataWithNotify(i, j, k, i1 + j1);
        w.markBlocksDirty(i, j, k, i, j, k);
        sound(w, "random.click", i + 0.5, j + 0.5, k + 0.5, 0.3f, 0.6f);
        notifyAttached(w, i, j, k, id, i1);
        w.scheduleBlockUpdate(i, j, k, id);
        return true;
    }
    case DOOR_WOOD: case DOOR_IRON: return doorActivated(w, i, j, k, id);
    default: return false;
    }
}

void entityTouching(World& w, const AABB& box) {
    // Entity.moveEntity: every block the box (shrunk by 0.001) is in
    int x0 = int(std::floor(box.minX + 0.001)), y0 = int(std::floor(box.minY + 0.001)), z0 = int(std::floor(box.minZ + 0.001));
    int x1 = int(std::floor(box.maxX - 0.001)), y1 = int(std::floor(box.maxY - 0.001)), z1 = int(std::floor(box.maxZ - 0.001));
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z) {
                int id = w.getBlockId(x, y, z);
                if ((id == PLATE_STONE || id == PLATE_WOOD || id == PLATE_IRON) && w.getBlockMetadata(x, y, z) != 1)
                    plateCheck(w, x, y, z);
            }
}

} // namespace Redstone
