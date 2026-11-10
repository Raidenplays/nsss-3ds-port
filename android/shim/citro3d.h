#pragma once
// Android stand-in for the citro3d the game uses outside the renderer: matrices (row-major, the
// same functions and argument order, but plain OpenGL projections: no 90 degree screen tilt,
// depth -1..1), textures (linear RGBA in memory, uploaded to a GL texture on flush), frames and the
// odd blend-state call from the HUD. The 3D renderer itself is android/src/RendererGL.cpp.
#include <cmath>
#include "3ds.h"

// ---- vectors and matrices ----
union C3D_FVec {
    struct { float w, z, y, x; };
    float c[4];
};
union C3D_Mtx {
    C3D_FVec r[4];
    float m[16];
};
#define C3D_AngleFromDegrees(d) ((d) * (float)M_PI / 180.0f)
#define C3D_AspectRatioTop (400.0f / 240.0f)
#define C3D_AspectRatioBot (320.0f / 240.0f)

void Mtx_Zeros(C3D_Mtx* out);
void Mtx_Identity(C3D_Mtx* out);
void Mtx_Copy(C3D_Mtx* out, const C3D_Mtx* in);
void Mtx_Multiply(C3D_Mtx* out, const C3D_Mtx* a, const C3D_Mtx* b);
void Mtx_Translate(C3D_Mtx* mtx, float x, float y, float z, bool bRightSide);
void Mtx_Scale(C3D_Mtx* mtx, float x, float y, float z);
void Mtx_RotateX(C3D_Mtx* mtx, float angle, bool bRightSide);
void Mtx_RotateY(C3D_Mtx* mtx, float angle, bool bRightSide);
void Mtx_RotateZ(C3D_Mtx* mtx, float angle, bool bRightSide);
// OpenGL perspective (the aspect is the window's, set by the platform layer when the argument is
// the 3DS top screen's): fovy in radians
void Mtx_PerspTilt(C3D_Mtx* mtx, float fovy, float aspect, float near, float far, bool isLeftHanded);
void Mtx_PerspStereoTilt(C3D_Mtx* mtx, float fovy, float aspect, float near, float far, float iod, float screen,
                         bool isLeftHanded);
void Mtx_OrthoTilt(C3D_Mtx* mtx, float left, float right, float bottom, float top, float near, float far,
                   bool isLeftHanded);

// ---- textures ----
enum GPU_TEXCOLOR { GPU_RGBA8 = 0, GPU_RGB8 = 1, GPU_RGBA5551 = 2, GPU_RGB565 = 3, GPU_RGBA4 = 4, GPU_LA8 = 5, GPU_A8 = 8 };
enum GPU_TEXTURE_FILTER_PARAM { GPU_NEAREST = 0, GPU_LINEAR = 1 };
enum GPU_TEXTURE_WRAP_PARAM { GPU_CLAMP_TO_EDGE = 0, GPU_CLAMP_TO_BORDER = 1, GPU_REPEAT = 2, GPU_MIRRORED_REPEAT = 3 };
struct C3D_Tex {
    void* data = nullptr; // width*height u32 pixels 0xRRGGBBAA, row 0 = the top of the image
    u16 width = 0, height = 0;
    unsigned id = 0;      // the GL texture
    int wrap = GPU_CLAMP_TO_EDGE;
    int filter = GPU_NEAREST;
};
bool C3D_TexInit(C3D_Tex* tex, u16 width, u16 height, GPU_TEXCOLOR format);
void C3D_TexDelete(C3D_Tex* tex);
void C3D_TexFlush(C3D_Tex* tex); // (re)uploads data
void C3D_TexSetFilter(C3D_Tex* tex, GPU_TEXTURE_FILTER_PARAM magFilter, GPU_TEXTURE_FILTER_PARAM minFilter);
void C3D_TexSetWrap(C3D_Tex* tex, GPU_TEXTURE_WRAP_PARAM wrapS, GPU_TEXTURE_WRAP_PARAM wrapT);

// ---- frames and targets ----
#define C3D_DEFAULT_CMDBUF_SIZE 0x40000
#define C3D_FRAME_SYNCDRAW 1
enum { C3D_CLEAR_COLOR = 1, C3D_CLEAR_DEPTH = 2, C3D_CLEAR_ALL = 3 };
enum GPU_COLORBUF { GPU_RB_RGBA8 = 0, GPU_RB_RGB8 = 1 };
enum GPU_DEPTHBUF { GPU_RB_DEPTH16 = 0, GPU_RB_DEPTH24 = 2, GPU_RB_DEPTH24_STENCIL8 = 3 };
struct C3D_RenderTarget {
    int screen = 0; // GFX_TOP / GFX_BOTTOM
};
inline bool C3D_Init(size_t) { return true; }
inline void C3D_Fini() {}
bool C3D_FrameBegin(u8 flags);
void C3D_FrameEnd(u8 flags); // presents the frame
inline void C3D_RenderTargetSetOutput(C3D_RenderTarget*, gfxScreen_t, gfx3dSide_t, u32) {}
inline void C3D_RenderTargetDelete(C3D_RenderTarget*) {}
// GX transfer flags: nothing to transfer on Android
#define GX_TRANSFER_FLIP_VERT(x) 0
#define GX_TRANSFER_OUT_TILED(x) 0
#define GX_TRANSFER_RAW_COPY(x) 0
#define GX_TRANSFER_IN_FORMAT(x) 0
#define GX_TRANSFER_OUT_FORMAT(x) 0
#define GX_TRANSFER_SCALING(x) 0
#define GX_TRANSFER_FMT_RGBA8 0
#define GX_TRANSFER_FMT_RGB8 1
#define GX_TRANSFER_SCALE_NO 0
#define GX_BUFFER_DIM(w, h) (((u32)(h) << 16) | ((u32)(w)&0xFFFF))
// the bottom screen's map: linear RGBA (0xRRGGBBAA, top row first) into a texture
void C3D_SyncDisplayTransfer(u32* inadr, u32 indim, u32* outadr, u32 outdim, u32 flags);

// ---- blending (the HUD switches it for the crosshair) ----
enum GPU_BLENDEQUATION { GPU_BLEND_ADD = 0, GPU_BLEND_SUBTRACT = 1, GPU_BLEND_REVERSE_SUBTRACT = 2 };
enum GPU_BLENDFACTOR {
    GPU_ZERO = 0, GPU_ONE = 1, GPU_SRC_COLOR = 2, GPU_ONE_MINUS_SRC_COLOR = 3, GPU_DST_COLOR = 4,
    GPU_ONE_MINUS_DST_COLOR = 5, GPU_SRC_ALPHA = 6, GPU_ONE_MINUS_SRC_ALPHA = 7, GPU_DST_ALPHA = 8,
    GPU_ONE_MINUS_DST_ALPHA = 9
};
void C3D_AlphaBlend(GPU_BLENDEQUATION colorEq, GPU_BLENDEQUATION alphaEq, GPU_BLENDFACTOR srcClr,
                    GPU_BLENDFACTOR dstClr, GPU_BLENDFACTOR srcAlpha, GPU_BLENDFACTOR dstAlpha);
