import net.minecraft.src.*;
import java.io.*;
public class DumpBlocks {
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    PrintWriter w = new PrintWriter(new FileWriter(a[0]));
    w.println("{");
    boolean first = true;
    for (int id = 1; id < 256; id++) {
      Block b = Block.blocksList[id];
      if (b == null) continue;
      if (!first) w.println(",");
      first = false;
      StringBuilder tex = new StringBuilder("[");
      for (int m = 0; m < 16; m++) {
        if (m > 0) tex.append(",");
        tex.append("[");
        for (int s = 0; s < 6; s++) {
          int t; try { t = b.getBlockTextureFromSideAndMetadata(s, m); } catch (Throwable e) { t = -1; }
          if (s > 0) tex.append(",");
          tex.append(t);
        }
        tex.append("]");
      }
      tex.append("]");
      String name; try { name = b.getBlockName(); } catch (Throwable e) { name = "?"; }
      String disp; try { disp = StringTranslate.getInstance().translateNamedKey(name); } catch (Throwable e) { disp = name; }
      disp = disp.replace("\\", "").replace("\"", "'");
      int rt; try { rt = b.getRenderType(); } catch (Throwable e) { rt = -1; }
      int pass; try { pass = b.getRenderBlockPass(); } catch (Throwable e) { pass = 0; }
      w.print("\"" + id + "\":{\"name\":\"" + name + "\",\"disp\":\"" + disp + "\",\"cls\":\"" + b.getClass().getSimpleName() + "\",\"render\":" + rt
        + ",\"opaque\":" + b.isOpaqueCube() + ",\"normal\":" + b.renderAsNormalBlock() + ",\"pass\":" + pass
        + ",\"opacity\":" + Block.lightOpacity[id] + ",\"light\":" + Block.lightValue[id]
        + ",\"liquid\":" + b.blockMaterial.getIsLiquid()
        + ",\"bounds\":[" + b.minX + "," + b.minY + "," + b.minZ + "," + b.maxX + "," + b.maxY + "," + b.maxZ + "]"
        + ",\"tex\":" + tex + "}");
    }
    w.println("\n}");
    w.close();
    System.out.println("ok");
  }
}
