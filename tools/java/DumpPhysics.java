import net.minecraft.src.*;
import java.io.*;
import java.util.*;

// Everything the player physics needs from each block, read from the Axeolon build itself:
//   B id material solid liquid water lava ice slipperiness opaque
//   C id meta n  box*n (minX minY minZ maxX maxY maxZ relative to the block)  - isolated block
// usage: java DumpPhysics <tmp world dir> > physics.txt
public class DumpPhysics {
	public static void main(String[] a) throws Exception {
		World w = new World(new File(a[0]), "phys", 1202L);
		IdentityHashMap mats = new IdentityHashMap();
		int x = 8, y = 120, z = 8;
		Chunk c = w.getChunkFromBlockCoords(x, z);
		AxisAlignedBB all = AxisAlignedBB.getBoundingBox(x - 4, y - 4, z - 4, x + 5, y + 5, z + 5);
		for(int id = 1; id < 256; ++id) {
			Block b = Block.blocksList[id];
			if(b == null) continue;
			Material m = b.blockMaterial;
			if(!mats.containsKey(m)) mats.put(m, new Integer(mats.size()));
			System.out.println("B " + id + " " + mats.get(m) + " " + (m.getIsSolid() ? 1 : 0) + " " + (m.getIsLiquid() ? 1 : 0) + " "
				+ (m == Material.water ? 1 : 0) + " " + (m == Material.lava ? 1 : 0) + " " + (m == Material.ice ? 1 : 0) + " "
				+ b.slipperiness + " " + (b.isOpaqueCube() ? 1 : 0));
			for(int meta = 0; meta < 16; ++meta) {
				c.setBlockIDWithMetadata(x & 15, y, z & 15, id, meta);
				ArrayList l = new ArrayList();
				try { b.getCollidingBoundingBoxes(w, x, y, z, all, l); } catch(Throwable t) { l.clear(); }
				StringBuilder sb = new StringBuilder("C " + id + " " + meta + " " + l.size());
				for(int i = 0; i < l.size(); ++i) {
					AxisAlignedBB bb = (AxisAlignedBB)l.get(i);
					sb.append(" " + (bb.minX - x) + " " + (bb.minY - y) + " " + (bb.minZ - z) + " " + (bb.maxX - x) + " " + (bb.maxY - y) + " " + (bb.maxZ - z));
				}
				System.out.println(sb);
			}
			c.setBlockIDWithMetadata(x & 15, y, z & 15, 0, 0);
		}
	}
}
