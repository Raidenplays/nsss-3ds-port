import net.minecraft.src.*;
// Block.getExplosionResistance(null) for every block (-> source/world/BlockResistance.h)
public class DumpResistance { public static void main(String[] a) throws Exception {
  Class.forName("net.minecraft.src.Block");
  StringBuilder sb = new StringBuilder();
  for (int i = 0; i < 256; i++) { Block b = Block.blocksList[i]; float r = b == null ? 0 : b.getExplosionResistance(null); sb.append(r + "f,"); if (i % 8 == 7) sb.append("\n"); }
  System.out.print(sb);
}}
