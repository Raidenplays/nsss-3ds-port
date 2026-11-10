import net.minecraft.src.*;
import java.io.File;
public class DumpLight { public static void main(String[] a){ World w = new World(new File(a[0]), a[1], 0L); for(int i=0;i<16;i++) System.out.print(w.worldProvider.lightBrightnessTable[i]+"f, "); System.out.println(); System.out.println("skylightSubtracted@time0 " + w.skylightSubtracted); } }
