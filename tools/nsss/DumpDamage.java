import com.mojang.minecraft.core.entity.item.*;
public class DumpDamage {
  public static void main(String[] a) throws Exception {
    Class.forName("com.mojang.minecraft.core.level.tile.Block");
    for (int i = 256; i < Item.itemsList.length; i++) {
      Item it = Item.itemsList[i];
      if (it == null) continue;
      int d = it.getDamageVsEntity(null);
      int hit = it instanceof ItemSword ? 1 : it instanceof ItemTool ? 2 : 0;
      int mine = it instanceof ItemSword ? 2 : it instanceof ItemTool ? 1 : 0;
      if (d != 1 || hit != 0 || mine != 0) System.out.println(i + " " + d + " " + hit + " " + mine + " " + it.getClass().getSimpleName());
    }
  }
}
