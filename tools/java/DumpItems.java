package net.minecraft.src;

// Every item's GUI icon as RenderItem draws it: icon index (per damage for items with subtypes),
// tint colour, optional overlay icon + tint, and whether a block draws as a 3D cube.
// Output lines: id damage icon colour overlay overlayColour flags name
//   flags: 1 = block drawn 3D, 2 = icon from terrain2 (block icons >= 256), 4 = item (gui/items.png)
public class DumpItems {
	public static void main(String[] a) throws Exception {
		Class.forName("net.minecraft.src.Block");
		Class.forName("net.minecraft.src.Item");
		for(int id = 1; id < Item.itemsList.length; ++id) {
			Item it = Item.itemsList[id];
			if(it == null) continue;
			int n = it.getHasSubtypes() ? 16 : 1;
			for(int d = 0; d < n; ++d) {
				ItemStack s = new ItemStack(id, 1, d);
				int flags = 0, icon;
				try { icon = s.getIconIndex(); } catch(Throwable t) { icon = -1; }
				if(id < 256) {
					Block b = Block.blocksList[id];
					if(b == null) continue;
					if(RenderBlocks.func_1219_a(b.getRenderType())) flags |= 1;
					if(AxeolonTerrain2.uses(b, d) || icon >= 256) flags |= 2;
				} else flags |= 4;
				int col = 0xFFFFFF, ov = -1, ovc = 0xFFFFFF;
				try { col = it.getColorFromDamage(d, 0); ov = it.getOverlayIconIndex(d); if(ov >= 0) ovc = it.getColorFromDamage(d, 1); } catch(Throwable t) {}
				String name;
				try { name = StringTranslate.getInstance().translateNamedKey(it.getItemNameIS(s)).trim(); } catch(Throwable t) { name = ""; }
				if(name.length() == 0) name = String.valueOf(it.getItemNameIS(s));
				System.out.println(id + "\t" + d + "\t" + icon + "\t" + col + "\t" + ov + "\t" + ovc + "\t" + flags + "\t" + name);
			}
		}
	}
}
