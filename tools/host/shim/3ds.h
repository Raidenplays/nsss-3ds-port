#pragma once
// Android stand-in for the parts of libctru the game uses: integer types, time, threads and locks,
// linear memory, the app loop, the buttons / Circle Pad / C-stick / touch screen (driven by the
// Pocket Edition touch controls, see android/src/Platform.cpp) and the DSP sound channels (a
// software mixer on AAudio, see android/src/Ndsp.cpp).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef int32_t Result;
typedef u32 Handle;
#define R_SUCCEEDED(r) ((r) >= 0)
#define R_FAILED(r) ((r) < 0)
#define U64_MAX UINT64_MAX

// ---- time ----
#define SYSCLOCK_ARM11 268111856ULL
u64 osGetTime();         // milliseconds
u64 svcGetSystemTick();  // ARM11 ticks
void svcSleepThread(s64 ns);
inline void osSetSpeedupEnable(bool) {}

// ---- threads ----
#define CUR_THREAD_HANDLE 0xFFFF8000u
typedef struct AxThread* Thread;
typedef void (*ThreadFunc)(void*);
Thread threadCreate(ThreadFunc entry, void* arg, size_t stackSize, int prio, int core, bool detached);
Result threadJoin(Thread t, u64 timeoutNs);
void threadFree(Thread t);
Result svcGetThreadId(u32* out, Handle h);
Result svcGetThreadPriority(s32* out, Handle h);

struct LightLock { void* impl = nullptr; };
void LightLock_Init(LightLock* l);
void LightLock_Lock(LightLock* l);
void LightLock_Unlock(LightLock* l);
enum ResetType { RESET_ONESHOT = 0, RESET_STICKY = 1, RESET_PULSE = 2 };
struct LightEvent { void* impl = nullptr; };
void LightEvent_Init(LightEvent* e, ResetType t);
void LightEvent_Signal(LightEvent* e);
void LightEvent_Wait(LightEvent* e);

// ---- memory (no separate linear heap: ordinary memory) ----
void* linearAlloc(size_t size);
void linearFree(void* p);
u32 linearGetSize(void* p);
inline Result GSPGPU_FlushDataCache(const void*, u32) { return 0; }
inline Result DSP_FlushDataCache(const void*, u32) { return 0; }

// ---- the app ----
bool aptMainLoop();
inline Result APT_CheckNew3DS(bool* out) { *out = true; return 0; } // the C-stick / ZL / ZR layout
inline Result romfsInit() { return 0; }
inline Result romfsExit() { return 0; }
enum gfxScreen_t { GFX_TOP = 0, GFX_BOTTOM = 1 };
enum gfx3dSide_t { GFX_LEFT = 0, GFX_RIGHT = 1 };
inline void gfxInitDefault() {}
inline void gfxExit() {}
inline void gfxSet3D(bool) {}
inline void gfxSetDoubleBuffering(gfxScreen_t, bool) {}
void gspWaitForVBlank();
float osGet3DSliderState(); // always 0: no stereoscopic 3D

