import net.minecraft.src.*;
import java.io.*;
public class CheckRT {
  public static void main(String[] a) throws Exception {
    World w = new World(new File(a[0]), a[1], 0L);
    int bx = -8*16+5, bz = -1*16+5; StringBuilder s = new StringBuilder();
    for (int y = 100; y < 110; y++) s.append(w.getBlockId(bx, y, bz)).append(' ');
    System.out.println("pillar: " + s);
    System.out.println("wool id " + w.getBlockId(-8*16+6, 100, -16+6) + " meta " + w.getBlockMetadata(-8*16+6, 100, -16+6));
    System.out.println("new chunk bedrock " + w.getBlockId(40*16+3, 2, 40*16+3) + " above " + w.getBlockId(40*16+3, 5, 40*16+3));
    NBTTagCompound root = CompressedStreamTools.func_1138_a(new FileInputStream(new File(new File(a[0], a[1]), "level.dat")));
    NBTTagCompound p = root.getCompoundTag("Data").getCompoundTag("Player");
    NBTTagList pos = p.getTagList("Pos");
    System.out.println("player " + ((NBTTagDouble)pos.tagAt(0)).doubleValue + " " + ((NBTTagDouble)pos.tagAt(1)).doubleValue + " " + ((NBTTagDouble)pos.tagAt(2)).doubleValue + " time " + w.worldTime + " seed " + w.randomSeed);
  }
}
