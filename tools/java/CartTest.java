import net.minecraft.src.*;
import java.io.*;
public class CartTest {
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    World w = new World(new File(a[0]), a[1], 0L, null);
    int Y = 120;
    for (int x = -4; x < 30; x++) for (int z = -4; z < 30; z++) { w.setBlockWithNotify(x, Y - 1, z, 1); for (int y = Y; y < Y + 6; y++) w.setBlockWithNotify(x, y, z, 0); }
    // a line east, up a slope, a curve south, and on
    for (int x = 0; x < 8; x++) w.setBlockWithNotify(x, Y, 0, 66);
    w.setBlockWithNotify(8, Y, 0, 1); w.setBlockWithNotify(8, Y + 1, 0, 66);
    w.setBlockWithNotify(9, Y, 0, 1); w.setBlockWithNotify(9, Y + 1, 0, 66);
    w.setBlockWithNotify(10, Y, 0, 1); w.setBlockWithNotify(10, Y + 1, 0, 66);
    for (int z = 1; z < 10; z++) { w.setBlockWithNotify(10, Y, z, 1); w.setBlockWithNotify(10, Y + 1, z, 66); }
    StringBuilder sb = new StringBuilder();
    for (int x = 0; x <= 10; x++) sb.append(w.getBlockMetadata(x, x >= 8 ? Y + 1 : Y, 0)).append(' ');
    System.out.println("rails " + sb);
    EntityMinecart c = new EntityMinecart(w, 0.5, Y + 0.5, 0.5, 0);
    w.entityJoinedWorld(c);
    c.motionX = Double.parseDouble(a[2]);
    for (int t = 0; t < Integer.parseInt(a[3]); t++) {
      c.onUpdate();
      System.out.printf("%d %.6f %.6f %.6f %.6f %.6f %.6f %.3f%n", t, c.posX, c.posY, c.posZ, c.motionX, c.motionY, c.motionZ, c.rotationYaw);
    }
  }
}
