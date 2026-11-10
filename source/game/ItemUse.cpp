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

// World.func_695_a (canBlockBePlacedAt): nobody standing where a solid block would go; then a liquid,
// fire or a snow layer is simply replaced, anything else needs an empty spot the block accepts
// (Block.canPlace) - except ice beside hot burnt cloth, which always asks
bool canPlaceAt(World& w, const Player& p, int id, int x, int y, int z) {
    // getCollisionBoundingBoxFromPool: the blocks with one, taken as the whole cube
    bool collides = matSolid(BP[id].mat) && BP[id].cls != BC::PressurePlate && BP[id].cls != BC::Sign;
    if (collides && AABB{double(x), double(y), double(z), x + 1.0, y + 1.0, z + 1.0}.intersects(p.boundingBox)) return false;
    int cur = w.getBlockId(x, y, z);
    bool replaceable = (cur >= 8 && cur <= 11) || cur == 51 || cur == 78;
    if (!replaceable || (id == 79 && Blocks::hotClothNearby(w, x, y, z)))
        return id > 0 && cur == 0 && Blocks::canPlace(w, id, x, y, z);
    return true;
}

// Block.onBlockPlaced (the side clicked) then onBlockPlacedBy (the way the player faces)
void placed(World& w, Player& p, int id, int x, int y, int z, int side) {
    auto cube = [&](int a, int b, int c) { return w.isBlockOpaqueCube(a, b, c); }; // isBlockNormalCube
    auto slab = [&](int a, int b, int c, int m) { return w.getBlockId(a, b, c) == 44 && w.getBlockMetadata(a, b, c) == m; };
    int meta = w.getBlockMetadata(x, y, z);
    switch (BP[id].cls) {
    case BC::Torch: case BC::RedstoneTorch: // BlockTorch (the slab tests ignore the side clicked, as in the jar)
        if (side == 1 && (cube(x, y - 1, z) || w.getBlockId(x, y - 1, z) == 85 || slab(x, y - 1, z, 1))) meta = 5;
        if ((side == 2 && cube(x, y, z + 1)) || slab(x, y, z + 1, 3)) meta = 4;
        if ((side == 3 && cube(x, y, z - 1)) || slab(x, y, z - 1, 2)) meta = 3;
        if ((side == 4 && cube(x + 1, y, z)) || slab(x + 1, y, z, 5)) meta = 2;
        if ((side == 5 && cube(x - 1, y, z)) || slab(x - 1, y, z, 4)) meta = 1;
        w.setBlockMetadataWithNotify(x, y, z, meta);
        break;
    case BC::Ladder:
        if ((meta == 0 || side == 2) && cube(x, y, z + 1)) meta = 2;
        if ((meta == 0 || side == 3) && cube(x, y, z - 1)) meta = 3;
        if ((meta == 0 || side == 4) && cube(x + 1, y, z)) meta = 4;
        if ((meta == 0 || side == 5) && cube(x - 1, y, z)) meta = 5;
        w.setBlockMetadataWithNotify(x, y, z, meta);
        break;
    case BC::Button: { // a side only; the pressed bit stays
        int hi = meta & 8;
        meta &= 7;
        if (side == 2 && cube(x, y, z + 1)) meta = 4;
        if (side == 3 && cube(x, y, z - 1)) meta = 3;
        if (side == 4 && cube(x + 1, y, z)) meta = 2;
        if (side == 5 && cube(x - 1, y, z)) meta = 1;
        w.setBlockMetadataWithNotify(x, y, z, meta + hi);
        break;
    }
    case BC::Lever: { // a floor lever turns one of two ways at random; no solid block behind the side: it drops
        int hi = meta & 8, m = -1;
        if (side == 1 && cube(x, y - 1, z)) m = 5 + w.rand.nextInt(2);
        if (side == 2 && cube(x, y, z + 1)) m = 4;
        if (side == 3 && cube(x, y, z - 1)) m = 3;
        if (side == 4 && cube(x + 1, y, z)) m = 2;
        if (side == 5 && cube(x - 1, y, z)) m = 1;
        if (m == -1) {
            Blocks::dropBlockAsItem(w, x, y, z, id, w.getBlockMetadata(x, y, z));
            w.setBlockWithNotify(x, y, z, 0);
        } else {
            w.setBlockMetadataWithNotify(x, y, z, m + hi);
        }
        break;
    }
    case BC::Step: // the side clicked; sneaking: the bottom half, running: the top
        meta = side == 1 ? 0 : side == 0 ? 1 : side;
        if (p.isSneaking()) meta = 0;
        if (p.isRunning()) meta = 1;
        w.setBlockMetadataWithNotify(x, y, z, meta);
        break;
    case BC::Furnace: {
        static const int F[4] = {2, 5, 3, 4};
        w.setBlockMetadataWithNotify(x, y, z, F[yawQuarter(p)]);
        break;
    }
    case BC::Stairs: {
        static const int S[4] = {2, 1, 3, 0};
        w.setBlockMetadataWithNotify(x, y, z, S[yawQuarter(p)]);
        break;
    }
    default: break;
    }
}

// ItemBlock.onItemUse: a slab on a slab facing it makes a double slab; otherwise the block goes in the
// spot beside the side clicked (a snow layer is replaced). The item's damage is never the metadata.
bool placeBlock(World& w, Player& p, Inventory& inv, int id, int x, int y, int z, int side, bool creative) {
    bool alreadyPlaced = false;
    if (id == 44 && w.getBlockId(x, y, z) == 44 && !p.isSneaking()) {
        int o = w.getBlockMetadata(x, y, z);
        if ((side == 0 && o == 1) || (side == 1 && o == 0) || (side > 1 && o > 0 && side == o)) {
            if (w.setBlockWithNotify(x, y, z, 43)) {
                placed(w, p, 43, x, y, z, side);
                stepSound(43, x, y, z);
                consume(inv, creative);
                alreadyPlaced = true;
            }
        }
    }
    if (w.getBlockId(x, y, z) == 78) side = 0;
    else offset(side, x, y, z);
    if (inv.held().empty()) return false;
    if (canPlaceAt(w, p, id, x, y, z) && !alreadyPlaced && w.setBlockWithNotify(x, y, z, id)) {
        placed(w, p, id, x, y, z, side);
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
        return placeBlock(w, p, inv, id, x, y, z, side, creative);
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
        return placeBlock(w, p, inv, 83, x, y, z, side, creative);
    case 331: // ItemRedstone
        return placeBlock(w, p, inv, 55, x, y, z, side, creative);
    case 354: // ItemReed: a cake
        return false; // (no cake in NSSS)
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
