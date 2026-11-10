import com.mojang.minecraft.core.level.tile.*;
import com.mojang.minecraft.core.level.tile.material.*;
import com.mojang.minecraft.core.entity.item.*;
import com.mojang.minecraft.core.entity.tile.*;
import com.mojang.minecraft.core.crafting.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;

// Writes the engine's table inputs from the NSSS a1.1.12 jar, in the same formats as tools/java/*:
//   blocks.json (DumpBlocks)  blockprops.tsv (DumpBlockProps)  items.tsv (DumpItems)
//   survival.txt (DumpSurvival)  drops.txt (DumpDrops)
// usage: java -cp out:a1.1.12.jar DumpNsss <out dir>
public class DumpNsss {
  static Object f(Object o, String n) {
    for (Class<?> c = o.getClass(); c != null; c = c.getSuperclass()) {
      try { Field x = c.getDeclaredField(n); x.setAccessible(true); return x.get(o); } catch (Exception e) {}
    }
    return null;
  }
  static final Map<String, String> NAMES = new HashMap<>();
  static {
    String[] kv = {
      "stone","Stone","grass","Grass","dirt","Dirt","cobblestone","Cobblestone","planks","Wooden Planks","sapling","Sapling",
      "bedrock","Bedrock","waterMoving","Water","waterStill","Water","lavaMoving","Lava","lavaStill","Lava","sand","Sand",
      "gravel","Gravel","oreGold","Gold Ore","oreIron","Iron Ore","oreCoal","Coal Ore","wood","Wood","leaves","Leaves",
      "sponge","Sponge","glass","Glass","cloth","Cloth","plantYellow","Flower","plantRed","Rose","mushroomBrown","Mushroom",
      "mushroomRed","Mushroom","blockGold","Block of Gold","blockSteel","Block of Iron","stepDouble","Double Slab",
      "stepSingle","Slab","brick","Bricks","tnt","TNT","bookShelf","Bookshelf","cobblestoneMossy","Moss Stone",
      "obsidian","Obsidian","torchWood","Torch","fire","Fire","mobSpawner","Mob Spawner","stairSingle","Wooden Stairs",
      "crate","Chest","redWire","Redstone Dust","oreDiamond","Diamond Ore","blockDiamond","Block of Diamond",
      "workbench","Workbench","crops","Crops","tilledField","Farmland","stoneOvenIdle","Furnace","stoneOvenActive","Furnace",
      "signPost","Sign","doorWood","Wooden Door","ladder","Ladder","minecartTrack","Rail","stairCompactStone","Stone Stairs",
      "pressurePlateWoodIdle","Sign","lever","Lever","pressurePlateStone","Stone Pressure Plate","doorSteel","Iron Door",
      "pressurePlateWood","Wooden Pressure Plate","oreRed","Redstone Ore","oreRedGlowing","Redstone Ore",
      "torchRedIdle","Redstone Torch","torchRedActive","Redstone Torch","button","Button","snow","Snow","ice","Ice",
      "blockSnow","Snow Block","cactus","Cactus","blockClay","Clay","reed","Reeds","jukebox","Jukebox","fence","Fence",
      "gear","Gear","plantBlue","Blue Rose","spongeInactive","Wet Sponge","oreCopper","Copper Ore","motor","Motor",
      "generator","Generator","blockCopper","Block of Copper","brickMossy","Mossy Bricks","bleedingObsidian","Bleeding Obsidian",
      "web","Cobweb","pressurePlateSteel","Iron Pressure Plate","lightBulb","Light Bulb","lightBulbOn","Light Bulb",
      "pyramidion","Pyramidion","stoneSand","Sandstone","brickBlue","Blue Bricks","brickFlint","Flint Bricks","blockClayRed","Red Clay",
      "shovel","Iron Shovel","pickaxeSteel","Iron Pickaxe","axeSteel","Iron Axe","striker","Flint and Steel","appleRed","Apple",
      "ingotIron","Iron Ingot","ingotGold","Gold Ingot","swordSteel","Iron Sword","bowlEmpty","Bowl","bowlSoup","Mushroom Stew",
      "silk","String","gunpowder","Gunpowder","porkRaw","Raw Porkchop","porkCooked","Cooked Porkchop","appleGold","Golden Apple",
      "bucketEmpty","Bucket","bucketWater","Water Bucket","bucketLava","Lava Bucket","minecartEmpty","Minecart",
      "minecartBox","Storage Minecart","minecartEngine","Powered Minecart","bucketMilk","Milk","slimeBall","Slimeball",
      "eggCooked","Cooked Egg","ingotCopper","Copper Ingot","fishRaw","Raw Fish","fishCooked","Cooked Fish",
      "carbonFilament","Carbon Filament","fireExtinguisher","Fire Extinguisher","bucketSlime","Slime Bucket","clayRed","Red Clay",
      "redstone","Redstone","brick_blue","Blue Brick","plateSponge","Sponge Chestplate","helmetSponge","Sponge Helmet",
      "legsSponge","Sponge Leggings","bootsSponge","Sponge Boots"};
    for (int i = 0; i < kv.length; i += 2) NAMES.put(kv[i], kv[i + 1]);
  }
  static String pretty(String field) {
    if (field == null) return "?";
    if (NAMES.containsKey(field)) return NAMES.get(field);
    if (field.startsWith("record")) return "Music Disc";
    String[] mats = {"Wood","Stone","Steel","Diamond","Gold","Obsidian","Leather","Chain","Sponge"};
    String[] kinds = {"sword","shovel","pickaxe","axe","hoe","helmet","plate","legs","boots"};
    String[] kindNames = {"Sword","Shovel","Pickaxe","Axe","Hoe","Helmet","Chestplate","Leggings","Boots"};
    for (int k = 0; k < kinds.length; k++) for (String m : mats)
      if (field.equals(kinds[k] + m)) return (m.equals("Steel") ? "Iron" : m.equals("Wood") ? "Wooden" : m) + " " + kindNames[k];
    StringBuilder s = new StringBuilder();
    for (int i = 0; i < field.length(); i++) {
      char c = field.charAt(i);
      if (i == 0) s.append(Character.toUpperCase(c));
      else if (Character.isUpperCase(c)) s.append(' ').append(c);
      else s.append(c);
    }
    String r = s.toString();
    if (r.startsWith("Dye ")) r = r.substring(4) + " Dye";
    return r;
  }
  static String st(ItemStack s) { return s == null ? "-" : s.itemID + ":" + s.getItemDamage() + ":" + s.stackSize; }

