#pragma once
// The jar's (Alpha's) MathHelper: sine and cosine come from a 65536-entry table indexed with
// 10430.38F (Beta's 10430.378F is a different float), and world generation, movement and rendering
// all depend on getting the same values as Java.
#include <cmath>

namespace MathHelper {
const float* sinTable(); // 65536 floats, built on first use
inline float sin(float v) { return sinTable()[int(v * 10430.38f) & 0xFFFF]; }
inline float cos(float v) { return sinTable()[int(v * 10430.38f + 16384.0f) & 0xFFFF]; }
inline float sqrt_float(float v) { return float(std::sqrt(double(v))); }
inline float sqrt_double(double v) { return float(std::sqrt(v)); }
inline int floor_double(double v) {
    int i = int(v);
    return v < double(i) ? i - 1 : i;
}
inline int floor_float(float v) {
    int i = int(v);
    return v < float(i) ? i - 1 : i;
}
inline float abs(float v) { return v >= 0.0f ? v : -v; }
} // namespace MathHelper
