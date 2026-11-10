package net.minecraft.src;

import java.io.*;
import java.lang.reflect.*;
import java.util.*;

// Dumps what Axeolon's own model classes produce, to compare with the 3DS port (hosttest/models.cpp):
//   S  sampleBone for every model/animation/bone/channel at a few times
//   M  mob vertices after blending two animations + a head turn, drawn the way renderBone/ModelRenderer do
//   R  block rig quads from AxeolonBlockRig.emit for every animation at a few times, yaws and pitches
// usage: java ... net.minecraft.src.CheckModels <minecraft/src/axeolon> <out.txt>
public class CheckModels {
	static PrintWriter out;
	static final float[] TIMES = {0.0F, 0.07F, 0.25F, 0.5F, 0.9F, 1.3F, 2.75F, 7.1F};

	public static void main(String[] a) throws Exception {
		Class.forName("net.minecraft.src.Block");
		out = new PrintWriter(new BufferedWriter(new FileWriter(a[1])));
		String[] subs = {"mob", "block"};
		for(int s = 0; s < 2; ++s) {
			File[] fs = new File(a[0], subs[s]).listFiles();
			Arrays.sort(fs);
			for(int i = 0; i < fs.length; ++i) {
				String n = fs[i].getName();
				if(!n.endsWith(".bbmodel")) continue;
				n = n.substring(0, n.length() - 8);
				AxeolonBBModel m = new AxeolonBBModel("/axeolon/" + subs[s] + "/" + n + ".bbmodel", s == 0 ? 64 : 64, s == 0 ? 32 : 64);
				samples(subs[s] + "/" + n, m);
				if(s == 0) mob(n, m);
				else rig(n);
			}
		}
		out.close();
	}

	static Object field(Object o, String f) throws Exception {
		Field fl = o.getClass().getDeclaredField(f);
		fl.setAccessible(true);
		return fl.get(o);
	}

	static List animNames(AxeolonBBModel m) throws Exception {
		m.getAnimation("x");
		List l = new ArrayList(((Map)field(m, "animations")).keySet());
		Collections.sort(l);
		return l;
	}

	static void samples(String id, AxeolonBBModel m) throws Exception {
		List names = animNames(m);
		Map bones = (Map)field(m, "bones");
		List bn = new ArrayList(bones.keySet());
		Collections.sort(bn);
		for(int i = 0; i < names.size(); ++i) {
			String an = (String)names.get(i);
			out.println("A " + id + " " + an + " " + m.getAnimationLength(an) + " " + m.getAnimation(an).loop);
			for(int b = 0; b < bn.size(); ++b) for(int c = 0; c < 3; ++c) for(int t = 0; t < TIMES.length; ++t) {
				float[] v = m.sampleBone(an, (String)bn.get(b), c, TIMES[t]);
				if(v != null) out.println("S " + id + " " + an + " " + bn.get(b) + " " + c + " " + TIMES[t] + " " + v[0] + " " + v[1] + " " + v[2]);
			}
		}
	}

