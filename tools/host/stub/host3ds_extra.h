// Host-only: the few libctru/citro3d declarations tools/host/shim lacks, so tools/host/check.sh can
// syntax-check every source file on a PC. Never used by the real 3DS build.
#pragma once
#include <cstddef>
#include <cstdint>
struct DVLB_s { int numDVLE; struct DVLE_s* DVLE; };
struct shaderProgram_s { int dummy; };
inline void svcOutputDebugString(const char*, int) {}
inline void svcBreak(int) {}
enum { USERBREAK_PANIC = 0 };
inline unsigned linearSpaceFree() { return 0; }
inline int APT_SetAppCpuTimeLimit(unsigned) { return 0; }
