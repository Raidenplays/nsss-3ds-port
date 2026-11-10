package net.minecraft.src;
public class DumpModelBlocks {
	public static void main(String[] a) throws Exception {
		Class.forName("net.minecraft.src.Block");
		for(int i = 0; i < 256; ++i) {
			Block b = Block.blocksList[i];
			if(b == null || !AxeolonBlockAnims.isModelBlock(b)) continue;
			String extra = "";
			if(b instanceof BlockDoor) extra = "door wood=" + ((BlockDoor)b).woodType + " iron=" + (b.blockMaterial == Material.iron);
			if(b instanceof BlockPressurePlate) extra = "plate base=" + ((BlockPressurePlate)b).woodBase;
			if(b instanceof BlockPistonBase) extra = "piston sticky=" + ((BlockPistonBase)b).sticky;
			System.out.println(i + " " + b.getClass().getSimpleName() + " " + extra);
		}
		System.out.println("crate=" + Block.crate.blockID + " storageCrate=" + Block.storageCrate.blockID + " lever=" + Block.lever.blockID + " button=" + Block.button.blockID + " ovenIdle=" + Block.stoneOvenIdle.blockID + " ovenActive=" + Block.stoneOvenActive.blockID + " dispenser=" + Block.dispenser.blockID + " music=" + Block.musicBlock.blockID);
	}
}
