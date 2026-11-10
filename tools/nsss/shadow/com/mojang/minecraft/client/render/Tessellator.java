package com.mojang.minecraft.client.render;
import java.io.*;
// A stand-in for the jar's Tessellator, for tools/nsss BlockSimDump's "render" command (put ahead of
// the jar on the classpath): RenderBlocks' quads are written out instead of drawn. A vertex is kept as
// the jar's chunk tessellator keeps it - (float)(d + offset), (float)u, (float)v - and its colour as
// setColorOpaque_F makes it.
public class Tessellator {
   public static final Tessellator instance = new Tessellator();
   public static int chunkOffsetX = 0, chunkOffsetZ = 0;
   public static DataOutputStream out;
   public static int bx, by, bz; // the block being rendered
   private double xOffset, yOffset, zOffset, u, v;
   private int color;
   private final float[] q = new float[20];
   private int n;
   public boolean isDrawing;

   public void setColorOpaque_F(float r, float g, float b) { this.setColorOpaque((int)(r * 255.0F), (int)(g * 255.0F), (int)(b * 255.0F)); }
   public void setColorRGBA_F(float r, float g, float b, float a) { this.setColorOpaque((int)(r * 255.0F), (int)(g * 255.0F), (int)(b * 255.0F)); }
   public void setColorOpaque(int r, int g, int b) {
      r = Math.max(0, Math.min(255, r)); g = Math.max(0, Math.min(255, g)); b = Math.max(0, Math.min(255, b));
      this.color = r << 16 | g << 8 | b;
   }
   public void setTranslationF(float x, float y, float z) { this.xOffset += x; this.yOffset += y; this.zOffset += z; }
   public void setTranslationD(double x, double y, double z) { this.xOffset = x; this.yOffset = y; this.zOffset = z; }
   public void setNormal(float x, float y, float z) {}
   public void setRenderingChunk(boolean b) {}
   public void startDrawingQuads() { this.n = 0; this.isDrawing = true; }
   public void startDrawing(int mode) { this.startDrawingQuads(); }
   public void draw() { this.isDrawing = false; }
   public void setTextureUV(double u, double v) { this.u = u; this.v = v; }
   public void addVertexWithUV(double x, double y, double z, double u, double v) { this.setTextureUV(u, v); this.addVertex(x, y, z); }
   public void addVertex(double x, double y, double z) {
      int o = this.n * 5;
      this.q[o] = (float)(x + this.xOffset);
      this.q[o + 1] = (float)(y + this.yOffset);
      this.q[o + 2] = (float)(z + this.zOffset);
      this.q[o + 3] = (float)this.u;
      this.q[o + 4] = (float)this.v;
      if (++this.n == 4) {
         this.n = 0;
         try {
            out.writeInt(bx); out.writeInt(by); out.writeInt(bz);
            for (int i = 0; i < 20; i++) out.writeInt(Float.floatToRawIntBits(this.q[i]));
            out.writeInt(this.color);
         } catch (IOException e) { throw new RuntimeException(e); }
      }
   }
}
