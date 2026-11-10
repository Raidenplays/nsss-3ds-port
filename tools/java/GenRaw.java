import net.minecraft.src.*;
import java.io.*;
// usage: GenRaw <tmpdir> <seed> <cx0> <cz0> <r> [dumpcx dumpcz out]
public class GenRaw {
  static long fnv(byte[] b) { long h = 0xcbf29ce484222325L; for (byte x : b) { h ^= (x & 0xff); h *= 0x100000001b3L; } return h; }
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    long seed = Long.parseLong(a[1]);
    World w = new World(new File(a[0]), "W" + seed, seed);
    ChunkProviderGenerate g = new ChunkProviderGenerate(w, seed);
    int cx0 = Integer.parseInt(a[2]), cz0 = Integer.parseInt(a[3]), r = Integer.parseInt(a[4]);
    for (int cx = cx0 - r; cx <= cx0 + r; cx++) for (int cz = cz0 - r; cz <= cz0 + r; cz++) {
      Chunk c = g.provideChunk(cx, cz);
      System.out.printf("%d %d %016x%n", cx, cz, fnv(c.blocks));
      if (a.length > 7 && cx == Integer.parseInt(a[5]) && cz == Integer.parseInt(a[6])) {
        FileOutputStream o = new FileOutputStream(a[7]); o.write(c.blocks); o.close();
      }
    }
  }
}
