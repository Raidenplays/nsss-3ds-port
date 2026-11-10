import net.minecraft.src.*;
import java.io.*;
public class GenWorld {
  public static void main(String[] a) throws Exception {
    long seed = Long.parseLong(a[2]);
    int r = Integer.parseInt(a[3]);
    long t0 = System.currentTimeMillis();
    World w = new World(new File(a[0]), a[1], seed);
    int sx = w.spawnX >> 4, sz = w.spawnZ >> 4;
    System.out.println("spawn " + w.spawnX + "," + w.spawnY + "," + w.spawnZ + " seed " + w.randomSeed);
    for (int cx = sx - r; cx <= sx + r; cx++)
      for (int cz = sz - r; cz <= sz + r; cz++)
        w.getChunkFromChunkCoords(cx, cz);
    System.out.println("generated " + (2*r+1)*(2*r+1) + " chunks in " + (System.currentTimeMillis()-t0) + " ms");
    w.saveWorld(true, null);
    System.out.println("saved");
  }
}
