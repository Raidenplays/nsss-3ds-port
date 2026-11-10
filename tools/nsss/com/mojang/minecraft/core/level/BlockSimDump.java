package com.mojang.minecraft.core.level;
import com.mojang.minecraft.core.enums.*;
import com.mojang.minecraft.core.level.chunk.*;
import java.io.*;
import java.util.*;
// java BlockSimDump <seed> <tmpdir> <script> <out>
// Script lines: "load cx cz R" (ring-load chunks), "set x y z id meta" (setBlockAndMetadataWithNotify),
// "tick n" (n x: worldTime++, tickUpdates(false)), "dump x0 y0 z0 x1 y1 z1" (id+meta, x then z then y).
// Same script runs on tools/host/blocksim.cpp; the outputs must be identical.
public class BlockSimDump {
  public static void main(String[] a) throws Exception {
    long seed = Long.parseLong(a[0]);
    World w = new World(new File(a[1]), "sim", seed, EnumSeason.summer, EnumWeather.clear, false, 0, null);
    w.chunkProvider = new ChunkProviderLoadOrGenerate(w, null, new ChunkProviderGenerate(w, seed));
    w.rand = new Random(777L);
    w.worldTime = 0;
    try (DataOutputStream o = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(a[3])));
         BufferedReader r = new BufferedReader(new FileReader(a[2]))) {
      String line;
      while ((line = r.readLine()) != null) {
        String[] p = line.trim().split("\\s+");
        if (p.length == 0 || p[0].isEmpty() || p[0].startsWith("#")) continue;
        int[] v = new int[p.length - 1];
        for (int i = 1; i < p.length; i++) v[i - 1] = Integer.parseInt(p[i]);
        switch (p[0]) {
          case "load":
            for (int ring = 0; ring <= v[2]; ring++)
              for (int dx = -ring; dx <= ring; dx++)
                for (int dz = -ring; dz <= ring; dz++)
                  if (Math.max(Math.abs(dx), Math.abs(dz)) == ring) w.chunkProvider.provideChunk(v[0] + dx, v[1] + dz);
            break;
          case "set": w.setBlockAndMetadataWithNotify(v[0], v[1], v[2], v[3], v[4]); break;
          case "tick": for (int i = 0; i < v[0]; i++) { w.worldTime++; w.tickUpdates(false); } break;
          case "rtick": // n rounds: random-tick every tickOnLoad block in the box, x then z then y
            for (int n = 0; n < v[6]; n++)
              for (int x = v[0]; x <= v[3]; x++) for (int z = v[2]; z <= v[5]; z++) for (int y = v[1]; y <= v[4]; y++) {
                int id = w.getBlockId(x, y, z);
                if (id > 0 && com.mojang.minecraft.core.level.tile.Block.tickOnLoad[id])
                  com.mojang.minecraft.core.level.tile.Block.allBlocks[id].updateTick(w, x, y, z, w.rand);
              }
            break;
          case "dump":
            for (int x = v[0]; x <= v[3]; x++) for (int z = v[2]; z <= v[5]; z++) for (int y = v[1]; y <= v[4]; y++) {
              o.writeByte(w.getBlockId(x, y, z)); o.writeByte(w.getBlockMetadata(x, y, z));
            }
            break;
        }
      }
    }
    System.out.println("ok");
  }
}
