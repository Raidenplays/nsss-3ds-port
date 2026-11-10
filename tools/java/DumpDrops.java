import net.minecraft.src.*;
import java.lang.reflect.*;
import java.util.*;
// D id meta dropId damage qty   (R = depends on the random: ported by hand)
public class DumpDrops { public static void main(String[] a) throws Exception {
  Class.forName("net.minecraft.src.Block");
  Method dd = Block.class.getDeclaredMethod("damageDropped", int.class); dd.setAccessible(true);
  StringBuilder b = new StringBuilder();
  for (int i = 1; i < 256; i++) { Block k = Block.blocksList[i]; if (k == null) continue;
    for (int m = 0; m < 16; m++) {
      Set<Integer> ids = new HashSet<>(), qs = new HashSet<>();
      for (int s = 0; s < 40; s++) { ids.add(k.idDropped(m, new Random(s * 7919L))); qs.add(k.quantityDropped(new Random(s * 104729L))); }
      int dmg = (Integer)dd.invoke(k, m);
      b.append("D ").append(i).append(' ').append(m).append(' ').append(ids.size() == 1 ? ids.iterator().next().toString() : "R").append(' ').append(dmg).append(' ').append(qs.size() == 1 ? qs.iterator().next().toString() : "R").append('\n');
    } }
  System.out.print(b);
}}
