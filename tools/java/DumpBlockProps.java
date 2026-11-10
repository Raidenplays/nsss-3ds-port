import net.minecraft.src.*;
import java.lang.reflect.*;
import java.util.*;
public class DumpBlockProps { public static void main(String[] a) throws Exception {
  Class.forName("net.minecraft.src.Block");
  ((BlockLeaves)Block.leaves).setGraphicsLevel(true); // fancy graphics, the game default
  Map<Object,String> mats = new IdentityHashMap<>();
  for (Field f : Material.class.getDeclaredFields()) if (Modifier.isStatic(f.getModifiers()) && f.getType()==Material.class) mats.put(f.get(null), f.getName());
  Map<Object,String> blocks = new IdentityHashMap<>();
  for (Field f : Block.class.getDeclaredFields()) if (Modifier.isStatic(f.getModifiers()) && Block.class.isAssignableFrom(f.getType()) ) { Object o=f.get(null); if(o!=null && !blocks.containsKey(o)) blocks.put(o, f.getName()); }
  for (int i = 0; i < 256; i++) { Block b = Block.blocksList[i]; if (b == null) continue;
    System.out.println(i+"\t"+blocks.getOrDefault(b,"?")+"\t"+b.getClass().getSimpleName()+"\t"+mats.get(b.blockMaterial)+"\t"+Block.lightOpacity[i]+"\t"+Block.lightValue[i]+"\t"+(b.isOpaqueCube()?1:0)+"\t"+(b.renderAsNormalBlock()?1:0)+"\t"+b.tickRate()+"\t"+(Block.isBlockContainer[i]?1:0)+"\t"+(Block.tickOnLoad[i]?1:0)+"\t"+(Block.opaqueCubeLookup[i]?1:0)+"\t"+b.stepSound.func_1145_d()+"\t"+b.stepSound.func_1146_a()+"\t"+b.stepSound.func_1147_b()+"\t"+b.stepSound.func_1144_c()); }
}}
