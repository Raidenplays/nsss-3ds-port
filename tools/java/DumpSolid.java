import net.minecraft.src.*;
public class DumpSolid { public static void main(String[] a){ StringBuilder s=new StringBuilder();
 for(int i=0;i<256;i++){ Block b=Block.blocksList[i]; s.append(b!=null&&b.blockMaterial.isSolid()?'1':'0'); } System.out.println(s); } }
