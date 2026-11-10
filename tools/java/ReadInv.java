package net.minecraft.src;
import java.io.*;
public class ReadInv { public static void main(String[] a) throws Exception {
 Class.forName("net.minecraft.src.Block"); Class.forName("net.minecraft.src.Item");
 NBTTagCompound r = CompressedStreamTools.func_1138_a(new FileInputStream(a[0]));
 NBTTagCompound p = r.getCompoundTag("Data").getCompoundTag("Player");
 InventoryPlayer inv = new InventoryPlayer(null); inv.readFromNBT(p.getTagList("Inventory"));
 int n=0; for(int i=0;i<inv.mainInventory.length;i++) if(inv.mainInventory[i]!=null) n++;
 System.out.println("Java reads " + n + " stacks; slot 36 = " + inv.mainInventory[36] + ", charm0 = " + inv.charms.charms[0] + ", AxBattleSlot=" + p.getInteger("AxBattleSlot") + " waypoints=" + p.getString("AxWaypoints"));
}}
