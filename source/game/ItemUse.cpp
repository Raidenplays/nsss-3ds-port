#include "ItemUse.h"
#include "Mobs.h"
#include <cmath>
#include "Inventory.h"
#include "Player.h"
#include "Survival.h"
#include "audio/Sound.h"
#include "world/BlockProps.h"
#include "world/BlockTable.h"
#include "world/Blocks.h"
#include "world/Redstone.h"
#include "world/MathHelper.h"
#include "world/World.h"
#include "world/gen/ChunkGenerator.h"

namespace itemuse {

bool signEdit = false;
int signX = 0, signY = 0, signZ = 0;

namespace {

inline int fl(double v) { return int(std::floor(v)); }

void stepSound(int id, int x, int y, int z) { // the sound of placing a block
    sound::play(BS[id].step, x + 0.5, y + 0.5, z + 0.5, (BS[id].volume + 1.0f) / 2.0f, BS[id].pitch * 0.8f);
}

void consume(Inventory& inv, bool creative) {
    if (!creative) inv.consumeHeld();
}

// ItemStack.damageItem on the held item
void wear(Inventory& inv, int n, bool creative, const Player& p, World& w) {
    if (creative) return;
    ItemStack& h = inv.held();
    if (h.empty() || survival::maxDamage(h.id) <= 0) return;
    h.damage = int16_t(h.damage + n);
    if (h.damage > survival::maxDamage(h.id)) {
        h.clear();
        sound::play("random.break", p.posX, p.posY - 1.62, p.posZ, 0.8f, 0.8f + w.rand.nextFloat() * 0.4f);
    }
}

void offset(int side, int& x, int& y, int& z) {
    if (side == 0) --y;
    if (side == 1) ++y;
    if (side == 2) --z;
    if (side == 3) ++z;
    if (side == 4) --x;
    if (side == 5) ++x;
}

int yawQuarter(const Player& p) { return fl(double(p.rotationYaw * 4.0f / 360.0f) + 0.5) & 3; }

// World.canBlockBePlacedAt: the spot is empty (or water, lava, fire, a snow layer), the block
// accepts it, and nobody is standing in it
bool canPlaceAt(World& w, const Player& p, int id, int x, int y, int z) {
    if (y < 0 || y >= 255 || !w.isChunkLoaded(x >> 4, z >> 4)) return false;
    int cur = w.getBlockId(x, y, z);
    if (!(cur == 0 || (cur >= 8 && cur <= 11) || cur == 51 || cur == 78)) return false;
    if (!Blocks::canPlaceBlockAt(w, id, x, y, z)) return false;
    bool solid = (BLOCKS[id].flags & BF_EXISTS) && matSolid(BP[id].mat) && BP[id].cls != BC::Torch &&
                 BP[id].cls != BC::RedstoneTorch && BP[id].cls != BC::LapisTorch && BP[id].cls != BC::Ladder &&
                 BP[id].cls != BC::Button && BP[id].cls != BC::Lever && BP[id].cls != BC::PressurePlate;
    if (solid && AABB{double(x), double(y), double(z), x + 1.0, y + 1.0, z + 1.0}.intersects(p.boundingBox)) return false;
    return true;
}

// Block.onBlockPlaced (the side clicked) and onBlockPlacedBy (the way the player faces)
void placed(World& w, const Player& p, int id, int x, int y, int z, int side, bool upper) {
    auto opaque = [&](int a, int b, int c) { return BP[w.getBlockId(a, b, c)].opaqueCube; };
    int meta = w.getBlockMetadata(x, y, z);
    switch (BP[id].cls) {
    case BC::Torch: case BC::RedstoneTorch: case BC::LapisTorch: {
        if (side == 1 && opaque(x, y - 1, z)) meta = 5;
        if (side == 2 && opaque(x, y, z + 1)) meta = 4;
        if (side == 3 && opaque(x, y, z - 1)) meta = 3;
        if (side == 4 && opaque(x + 1, y, z)) meta = 2;
        if (side == 5 && opaque(x - 1, y, z)) meta = 1;
        w.setBlockMetadataWithNotify(x, y, z, meta);
        break;
    }
    case BC::Ladder:
        if ((meta == 0 || side == 2) && opaque(x, y, z + 1)) meta = 2;
        if ((meta == 0 || side == 3) && opaque(x, y, z - 1)) meta = 3;
        if ((meta == 0 || side == 4) && opaque(x + 1, y, z)) meta = 4;
        if ((meta == 0 || side == 5) && opaque(x - 1, y, z)) meta = 5;
        w.setBlockMetadataWithNotify(x, y, z, meta);
        break;
    case BC::Button: case BC::Lever: {
        int hi = meta & 8;
        meta &= 7;
        if (BP[id].cls == BC::Lever && side == 1 && opaque(x, y - 1, z)) meta = 5 + w.rand.nextInt(2);
        if (side == 2 && opaque(x, y, z + 1)) meta = 4;
        if (side == 3 && opaque(x, y, z - 1)) meta = 3;
        if (side == 4 && opaque(x + 1, y, z)) meta = 2;
        if (side == 5 && opaque(x - 1, y, z)) meta = 1;
        w.setBlockMetadataWithNotify(x, y, z, meta + hi);
        break;
    }
    case BC::Icicle: {
        bool rising = side == 1 ? true : side == 0 ? false : !(w.getBlockId(x, y + 1, z) != 0 && opaque(x, y + 1, z));
        w.setBlockMetadataWithNotify(x, y, z, rising ? 1 : 0);
        break;
    }
    case BC::Hopper:
        w.setBlockMetadataWithNotify(x, y, z, side == 2 ? 3 : side == 3 ? 2 : side == 4 ? 5 : side == 5 ? 4 : 0);
        break;
    case BC::Dispenser: case BC::Furnace: {
        static const int F[4] = {2, 5, 3, 4};
        w.setBlockMetadataWithNotify(x, y, z, F[yawQuarter(p)]);
        break;
    }
    case BC::Pumpkin:
        w.setBlockMetadataWithNotify(x, y, z, yawQuarter(p));
        break;
    case BC::PistonBase:
        Redstone::pistonPlaced(w, x, y, z, p.posX, p.posY, p.posZ, p.rotationYaw);
        break;
    case BC::PowerRepeater: // onBlockPlacedBy, then check()
        w.setBlockMetadataWithNotify(x, y, z, yawQuarter(p));
        Blocks::onNeighborBlockChange(w, x, y, z, id, 0);
        break;
    case BC::Stairs: {
        int d = yawQuarter(p);
        int m = d == 0 ? 2 : d == 1 ? 1 : d == 2 ? 3 : 0;
        if (upper) m |= 4; // Axeolon: upside-down stairs
        w.setBlockMetadataWithNotify(x, y, z, m);
        break;
    }
    default: break;
    }
}

// ItemBlock.onItemUse
bool placeBlock(World& w, Player& p, Inventory& inv, int id, int meta, int x, int y, int z, int side, float hitY, bool creative) {
    bool upper = side == 0 ? true : side == 1 ? false : hitY > 0.5f; // computeUpperHalf
    if (w.getBlockId(x, y, z) == 78) side = 0; // a snow layer is replaced
    else offset(side, x, y, z);
    if (!canPlaceAt(w, p, id, x, y, z)) return true;
    if (w.setBlockAndMetadataWithNotify(x, y, z, id, meta)) {
        placed(w, p, id, x, y, z, side, upper);
        stepSound(id, x, y, z);
        consume(inv, creative);
    }
    return true;
}

} // namespace

int sideOf(const RayHit& h) {
    if (h.ny < 0) return 0;
    if (h.ny > 0) return 1;
    if (h.nz < 0) return 2;
    if (h.nz > 0) return 3;
    if (h.nx < 0) return 4;
    return 5;
}

bool useOnBlock(World& w, Player& p, Inventory& inv, const RayHit& hit, float hitY, bool creative) {
    ItemStack& h = inv.held();
    if (h.empty()) return false;
    int id = h.id, x = hit.x, y = hit.y, z = hit.z, side = sideOf(hit);
    int target = w.getBlockId(x, y, z);

    if (id < 256) {
        if (!(BLOCKS[id].flags & BF_EXISTS)) return false;
        return placeBlock(w, p, inv, id, h.damage & 15, x, y, z, side, hitY, creative);
    }
    switch (id) {
    case 290: case 291: case 292: case 293: case 294: { // ItemHoe
        Mat above = BP[w.getBlockId(x, y + 1, z)].mat;
        if ((matSolid(above) || target != 2) && target != 3) return false;
        stepSound(60, x, y, z);
        w.setBlockWithNotify(x, y, z, 60);
        wear(inv, 1, creative, p, w);
        if (w.rand.nextInt(8) == 0 && target == 2) {
            // a seed pops out of the grass (World.drops -> an EntityItem)
            float s = 0.7f;
            w.drops.push_back(World::Drop{x + double(w.rand.nextFloat() * s + (1.0f - s) * 0.5f), y + 1.2,
                                          z + double(w.rand.nextFloat() * s + (1.0f - s) * 0.5f), 295, 1, 0});
        }
        return true;
    }
    case 295: // ItemSeeds: wheat on top of farmland
        if (side != 1) return false;
        if (target == 60 && w.getBlockId(x, y + 1, z) == 0) {
            w.setBlockWithNotify(x, y + 1, z, 59);
            consume(inv, creative);
            return true;
        }
        return false;
    case 338: // ItemReed: sugar cane
        return placeBlock(w, p, inv, 83, 0, x, y, z, side, hitY, creative);
    case 331: // ItemRedstone
        return placeBlock(w, p, inv, 55, 0, x, y, z, side, hitY, creative);
    case 354: // ItemReed: a cake
        return placeBlock(w, p, inv, 92, 0, x, y, z, side, hitY, creative);
    case 324: case 330: { // ItemDoor
        if (side != 1) return false;
        ++y;
        int door = id == 330 ? 71 : (h.damage <= 0 || h.damage >= 10) ? 64 : 144 + h.damage;
        if (y >= 254 || !BP[w.getBlockId(x, y - 1, z)].opaqueCube || w.getBlockId(x, y, z) != 0 || w.getBlockId(x, y + 1, z) != 0)
            return false;
        int f = fl(double((p.rotationYaw + 180.0f) * 4.0f / 360.0f) - 0.5) & 3;
        int dx = f == 1 ? -1 : f == 3 ? 1 : 0, dz = f == 0 ? 1 : f == 2 ? -1 : 0;
        auto op = [&](int a, int b, int c) { return BP[w.getBlockId(a, b, c)].opaqueCube ? 1 : 0; };
        int left = op(x - dx, y, z - dz) + op(x - dx, y + 1, z - dz);
        int right = op(x + dx, y, z + dz) + op(x + dx, y + 1, z + dz);
        bool doorLeft = w.getBlockId(x - dx, y, z - dz) == door || w.getBlockId(x - dx, y + 1, z - dz) == door;
        bool doorRight = w.getBlockId(x + dx, y, z + dz) == door || w.getBlockId(x + dx, y + 1, z + dz) == door;
        if ((doorLeft && !doorRight) || right > left) f = ((f - 1) & 3) + 4;
        w.setBlockWithNotify(x, y, z, door);
        w.setBlockMetadataWithNotify(x, y, z, f);
        w.setBlockWithNotify(x, y + 1, z, door);
        w.setBlockMetadataWithNotify(x, y + 1, z, f + 8);
        stepSound(door, x, y, z);
        consume(inv, creative);
        return true;
    }
    case 323: { // ItemSign: a post on top, on the wall otherwise; then the keyboard for the words
        if (side == 0 || !matSolid(BP[target].mat)) return false;
        offset(side, x, y, z);
        if (y < 0 || y >= 255 || w.getBlockId(x, y, z) != 0) return false;
        if (side == 1)
            w.setBlockAndMetadataWithNotify(x, y, z, 63, fl(double((p.rotationYaw + 180.0f) * 16.0f / 360.0f) + 0.5) & 15);
        else
            w.setBlockAndMetadataWithNotify(x, y, z, 68, side);
        if (nbt::Tag* te = w.tileEntityAt(x, y, z)) te->setInt("Wood", h.damage >= 0 && h.damage < 10 ? h.damage : 0);
        stepSound(63, x, y, z);
        consume(inv, creative);
        signEdit = true;
        signX = x;
        signY = y;
        signZ = z;
        return true;
    }
    case 259: { // ItemFlintAndSteel
        offset(side, x, y, z);
        if (w.getBlockId(x, y, z) == 0) {
            sound::play("fire.ignite", x + 0.5, y + 0.5, z + 0.5, 1.0f, w.rand.nextFloat() * 0.4f + 0.8f);
            w.setBlockWithNotify(x, y, z, 51);
        }
        wear(inv, 1, creative, p, w);
        return true;
    }
    case 363: { // NSSS ItemDye (bonemeal): a sapling grows now, crops ripen, grass sprouts flowers
        static JavaRandom itemRand(int64_t(0x5DEECE66DLL) ^ 8682522807148012LL); // Item.itemRand (new Random())
        if (target == 6) { // BlockSapling.growTree
            int l = w.getBlockMetadata(x, y, z) & 3;
            w.setBlock(x, y, z, 0);
            if (!nsssSaplingTree(w, w.rand, x, y, z)) w.setBlockAndMetadata(x, y, z, 6, l);
            consume(inv, creative);
            return true;
        }
        if (target == 59) { // BlockCrops.fertilize
            w.setBlockMetadataWithNotify(x, y, z, 7);
            consume(inv, creative);
            return true;
        }
        if (target != 2) return false;
        consume(inv, creative);
        for (int j1 = 0; j1 < 128; ++j1) {
            int k1 = x, l1 = y + 1, i2 = z;
            bool ok = true;
            for (int j2 = 0; j2 < j1 / 16; ++j2) {
                k1 += itemRand.nextInt(3) - 1;
                int a = itemRand.nextInt(3) - 1;
                l1 += a * itemRand.nextInt(3) / 2;
                i2 += itemRand.nextInt(3) - 1;
                if (w.getBlockId(k1, l1 - 1, i2) != 2 || w.isBlockOpaqueCube(k1, l1, i2)) { ok = false; break; }
            }
            if (!ok || w.getBlockId(k1, l1, i2) != 0) continue;
            if (itemRand.nextInt(7) > 1) w.setBlockWithNotify(k1, l1, i2, 37);
            else if (itemRand.nextInt(3) == 1) w.setBlockWithNotify(k1, l1, i2, 88);
            else w.setBlockWithNotify(k1, l1, i2, 38);
        }
        return true;
    }
    default: return false;
    }
}

bool useItem(World& w, Player& p, Inventory& inv, bool creative, double ex, double ey, double ez, double lx, double ly,
             double lz) {
    ItemStack& h = inv.held();
    if (h.empty()) return false;
    // ItemFood / ItemSoup
    int heal = 0;
    switch (h.id) {
    case 260: heal = 4; break;   // apple
    case 282: heal = 10; break;  // mushroom stew (the bowl stays)
    case 297: heal = 5; break;   // bread
    case 319: heal = 3; break;   // raw porkchop
    case 320: heal = 8; break;   // cooked porkchop
    case 322: heal = 42; break;  // golden apple
    case 349: heal = 2; break;   // raw fish
    case 350: heal = 5; break;   // cooked fish
    default: break;
    }
    if (heal > 0) {
        p.heal(heal);
        if (!creative) {
            if (h.id == 282) {
                h.id = 281;
                h.count = 1;
                h.damage = 0;
            } else {
                inv.consumeHeld();
            }
        }
        return true;
    }
    // ItemBucket: 325 empty, 326 water, 327 lava
    if (h.id == 325 || h.id == 326 || h.id == 327) {
        bool empty = h.id == 325;
        // rayTraceBlocks_do: through the air (and liquid unless we're filling) up to 5 blocks
        int x = fl(ex), y = fl(ey), z = fl(ez);
        double len = std::sqrt(lx * lx + ly * ly + lz * lz);
        if (len < 1e-6) return false;
        double dx = lx / len, dy = ly / len, dz = lz / len;
        int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
        auto tmax = [](double o, double d, int c) {
            if (std::fabs(d) < 1e-12) return 1e30;
            double edge = d > 0 ? c + 1.0 : double(c);
            return (edge - o) / d;
        };
        double tx = tmax(ex, dx, x), ty = tmax(ey, dy, y), tz = tmax(ez, dz, z);
        double ddx = std::fabs(dx) < 1e-12 ? 1e30 : 1.0 / std::fabs(dx), ddy = std::fabs(dy) < 1e-12 ? 1e30 : 1.0 / std::fabs(dy),
               ddz = std::fabs(dz) < 1e-12 ? 1e30 : 1.0 / std::fabs(dz);
        int side = -1;
        for (int i = 0; i < 64; ++i) {
            if (y >= 0 && y < 256) {
                int id = w.getBlockId(x, y, z);
                bool liquid = id >= 8 && id <= 11;
                bool hit = false;
                if (liquid) hit = empty && w.getBlockMetadata(x, y, z) == 0;
                else hit = id != 0 && id != 51;
                if (hit && side >= 0) {
                    if (empty) {
                        if (!liquid) return false;
                        w.setBlockWithNotify(x, y, z, 0);
                        if (!creative) {
                            h.id = id <= 9 ? 326 : 327;
                            h.count = 1;
                            h.damage = 0;
                        }
                        return true;
                    }
                    offset(side, x, y, z);
                    if (w.getBlockId(x, y, z) == 0 || !matSolid(BP[w.getBlockId(x, y, z)].mat)) {
                        if (w.info.dimension == -1 && h.id == 326) {
                            sound::play("random.fizz", ex + 0.5, ey + 0.5, ez + 0.5, 0.5f, 2.6f + (w.rand.nextFloat() - w.rand.nextFloat()) * 0.8f);
                        } else {
                            w.setBlockAndMetadataWithNotify(x, y, z, h.id == 326 ? 9 : 11, 0);
                        }
                        if (!creative) h.id = 325;
                        return true;
                    }
                    return false;
                }
            }
            double t;
            if (tx < ty && tx < tz) { t = tx; x += sx; tx += ddx; side = sx > 0 ? 4 : 5; }
            else if (ty < tz) { t = ty; y += sy; ty += ddy; side = sy > 0 ? 0 : 1; }
            else { t = tz; z += sz; tz += ddz; side = sz > 0 ? 2 : 3; }
            if (t > 5.0) break;
        }
        return false;
    }
    return false;
}

} // namespace itemuse

