import net.minecraft.src.*;
import java.io.*;
public class CheckNew {
  public static void main(String[] a) throws Exception {
    World w = new World(new File(a[0]), a[1], 0L);
    McRegionChunkLoader l = new McRegionChunkLoader(new File(a[0], a[1]));
    Chunk c = l.loadChunk(w, 40, 40);
    System.out.println("direct load: " + (c == null ? "null" : ("id " + (c.blocks[3<<12|3<<8|2] & 255) + " len " + c.blocks.length)));
    Chunk c2 = w.getChunkFromChunkCoords(40, 40);
    System.out.println("via world: id " + (c2.blocks[3<<12|3<<8|2] & 255) + " populated " + c2.isTerrainPopulated);
  }
}
