import com.mojang.minecraft.core.level.tile.*;
import com.mojang.minecraft.core.level.tile.material.*;
import java.io.*;
import java.lang.reflect.*;
public class DumpNsssBlocks {
  static Object f(Object o, String n) { for (Class<?> c = o.getClass(); c != null; c = c.getSuperclass()) { try { Field x = c.getDeclaredField(n); x.setAccessible(true); return x.get(o); } catch (Exception e) {} } return null; }
  public static void main(String[] a) throws Exception {
    Class.forName("com.mojang.minecraft.core.level.tile.Block");
    PrintWriter w = new PrintWriter(new FileWriter(a[0]));
    for (int id = 1; id < 256; id++) {
      Block b = Block.allBlocks[id];
      if (b == null) continue;
      StringBuilder tex = new StringBuilder();
      for (int m = 0; m < 16; m++) for (int s = 0; s < 6; s++) { int t; try { t = b.getBlockTextureFromSideAndMetadata(s, m); } catch (Throwable e) { t = -1; } tex.append(t).append(m == 15 && s == 5 ? "" : ","); }
      String mat = "?";
      for (Field mf : Material.class.getFields()) if (Modifier.isStatic(mf.getModifiers()) && mf.get(null) == f(b, "blockMaterial")) mat = mf.getName();
      Object ss = f(b, "stepSound");
      String snd = ss == null ? "" : f(ss, "field_1678_a") + "";
      w.println(id + "\t" + b.getClass().getSimpleName() + "\t" + mat + "\t" + Block.lightOpacity[id] + "\t" + Block.lightValue[id]
        + "\t" + b.isOpaqueCube() + "\t" + b.renderAsNormalBlock() + "\t" + b.getRenderType() + "\t" + b.getRenderBlockPass()
        + "\t" + f(b, "blockHardness") + "\t" + f(b, "blockResistance") + "\t" + b.tickRate() + "\t" + Block.tickOnLoad[id] + "\t" + Block.isBlockContainer[id]
        + "\t" + f(b,"minX")+","+f(b,"minY")+","+f(b,"minZ")+","+f(b,"maxX")+","+f(b,"maxY")+","+f(b,"maxZ") + "\t" + tex);
    }
    w.close();
    System.out.println("ok");
  }
}
