import net.minecraft.src.*;
// Item.getDamageVsEntity for every item that hits harder than a fist (-> source/game/EntityDamage.cpp)
public class DumpDamage { public static void main(String[] a) throws Exception {
  Class.forName("net.minecraft.src.Block");
  Class.forName("net.minecraft.src.Item");
  for (int i = 0; i < Item.itemsList.length; i++) { Item it = Item.itemsList[i]; if (it == null) continue;
    int d; try { d = it.getDamageVsEntity(null); } catch (Throwable t) { continue; }
    if (d != 1) System.out.println(i + "\t" + d + "\t" + it.getClass().getSimpleName()); }
}}
