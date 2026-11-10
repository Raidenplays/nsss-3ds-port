package net.minecraft.src;

import java.io.*;

// Walks Axeolon's own EntityPlayer through a world with a fixed script of inputs and prints where
// it is every tick. hosttest/physics.cpp runs the 3DS port on the same script; the two must match.
// usage: java net.minecraft.src.SimPlayer <saves dir> <world> x y z yaw  (y = saved Pos[1], eye level)
public class SimPlayer extends EntityPlayer {
	float sf, fw; boolean jp, sn;

	public SimPlayer(World w) { super(w); }

	public boolean isSneaking() { return this.sn; }

	protected void updatePlayerActionState() {
		super.updatePlayerActionState();
		this.moveStrafing = this.sf;
		this.moveForward = this.fw;
		this.isJumping = this.jp;
	}

	public void onLivingUpdate() {
		if(this.sn && this.ySize < 0.2F) this.ySize = 0.2F; // EntityPlayerSP
		super.onLivingUpdate();
	}

	public void func_6420_o() {}

	// The script: tick -> inputs (shared with physics.cpp)
	static void inputs(SimPlayer p, int t) {
		p.sf = 0; p.fw = 0; p.jp = false; p.sn = false;
		if(t < 40) p.fw = 1;
		else if(t < 70) { p.fw = 1; p.rotationYaw += 6.0F; }
		else if(t < 100) { p.fw = 1; p.jp = true; }
		else if(t < 140) { p.sf = 1; p.sn = true; }
		else if(t < 200) { p.fw = 1; p.rotationYaw -= 3.0F; p.jp = (t % 15) < 3; }
		else if(t < 240) { p.fw = -1; p.sf = -1; }
		if(p.sn) { p.sf = (float)((double)p.sf * 0.3D); p.fw = (float)((double)p.fw * 0.3D); } // MovementInputFromOptions
	}

	public static void main(String[] a) throws Exception {
		Class.forName("net.minecraft.src.Block");
		World w = new World(new File(a[0]), a[1]);
		SimPlayer p = new SimPlayer(w);
		p.preparePlayerToSpawn();
		double x = Double.parseDouble(a[2]), y = Double.parseDouble(a[3]), z = Double.parseDouble(a[4]);
		float yaw = Float.parseFloat(a[5]);
		p.setPositionAndRotation(x, y, z, yaw, 0.0F);
		p.motionX = p.motionY = p.motionZ = 0;
		// Beta only keeps a 31x31-chunk window around the player loaded (EntityPlayerSP moves it)
		java.lang.reflect.Field f = World.class.getDeclaredField("chunkProvider");
		f.setAccessible(true);
		((ChunkProviderLoadOrGenerate)f.get(w)).func_21110_c((int)Math.floor(x) >> 4, (int)Math.floor(z) >> 4);
		for(int cx = ((int)Math.floor(x) >> 4) - 4; cx <= ((int)Math.floor(x) >> 4) + 4; ++cx)
			for(int cz = ((int)Math.floor(z) >> 4) - 4; cz <= ((int)Math.floor(z) >> 4) + 4; ++cz) w.getChunkFromChunkCoords(cx, cz);
		for(int t = 0; t < 240; ++t) {
			inputs(p, t);
			p.onUpdate();
			System.out.println(t + " " + p.posX + " " + p.posY + " " + p.posZ + " " + p.motionX + " " + p.motionY + " " + p.motionZ + " " + (p.onGround ? 1 : 0));
		}
	}
}
