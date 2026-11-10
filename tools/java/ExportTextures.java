import net.minecraft.src.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;
import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
// Runs every vanilla PNG (and Axeolon's own sheets) through Axeolon's texture pipeline,
// exactly as RenderEngine does at load time, and writes the final images out.
public class ExportTextures {
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block"); Class.forName("net.minecraft.src.Item"); // same init order as the game
    File out = new File(a[1]);
    List<String> names = new ArrayList<>();
    try (ZipFile z = new ZipFile(a[0])) {
      for (Enumeration<? extends ZipEntry> e = z.entries(); e.hasMoreElements();) {
        String n = e.nextElement().getName();
        if (n.endsWith(".png")) names.add("/" + n);
      }
    }
    names.add(AxeolonTerrain2.PATH);
    int ok = 0, fail = 0;
    for (String n : names) {
      try {
        InputStream in = ExportTextures.class.getResourceAsStream(n);
        if (in == null) { System.out.println("missing " + n); fail++; continue; }
        BufferedImage img = ImageIO.read(in);
        BufferedImage res = AxeolonTextures.applyOverlay(n, img);
        File f = new File(out, n.substring(1));
        f.getParentFile().mkdirs();
        ImageIO.write(res, "png", f);
        ok++;
      } catch (Throwable t) { System.out.println("fail " + n + ": " + t); Throwable c = t; while (c.getCause() != null) c = c.getCause(); c.printStackTrace(System.out); fail++; }
    }
    System.out.println(ok + " exported, " + fail + " failed");
  }
}