	// ---- mob: GL emulated in float, as RenderLiving leaves it (identity here) ----
	static float[] mat;
	static ArrayList stack = new ArrayList();
	static void push() { stack.add(mat.clone()); }
	static void pop() { mat = (float[])stack.remove(stack.size() - 1); }
	static void mul(float[] b) {
		float[] o = new float[16];
		for(int i = 0; i < 4; ++i) for(int j = 0; j < 4; ++j) {
			float s = 0;
			for(int k = 0; k < 4; ++k) s += mat[i * 4 + k] * b[k * 4 + j];
			o[i * 4 + j] = s;
		}
		mat = o;
	}
	static void tr(float x, float y, float z) { mul(new float[] {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1}); }
	static void sc(float x, float y, float z) { mul(new float[] {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1}); }
	static void rot(float deg, int axis) {
		double r = Math.toRadians(deg);
		float c = (float)Math.cos(r), s = (float)Math.sin(r);
		if(axis == 0) mul(new float[] {1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0, 0, 0, 0, 1});
		else if(axis == 1) mul(new float[] {c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0, 0, 0, 0, 1});
		else mul(new float[] {c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
	}

	static void mob(String n, AxeolonBBModel m) throws Exception {
		List names = animNames(m);
		Map bones = (Map)field(m, "bones");
		List roots = (List)field(m, "roots");
		for(int i = 0; i < names.size(); ++i) {
			for(int t = 0; t < 3; ++t) {
				Iterator it = bones.values().iterator();
				while(it.hasNext()) {
					AxeolonBBModel.Bone b = (AxeolonBBModel.Bone)it.next();
					b.arx = b.ary = b.arz = b.apx = b.apy = b.apz = 0.0F;
					b.asx = b.asy = b.asz = 1.0F;
				}
				String an = (String)names.get(i), other = (String)names.get((i + 1) % names.size());
				m.apply(m.getAnimation(an), TIMES[t * 2 + 1], 0.75F);
				m.apply(m.getAnimation(other), TIMES[t * 2 + 2], 0.4F);
				AxeolonBBModel.Bone head = (AxeolonBBModel.Bone)bones.get("head");
				if(head != null) { head.ary += AxeolonModel.rad(25.0F) * 0.8F; head.arx += AxeolonModel.rad(-10.0F) * 0.8F; }
				mat = new float[] {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
				out.println("P mob/" + n + " " + an + " " + t);
				for(int r = 0; r < roots.size(); ++r) bone((AxeolonBBModel.Bone)roots.get(r), 0.0625F);
			}
		}
	}

	static void bone(AxeolonBBModel.Bone b, float s) throws Exception {
		push();
		tr((b.px + b.apx) * s, (b.py + b.apy) * s, (b.pz + b.apz) * s);
		float rz = b.rz + b.arz, ry = b.ry + b.ary, rx = b.rx + b.arx;
		if(rz != 0.0F) rot(rz * 57.29578F, 2);
		if(ry != 0.0F) rot(ry * 57.29578F, 1);
		if(rx != 0.0F) rot(rx * 57.29578F, 0);
		if(b.asx != 1.0F || b.asy != 1.0F || b.asz != 1.0F) sc(b.asx, b.asy, b.asz);
		for(int i = 0; i < b.cubes.size(); ++i) cube((ModelRenderer)b.cubes.get(i), s);
		for(int i = 0; i < b.children.size(); ++i) {
			push();
			tr(-b.px * s, -b.py * s, -b.pz * s);
			bone((AxeolonBBModel.Bone)b.children.get(i), s);
			pop();
		}
		pop();
	}

	static void cube(ModelRenderer r, float s) throws Exception {
		if(r.field_1402_i || !r.field_1403_h) return;
		push();
		tr(r.offsetX * s, r.offsetY * s, r.offsetZ * s);
		if(r.rotateAngleZ != 0.0F) rot(r.rotateAngleZ * (180.0F / (float)Math.PI), 2);
		if(r.rotateAngleY != 0.0F) rot(r.rotateAngleY * (180.0F / (float)Math.PI), 1);
		if(r.rotateAngleX != 0.0F) rot(r.rotateAngleX * (180.0F / (float)Math.PI), 0);
		TexturedQuad[] faces = (TexturedQuad[])field(r, "faces");
		for(int f = 0; f < faces.length; ++f) {
			StringBuilder sb = new StringBuilder("Q");
			for(int k = 0; k < 4; ++k) {
				PositionTexureVertex v = faces[f].vertexPositions[k];
				float x = (float)v.vector3D.xCoord * s, y = (float)v.vector3D.yCoord * s, z = (float)v.vector3D.zCoord * s;
				float wx = mat[0] * x + mat[1] * y + mat[2] * z + mat[3];
				float wy = mat[4] * x + mat[5] * y + mat[6] * z + mat[7];
				float wz = mat[8] * x + mat[9] * y + mat[10] * z + mat[11];
				sb.append(" " + wx + " " + wy + " " + wz + " " + v.texturePositionX + " " + v.texturePositionY);
			}
			out.println(sb);
		}
		pop();
	}

	// ---- block rigs ----
	static void rig(String n) throws Exception {
		AxeolonBlockRig rig = AxeolonBlockRig.of(n);
		List names = new ArrayList(animNames(rig.anims));
		names.add(0, null);
		Tessellator t = Tessellator.instance;
		float[][] poses = {{0, 0}, {90, 0}, {-90, 0}, {180, 0}, {0, 90}, {90, 90}, {0, -90}};
		for(int i = 0; i < names.size(); ++i) for(int k = 0; k < 4; ++k) for(int p = 0; p < poses.length; ++p) for(int h = 0; h < 3; ++h) {
			if(h > 0 && !n.startsWith("chest_double")) continue;
			if(p > 0 && k > 1) continue;
			float sec = TIMES[k * 2];
			setField(t, "rawBufferIndex", 0);
			setField(t, "vertexCount", 0);
			setField(t, "addedVertices", 0);
			t.startDrawingQuads();
			rig.emit(t, null, 0, 0, 0, (String)names.get(i), sec, poses[p][0], poses[p][1], h, 0.25D, 1.0D, -3.5D);
			int idx = ((Integer)field(t, "rawBufferIndex")).intValue();
			int[] raw = (int[])field(t, "rawBuffer");
			setField(t, "isDrawing", Boolean.FALSE);
			out.println("R " + n + " " + names.get(i) + " " + sec + " " + poses[p][0] + " " + poses[p][1] + " " + h + " " + idx / 8);
			for(int v = 0; v < idx; v += 8) {
				out.println("V " + Float.intBitsToFloat(raw[v]) + " " + Float.intBitsToFloat(raw[v + 1]) + " " + Float.intBitsToFloat(raw[v + 2]) + " "
					+ Float.intBitsToFloat(raw[v + 3]) + " " + Float.intBitsToFloat(raw[v + 4]) + " " + (raw[v + 5] & 255));
			}
		}
	}

	static void setField(Object o, String f, Object v) throws Exception {
		Field fl = o.getClass().getDeclaredField(f);
		fl.setAccessible(true);
		fl.set(o, v);
	}
}