namespace itemuse {
bool throwHeld(World& w, Player& p, Inventory& inv, bool creative, Mobs& mobs) {
    ItemStack& h = inv.held();
    if (h.empty()) return false;
    if (h.id == 261) { // ItemBow.onItemRightClick: InventoryPlayer.consumeInventoryItem(arrow)
        if (!creative) {
            int slot = -1;
            for (int i = 0; i < Inventory::MAIN && slot < 0; ++i)
                if (inv.main[i].id == 262 && !inv.main[i].empty()) slot = i;
            if (slot < 0) return false;
            if (--inv.main[slot].count <= 0) inv.main[slot].clear();
        }
        sound::play("random.bow", p.posX, p.posY, p.posZ, 1.0f, 1.0f / (w.rand.nextFloat() * 0.4f + 0.8f));
        mobs.throwFrom(p, Mobs::T_ARROW);
        wear(inv, 1, creative, p, w);
        return true;
    }
    if (h.id == 332 || h.id == 344) { // ItemSnowball / ItemEgg
        bool egg = h.id == 344;
        if (!creative) inv.consumeHeld();
        sound::play("random.bow", p.posX, p.posY, p.posZ, 0.5f, 0.4f / (w.rand.nextFloat() * 0.4f + 0.8f));
        mobs.throwFrom(p, egg ? Mobs::T_EGG : Mobs::T_SNOWBALL);
        return true;
    }
    return false;
}
} // namespace itemuse
