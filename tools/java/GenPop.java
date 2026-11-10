import net.minecraft.src.*;
import java.io.*;
// usage: GenPop <saves> <world> cx0 cz0 loadRadius reportRadius [dumpcx dumpcz out]
public class GenPop {
  static long fnv(byte[] b, long h) { for (byte x : b) { h ^= (x & 0xff); h *= 0x100000001b3L; } return h; }
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    ((BlockLeaves)Block.leaves).setGraphicsLevel(true);
    World w = new World(new File(a[0]), a[1]);
    w.rand.setSeed(777L);
    int cx0 = Integer.parseInt(a[2]), cz0 = Integer.parseInt(a[3]), R = Integer.parseInt(a[4]), R2 = Integer.parseInt(a[5]);
    java.lang.reflect.Field f = World.class.getDeclaredField("chunkProvider"); f.setAccessible(true);
    ((ChunkProviderLoadOrGenerate)f.get(w)).func_21110_c(cx0, cz0);
    for (int ring = 0; ring <= R; ring++) for (int dx = -ring; dx <= ring; dx++) for (int dz = -ring; dz <= ring; dz++) {
      if (Math.max(Math.abs(dx), Math.abs(dz)) != ring) continue;
      w.getChunkFromChunkCoords(cx0 + dx, cz0 + dz);
    }
    for (int cx = cx0 - R2; cx <= cx0 + R2; cx++) for (int cz = cz0 - R2; cz <= cz0 + R2; cz++) {
      Chunk c = w.getChunkFromChunkCoords(cx, cz);
      System.out.printf("%d %d %016x %016x %d%n", cx, cz, fnv(c.blocks, 0xcbf29ce484222325L), fnv(c.data.data, 0xcbf29ce484222325L), c.isTerrainPopulated ? 1 : 0);
    }
    if (a.length > 6) { Chunk c = w.getChunkFromChunkCoords(Integer.parseInt(a[6]), Integer.parseInt(a[7]));
      FileOutputStream o = new FileOutputStream(a[8]); o.write(c.blocks); o.write(c.data.data); o.close(); }
  }
}
