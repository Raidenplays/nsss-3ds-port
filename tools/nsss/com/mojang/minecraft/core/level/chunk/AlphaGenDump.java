package com.mojang.minecraft.core.level.chunk;
import com.mojang.minecraft.core.level.World;
import java.io.*;
import java.lang.reflect.*;
// java AlphaGenDump <seed> <x0> <z0> <x1> <z1> <out>: raw 32768-byte chunks from NSSS's own generator
public class AlphaGenDump {
  public static void main(String[] a) throws Exception {
    Field uf = sun.misc.Unsafe.class.getDeclaredField("theUnsafe"); uf.setAccessible(true);
    sun.misc.Unsafe u = (sun.misc.Unsafe) uf.get(null);
    World w = (World) u.allocateInstance(World.class);
    long seed = Long.parseLong(a[0]);
    w.randomSeed = seed;
    ChunkProviderGenerate g = new ChunkProviderGenerate(w, seed);
    int x0 = Integer.parseInt(a[1]), z0 = Integer.parseInt(a[2]), x1 = Integer.parseInt(a[3]), z1 = Integer.parseInt(a[4]);
    try (OutputStream out = new BufferedOutputStream(new FileOutputStream(a[5]))) {
      for (int cx = x0; cx <= x1; cx++) for (int cz = z0; cz <= z1; cz++) {
        g.genRandom.setSeed(cx * 341873128712L + cz * 132897987541L);
        byte[] b = new byte[32768];
        g.generateTerrain(cx, cz, b);
        g.replaceBlocksForBiome(cx, cz, b);
        g.mapGenBase.func_867_a(g, w, cx, cz, b);
        out.write(b);
      }
    }
    System.out.println("ok");
  }
}
