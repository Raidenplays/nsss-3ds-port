package com.mojang.minecraft.core.level;
import com.mojang.minecraft.core.enums.*;
import com.mojang.minecraft.core.level.chunk.*;
import java.io.*;
// java AlphaPopDump <seed> <tmpdir> cx0 cz0 loadRadius reportRadius out
// Loads chunks ring by ring (like tools/host/alphapop.cpp) through NSSS's own provider, so they
// generate and populate in the same order, then writes id+meta of every block in the report square.
public class AlphaPopDump {
  public static void main(String[] a) throws Exception {
    long seed = Long.parseLong(a[0]);
    World w = new World(new File(a[1]), "pop", seed, EnumSeason.summer, EnumWeather.clear, false, 0, null);
    // a fresh provider: forget the chunks the spawn search made
    w.chunkProvider = new ChunkProviderLoadOrGenerate(w, null, new ChunkProviderGenerate(w, seed));
    int cx0 = Integer.parseInt(a[2]), cz0 = Integer.parseInt(a[3]), R = Integer.parseInt(a[4]), R2 = Integer.parseInt(a[5]);
    for (int ring = 0; ring <= R; ring++)
      for (int dx = -ring; dx <= ring; dx++)
        for (int dz = -ring; dz <= ring; dz++) {
          if (Math.max(Math.abs(dx), Math.abs(dz)) != ring) continue;
          w.chunkProvider.provideChunk(cx0 + dx, cz0 + dz);
        }
    try (DataOutputStream o = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(a[6])))) {
      for (int cx = cx0 - R2; cx <= cx0 + R2; cx++) for (int cz = cz0 - R2; cz <= cz0 + R2; cz++) {
        Chunk c = w.chunkProvider.provideChunk(cx, cz);
        for (int x = 0; x < 16; x++) for (int z = 0; z < 16; z++) for (int y = 0; y < 128; y++) {
          o.writeByte(c.getBlockId(x, y, z));
          o.writeByte(c.getBlockMetadata(x, y, z));
        }
      }
    }
    System.out.println("ok");
  }
}
