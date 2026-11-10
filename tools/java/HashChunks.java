import net.minecraft.src.*;
import java.io.*;
public class HashChunks {
  static long fnv(byte[] b) { long h = 0xcbf29ce484222325L; for (byte x : b) { h ^= (x & 0xff); h *= 0x100000001b3L; } return h; }
  public static void main(String[] a) throws Exception {
    World w = new World(new File(a[0]), a[1], 0L);
    int r = Integer.parseInt(a[2]); int cx0 = Integer.parseInt(a[3]), cz0 = Integer.parseInt(a[4]);
    for (int cx = cx0 - r; cx <= cx0 + r; cx++) for (int cz = cz0 - r; cz <= cz0 + r; cz++) {
      Chunk c = w.getChunkFromChunkCoords(cx, cz);
      System.out.printf("%d %d %016x %016x %016x %016x%n", cx, cz, fnv(c.blocks), fnv(c.data.data), fnv(c.skylightMap.data), fnv(c.blocklightMap.data));
    }
    System.out.println("seed " + w.randomSeed + " spawn " + w.spawnX + " " + w.spawnY + " " + w.spawnZ + " time " + w.worldTime);
  }
}
