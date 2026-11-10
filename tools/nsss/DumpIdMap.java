import java.lang.reflect.*;
import java.util.*;
// prints NSSS's IdMap (the names /give accepts): "name id" per line, sorted
public class DumpIdMap {
  public static void main(String[] a) throws Exception {
    Class.forName("com.mojang.minecraft.core.level.tile.Block");
    Class.forName("com.mojang.minecraft.core.entity.item.Item");
    Class<?> c = Class.forName("com.mojang.minecraft.core.util.IdMap");
    Field f = c.getDeclaredField("map"); f.setAccessible(true);
    TreeMap<String, Integer> m = new TreeMap<>((Map<String, Integer>) f.get(null));
    for (Map.Entry<String, Integer> e : m.entrySet()) System.out.println(e.getKey() + " " + e.getValue());
  }
}
