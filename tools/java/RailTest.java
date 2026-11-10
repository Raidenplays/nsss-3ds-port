import net.minecraft.src.*;
import java.io.*;
public class RailTest {
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    World w = new World(new File(a[0]), a[1], 0L, null);
    int Y = 120;
    for (int x = -2; x < 14; x++) for (int z = -2; z < 14; z++) { w.setBlockWithNotify(x, Y - 1, z, 1); for (int y = Y; y < Y + 4; y++) w.setBlockWithNotify(x, y, z, 0); }
    int[][] seq = {{0,0,0},{1,0,0},{2,0,0},{2,0,1},{2,0,2},{1,0,2},{0,0,2},{0,0,1},{5,0,5},{5,0,6},{6,0,5},{4,0,5},{5,0,4},{8,0,8},{9,1,8},{10,1,8},{8,0,9}};
    for (int[] s : seq) {
      if (s[1] == 1) w.setBlockWithNotify(s[0], Y, s[2], 1);
      w.setBlockWithNotify(s[0], Y + s[1], s[2], 66);
    }
    StringBuilder sb = new StringBuilder();
    for (int[] s : seq) sb.append(w.getBlockMetadata(s[0], Y + s[1], s[2])).append(' ');
    System.out.println(sb);
  }
}
