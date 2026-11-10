import net.minecraft.src.*;
import java.io.*;
public class BoatTest {
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    World w = new World(new File(a[0]), a[1], 0L, null);
    int Y = 120;
    for (int x = -4; x < 30; x++) for (int z = -4; z < 30; z++) { w.setBlockWithNotify(x, Y - 2, z, 1); w.setBlockWithNotify(x, Y - 1, z, x >= 0 && x < 20 && z >= 0 && z < 20 ? 9 : 1); for (int y = Y; y < Y + 6; y++) w.setBlockWithNotify(x, y, z, 0); }
    w.setBlockWithNotify(15, Y, 5, 1); w.setBlockWithNotify(15, Y - 1, 5, 1);
    EntityBoat b = new EntityBoat(w, 5.5, Y + 0.5, 5.5);
    w.entityJoinedWorld(b);
    b.motionX = Double.parseDouble(a[2]); b.motionZ = Double.parseDouble(a[3]);
    for (int t = 0; t < Integer.parseInt(a[4]); t++) {
      b.onUpdate();
      System.out.printf("%d %.6f %.6f %.6f %.6f %.6f %.6f %.3f %b%n", t, b.posX, b.posY, b.posZ, b.motionX, b.motionY, b.motionZ, b.rotationYaw, b.isDead);
    }
  }
}
