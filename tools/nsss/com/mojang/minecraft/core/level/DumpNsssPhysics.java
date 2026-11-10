package com.mojang.minecraft.core.level;
import com.mojang.minecraft.core.level.tile.Block;
import com.mojang.minecraft.core.level.tile.material.Material;
import com.mojang.minecraft.core.level.tile.phys.AxisAlignedBB;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;

// physics.txt for the NSSS jar, read by tools/gen_physics.py:
//   B id material solid liquid water lava ice slipperiness opaque
//   C id meta n  box*n (relative to the block)  - an isolated block in air
// The World is never constructed: only the block lookups below are answered.
public class DumpNsssPhysics extends World {
  int bx, by, bz, bid, bmeta;
  DumpNsssPhysics() { super((String) null, null); }
  @Override public int getBlockId(int x, int y, int z) { return x == bx && y == by && z == bz ? bid : 0; }
  @Override public int getBlockMetadata(int x, int y, int z) { return x == bx && y == by && z == bz ? bmeta : 0; }
  @Override public Material getMaterialXYZ(int x, int y, int z) { int i = getBlockId(x, y, z); return i == 0 ? Material.air : Block.allBlocks[i].blockMaterial; }
  @Override public boolean isBlockNormalCube(int x, int y, int z) { int i = getBlockId(x, y, z); return i != 0 && Block.allBlocks[i].renderAsNormalBlock() && Block.allBlocks[i].blockMaterial.isSolidMaterial(); }

  public static void main(String[] a) throws Exception {
    Class.forName("com.mojang.minecraft.core.level.tile.Block");
    Field uf = sun.misc.Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
    DumpNsssPhysics w = (DumpNsssPhysics) ((sun.misc.Unsafe) uf.get(null)).allocateInstance(DumpNsssPhysics.class);
    w.bx = 8; w.by = 64; w.bz = 8;
    IdentityHashMap<Material, Integer> mats = new IdentityHashMap<>();
    AxisAlignedBB all = AxisAlignedBB.getBoundingBox(w.bx - 4, w.by - 4, w.bz - 4, w.bx + 5, w.by + 5, w.bz + 5);
    StringBuilder s = new StringBuilder();
    for (int id = 1; id < 256; id++) {
      Block b = Block.allBlocks[id];
      if (b == null) continue;
      Material m = b.blockMaterial;
      if (!mats.containsKey(m)) mats.put(m, mats.size());
      s.append("B " + id + " " + mats.get(m) + " " + (m.isSolidMaterial() ? 1 : 0) + " " + (m.getIsGroundCover() ? 1 : 0) + " "
        + (m == Material.water ? 1 : 0) + " " + (m == Material.lava ? 1 : 0) + " " + (m == Material.ice ? 1 : 0) + " "
        + b.slipperiness + " " + (b.isOpaqueCube() ? 1 : 0) + "\n");
      for (int meta = 0; meta < 16; meta++) {
        w.bid = id; w.bmeta = meta;
        ArrayList<AxisAlignedBB> l = new ArrayList<>();
        try { b.getCollidingBoundingBoxes(w, w.bx, w.by, w.bz, all, l); } catch (Throwable t) { l.clear(); System.err.println("block " + id + ": " + t); }
        s.append("C " + id + " " + meta + " " + l.size());
        for (AxisAlignedBB bb : l)
          s.append(" " + (bb.minX - w.bx) + " " + (bb.minY - w.by) + " " + (bb.minZ - w.bz) + " " + (bb.maxX - w.bx) + " " + (bb.maxY - w.by) + " " + (bb.maxZ - w.bz));
        s.append("\n");
      }
    }
    try (Writer o = new FileWriter(a[0])) { o.write(s.toString()); }
    System.out.println("ok");
  }
}
