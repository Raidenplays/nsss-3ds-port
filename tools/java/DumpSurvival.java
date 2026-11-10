import net.minecraft.src.*;
import java.lang.reflect.*;
import java.util.*;
// tools/generated/survival.txt: hardness, item stack sizes, tool speeds, recipes, smelting, fuel, recipe locks
public class DumpSurvival {
  static Object get(Object o, String f) throws Exception { Field x = o.getClass().getDeclaredField(f); x.setAccessible(true); return x.get(o); }
  static String st(ItemStack s) { return s == null ? "-" : s.itemID + ":" + s.getItemDamage() + ":" + s.stackSize; }
  public static void main(String[] a) throws Exception {
    Class.forName("net.minecraft.src.Block");
    StringBuilder b = new StringBuilder();
    for (int i = 0; i < 256; i++) { Block k = Block.blocksList[i]; if (k == null) continue;
      Field h = Block.class.getDeclaredField("blockHardness"); h.setAccessible(true);
      b.append("B ").append(i).append(' ').append(h.getFloat(k)).append('\n'); }
    TileEntityFurnace fur = new TileEntityFurnace();
    Method burn = TileEntityFurnace.class.getDeclaredMethod("getItemBurnTime", ItemStack.class); burn.setAccessible(true);
    for (int i = 0; i < Item.itemsList.length; i++) { Item it = Item.itemsList[i]; if (it == null) continue;
      ItemStack s = new ItemStack(i, 1, 0);
      int bt = (Integer)burn.invoke(fur, s);
      b.append("I ").append(i).append(' ').append(it.getItemStackLimit()).append(' ').append(it.getMaxDamage()).append(' ').append(bt).append('\n');
      StringBuilder t = new StringBuilder(); boolean any = false;
      for (int k = 0; k < 256; k++) { Block bl = Block.blocksList[k]; if (bl == null) continue;
        float f = s.getStrVsBlock(bl); boolean hv = s.canHarvestBlock(bl);
        if (f != 1.0F || hv) { any = true; t.append(' ').append(k).append('=').append(f).append(hv ? "h" : ""); } }
      if (any) b.append("T ").append(i).append(t).append('\n'); }
    List r = CraftingManager.getInstance().getRecipeList();
    for (Object o : r) {
      if (o instanceof ShapedRecipes) { ShapedRecipes sr = (ShapedRecipes)o; ItemStack[] in = sr.getInputs();
        b.append("S ").append(sr.getWidth()).append(' ').append(sr.getHeight()).append(' ').append(st(sr.getOutput()));
        for (ItemStack x : in) b.append(' ').append(x == null ? "-" : x.itemID + ":" + x.getItemDamage()); b.append('\n'); }
      else if (o instanceof ShapelessRecipes) { ShapelessRecipes sr = (ShapelessRecipes)o;
        b.append("L ").append(st(sr.getOutput()));
        for (Object x : sr.getInputs()) { ItemStack y = (ItemStack)x; b.append(' ').append(y.itemID + ":" + y.getItemDamage()); } b.append('\n'); }
      else b.append("? ").append(o.getClass().getName()).append('\n'); }
    Map m = FurnaceRecipes.smelting().getSmeltingList();
    for (Object k : m.keySet()) { Object v = m.get(k); b.append("F ").append(k).append(' ').append(v instanceof ItemStack ? st((ItemStack)v) : v.toString()).append('\n'); }
    Field lf = AxeolonRecipeLocks.class.getDeclaredField("locks"); lf.setAccessible(true);
    Map locks = (Map)lf.get(null);
    for (Object k : locks.keySet()) b.append("K ").append(k).append(' ').append(locks.get(k)).append('\n');
    System.out.print(b);
  }
}
