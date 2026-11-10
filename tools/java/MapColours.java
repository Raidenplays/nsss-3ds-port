package net.minecraft.src;
public class MapColours { public static void main(String[] a) throws Exception {
 Class.forName("net.minecraft.src.Block"); java.lang.reflect.Method m = AxeolonMap.class.getDeclaredMethod("baseColour", int.class); m.setAccessible(true);
 StringBuilder sb = new StringBuilder(); for(int i=0;i<256;i++){ sb.append(String.format("0x%06X,", m.invoke(null, i))); if(i%8==7) sb.append("\n"); } System.out.print(sb);
 System.out.println("snow="+Block.snow.blockID+" water="+Block.waterStill.blockID+","+Block.waterMoving.blockID);
}}