// ---- input ----
enum {
    KEY_A = 1u << 0, KEY_B = 1u << 1, KEY_SELECT = 1u << 2, KEY_START = 1u << 3,
    KEY_DRIGHT = 1u << 4, KEY_DLEFT = 1u << 5, KEY_DUP = 1u << 6, KEY_DDOWN = 1u << 7,
    KEY_R = 1u << 8, KEY_L = 1u << 9, KEY_X = 1u << 10, KEY_Y = 1u << 11,
    KEY_ZL = 1u << 14, KEY_ZR = 1u << 15, KEY_TOUCH = 1u << 20,
    KEY_CSTICK_RIGHT = 1u << 24, KEY_CSTICK_LEFT = 1u << 25, KEY_CSTICK_UP = 1u << 26, KEY_CSTICK_DOWN = 1u << 27,
    KEY_CPAD_RIGHT = 1u << 28, KEY_CPAD_LEFT = 1u << 29, KEY_CPAD_UP = 1u << 30, KEY_CPAD_DOWN = 1u << 31,
    KEY_UP = KEY_DUP | KEY_CPAD_UP, KEY_DOWN = KEY_DDOWN | KEY_CPAD_DOWN,
    KEY_LEFT = KEY_DLEFT | KEY_CPAD_LEFT, KEY_RIGHT = KEY_DRIGHT | KEY_CPAD_RIGHT,
};
struct touchPosition { u16 px, py; };
struct circlePosition { s16 dx, dy; };
void hidScanInput();
u32 hidKeysDown();
u32 hidKeysHeld();
u32 hidKeysUp();
void hidTouchRead(touchPosition* t);
void hidCircleRead(circlePosition* p);
void hidCstickRead(circlePosition* p);
inline Result irrstInit() { return -1; } // no Circle Pad Pro
inline void irrstExit() {}
inline void irrstScanInput() {}
inline u32 irrstKeysHeld() { return 0; }
inline void irrstCstickRead(circlePosition* p) { p->dx = p->dy = 0; }

// ---- the DSP: 24 channels, buffers of PCM16 queued per channel ----
enum { NDSP_WBUF_FREE = 0, NDSP_WBUF_QUEUED = 1, NDSP_WBUF_PLAYING = 2, NDSP_WBUF_DONE = 3 };
enum { NDSP_OUTPUT_MONO = 0, NDSP_OUTPUT_STEREO = 1, NDSP_OUTPUT_SURROUND = 2 };
enum { NDSP_INTERP_POLYPHASE = 0, NDSP_INTERP_LINEAR = 1, NDSP_INTERP_NONE = 2 };
enum { NDSP_FORMAT_MONO_PCM16 = 5, NDSP_FORMAT_STEREO_PCM16 = 10 };
struct ndspWaveBuf {
    union {
        s16* data_pcm16;
        void* data_vaddr;
    };
    u32 nsamples = 0;
    void* adpcm_data = nullptr;
    u32 offset = 0;
    bool looping = false;
    volatile u8 status = NDSP_WBUF_FREE;
    u16 sequence_id = 0;
    ndspWaveBuf* next = nullptr;
    ndspWaveBuf() : data_vaddr(nullptr) {}
};
Result ndspInit();
void ndspExit();
void ndspSetOutputMode(int mode);
void ndspChnReset(int id);
void ndspChnSetInterp(int id, int type);
void ndspChnSetRate(int id, float rate);
void ndspChnSetFormat(int id, u16 format);
void ndspChnSetMix(int id, float mix[12]);
void ndspChnWaveBufAdd(int id, ndspWaveBuf* buf);
void ndspChnWaveBufClear(int id);
bool ndspChnIsPlaying(int id);

// ---- the software keyboard (sign text): Android's keyboard, see android/src/Platform.cpp ----
enum SwkbdType { SWKBD_TYPE_NORMAL = 0, SWKBD_TYPE_QWERTY = 1 };
enum SwkbdButton { SWKBD_BUTTON_NONE = -1, SWKBD_BUTTON_LEFT = 0, SWKBD_BUTTON_MIDDLE = 1, SWKBD_BUTTON_RIGHT = 2,
                   SWKBD_BUTTON_CONFIRM = 2 };
enum { SWKBD_MULTILINE = 1 << 0 };
struct SwkbdState {
    const char* hint = "";
    int maxLen = 0;
    u32 features = 0;
};
inline void swkbdInit(SwkbdState* s, SwkbdType, int, int maxLen) { s->maxLen = maxLen; }
inline void swkbdSetHintText(SwkbdState* s, const char* hint) { s->hint = hint; }
inline void swkbdSetInitialText(SwkbdState*, const char*) {}
inline void swkbdSetFeatures(SwkbdState* s, u32 f) { s->features = f; }
SwkbdButton swkbdInputText(SwkbdState* s, char* buf, size_t size);