  public static void main(String[] a) throws Exception {
    Class.forName("com.mojang.minecraft.core.level.tile.Block");
    Class.forName("com.mojang.minecraft.core.entity.item.Item");
    // RenderGlobal.loadRenderers, as soon as a world loads: leaves move to the TextureLeavesFX tile
    Block.leaves.setFastOrFancy(3);
    File dir = new File(a[0]); dir.mkdirs();
    Map<Object, String> mats = new IdentityHashMap<>();
    for (Field fl : Material.class.getDeclaredFields()) if (Modifier.isStatic(fl.getModifiers()) && fl.getType() == Material.class) mats.put(fl.get(null), fl.getName());
    Map<Object, String> bnames = new IdentityHashMap<>();
    for (Field fl : Block.class.getDeclaredFields()) if (Modifier.isStatic(fl.getModifiers()) && Block.class.isAssignableFrom(fl.getType())) { Object o = fl.get(null); if (o != null && !bnames.containsKey(o)) bnames.put(o, fl.getName()); }
    Map<Object, String> inames = new IdentityHashMap<>();
    for (Field fl : Item.class.getDeclaredFields()) if (Modifier.isStatic(fl.getModifiers()) && Item.class.isAssignableFrom(fl.getType())) { Object o = fl.get(null); if (o != null && !inames.containsKey(o)) inames.put(o, fl.getName()); }

    // blocks.json + blockprops.tsv
    try (PrintWriter j = new PrintWriter(new FileWriter(new File(dir, "blocks.json")));
         PrintWriter p = new PrintWriter(new FileWriter(new File(dir, "blockprops.tsv")))) {
      j.println("{");
      boolean first = true;
      for (int id = 1; id < 256; id++) {
        Block b = Block.allBlocks[id];
        if (b == null) continue;
        StringBuilder tex = new StringBuilder("[");
        for (int m = 0; m < 16; m++) {
          tex.append(m > 0 ? ",[" : "[");
          for (int s = 0; s < 6; s++) { int t; try { t = b.getBlockTextureFromSideAndMetadata(s, m); } catch (Throwable e) { t = -1; } tex.append(s > 0 ? "," : "").append(t); }
          tex.append("]");
        }
        tex.append("]");
        String field = bnames.get(b), disp = pretty(field);
        int rt; try { rt = b.getRenderType(); } catch (Throwable e) { rt = -1; }
        int pass; try { pass = b.getRenderBlockPass(); } catch (Throwable e) { pass = 0; }
        Material mat = (Material) f(b, "blockMaterial");
        if (!first) j.println(",");
        first = false;
        j.print("\"" + id + "\":{\"name\":\"tile." + field + "\",\"disp\":\"" + disp + "\",\"cls\":\"" + b.getClass().getSimpleName() + "\",\"render\":" + rt
          + ",\"opaque\":" + b.isOpaqueCube() + ",\"normal\":" + b.renderAsNormalBlock() + ",\"pass\":" + pass
          + ",\"opacity\":" + Block.lightOpacity[id] + ",\"light\":" + Block.lightValue[id]
          + ",\"liquid\":" + (mat == Material.water || mat == Material.lava)
          + ",\"bounds\":[" + f(b,"minX") + "," + f(b,"minY") + "," + f(b,"minZ") + "," + f(b,"maxX") + "," + f(b,"maxY") + "," + f(b,"maxZ") + "]"
          + ",\"index\":" + b.blockIndexInTexture
          + ",\"tex\":" + tex + "}");
        StepSound ss = b.stepSound;
        String step = ss == null ? "step.stone" : "step." + ss.field_1678_a, brk = ss == null ? "step.stone" : ss.stepSoundDir();
        p.println(id + "\t" + field + "\t" + b.getClass().getSimpleName() + "\t" + mats.get(mat) + "\t" + Block.lightOpacity[id] + "\t" + Block.lightValue[id]
          + "\t" + (b.isOpaqueCube() ? 1 : 0) + "\t" + (b.renderAsNormalBlock() ? 1 : 0) + "\t" + b.tickRate() + "\t" + (Block.isBlockContainer[id] ? 1 : 0)
          + "\t" + (Block.tickOnLoad[id] ? 1 : 0) + "\t" + (Block.opaqueCubeLookup[id] ? 1 : 0)
          + "\t" + step + "\t" + brk + "\t" + (ss == null ? 1.0f : ss.getVolume()) + "\t" + (ss == null ? 1.0f : ss.getPitch()));
      }
      j.println("\n}");
    }

    // items.tsv: id damage icon colour overlay overlayColour flags name
    try (PrintWriter w = new PrintWriter(new OutputStreamWriter(new FileOutputStream(new File(dir, "items.tsv")), "UTF-8"))) {
      for (int id = 1; id < Item.itemsList.length; id++) {
        Item it = Item.itemsList[id];
        if (it == null && (id >= 256 || Block.allBlocks[id] == null)) continue;
        int icon, flags = 0;
        String name;
        if (id < 256) {
          Block b = Block.allBlocks[id];
          int rt = b.getRenderType();
          if (rt == 0 || rt == 10 || rt == 11 || rt == 13) flags |= 1; // RenderBlocks.getRenderItemType
          try { icon = b.getTextureIndex(2); } catch (Throwable t) { icon = -1; }
          name = pretty(bnames.get(b));
        } else {
          flags |= 4;
          try { icon = new ItemStack(id, 1, 0).getIconIndex(); } catch (Throwable t) { icon = -1; }
          name = pretty(inames.get(it));
        }
        w.println(id + "\t0\t" + icon + "\t16777215\t-1\t16777215\t" + flags + "\t" + name);
      }
    }

    // survival.txt
    StringBuilder s = new StringBuilder();
    for (int i = 0; i < 256; i++) { Block k = Block.allBlocks[i]; if (k == null) continue; s.append("B ").append(i).append(' ').append(f(k, "blockHardness")).append('\n'); }
    for (int i = 0; i < Item.itemsList.length; i++) {
      Item it = Item.itemsList[i];
      if (it == null) continue;
      ItemStack st = new ItemStack(i, 1, 0);
      int bt = TileEntityFurnace.getCookTime(st);
      s.append("I ").append(i).append(' ').append(it.maxStackSize).append(' ').append(it.getMaxDmg()).append(' ').append(bt).append('\n');
      StringBuilder t = new StringBuilder(); boolean any = false;
      for (int k = 0; k < 256; k++) {
        Block bl = Block.allBlocks[k]; if (bl == null) continue;
        float fv = st.getStrVsBlock(bl); boolean hv = st.canHarvestBlock(bl);
        if (fv != 1.0F || hv) { any = true; t.append(' ').append(k).append('=').append(fv).append(hv ? "h" : ""); }
      }
      if (any) s.append("T ").append(i).append(t).append('\n');
    }
    List<?> recipes = (List<?>) f(CraftingManager.getInstance(), "recipes");
    for (Object o : recipes) {
      if (o instanceof ShapedRecipes) {
        ItemStack[] in = (ItemStack[]) f(o, "recipeItems");
        s.append("S ").append(f(o, "recipeWidth")).append(' ').append(f(o, "recipeHeight")).append(' ').append(st((ItemStack) f(o, "recipeOutput")));
        for (ItemStack x : in) s.append(' ').append(x == null ? "-" : x.itemID + ":" + x.getItemDamage());
        s.append('\n');
      } else if (o instanceof ShapelessRecipes) {
        s.append("L ").append(st((ItemStack) f(o, "recipeOutput")));
        for (Object x : (List<?>) f(o, "recipeItems")) { ItemStack y = (ItemStack) x; s.append(' ').append(y.itemID + ":" + y.getItemDamage()); }
        s.append('\n');
      } else s.append("? ").append(o.getClass().getName()).append('\n');
    }
    for (int i = 1; i < 32000; i++) { int r = TileEntityFurnace.getCookResult(i); if (r > 0) s.append("F ").append(i).append(' ').append(r).append(":0:1").append('\n'); }
    try (Writer w = new FileWriter(new File(dir, "survival.txt"))) { w.write(s.toString()); }

    // drops.txt: D id meta dropId damage qty  (R = random)
    Method dd = Block.class.getDeclaredMethod("damageDropped", int.class); dd.setAccessible(true);
    StringBuilder d = new StringBuilder();
    for (int i = 1; i < 256; i++) {
      Block k = Block.allBlocks[i]; if (k == null) continue;
      for (int m = 0; m < 16; m++) {
        Set<Integer> ids = new HashSet<>(), qs = new HashSet<>();
        for (int x = 0; x < 40; x++) { ids.add(k.idDropped(m, new Random(x * 7919L))); qs.add(k.quantityDropped(new Random(x * 104729L))); }
        int dmg = (Integer) dd.invoke(k, m);
        d.append("D ").append(i).append(' ').append(m).append(' ').append(ids.size() == 1 ? ids.iterator().next().toString() : "R").append(' ').append(dmg).append(' ').append(qs.size() == 1 ? qs.iterator().next().toString() : "R").append('\n');
      }
    }
    try (Writer w = new FileWriter(new File(dir, "drops.txt"))) { w.write(d.toString()); }
    System.out.println("ok");
  }
}
