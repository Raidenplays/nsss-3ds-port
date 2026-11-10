#pragma once
// Time of day, sky and fog colours: WorldProvider.calculateCelestialAngle, World.calculateSkylight-
// Subtracted, World.func_4079_a (sky), WorldProvider.func_4096_a (fog), World.func_628_d (clouds),
// WorldProvider.func_4097_b (sunrise), World.func_679_f (stars) and EntityRenderer.updateFogColor.
#include <cmath>
#include <cstdint>
#include "world/MathHelper.h"

namespace atmosphere {

constexpr float PI_F = 3.1415927f;

struct RGB { float r, g, b; };

inline float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

inline float celestialAngle(int64_t time, float partial, bool nether) {
    if (nether) return 0.5f;
    int t = int(time % 24000LL);
    float f = (float(t) + partial) / 24000.0f - 0.25f;
    if (f < 0.0f) ++f;
    if (f > 1.0f) --f;
    float f0 = f;
    f = 1.0f - float((std::cos(double(f) * 3.141592653589793) + 1.0) / 2.0);
    return f0 + (f - f0) / 3.0f;
}

inline float daylight(float angle) { return clamp01(MathHelper::cos(angle * PI_F * 2.0f) * 2.0f + 0.5f); }

inline int skylightSubtracted(float angle) {
    float f = clamp01(1.0f - (MathHelper::cos(angle * PI_F * 2.0f) * 2.0f + 0.5f));
    return int(f * 11.0f);
}

// java.awt.Color.HSBtoRGB
inline uint32_t hsb(float h, float s, float v) {
    int r = 0, g = 0, b = 0;
    if (s == 0.0f) {
        r = g = b = int(v * 255.0f + 0.5f);
    } else {
        float hh = (h - std::floor(h)) * 6.0f;
        float f = hh - std::floor(hh);
        float p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        float R, G, B;
        switch (int(hh)) {
        case 0: R = v; G = t; B = p; break;
        case 1: R = q; G = v; B = p; break;
        case 2: R = p; G = v; B = t; break;
        case 3: R = p; G = q; B = v; break;
        case 4: R = t; G = p; B = v; break;
        default: R = v; G = p; B = q; break;
        }
        r = int(R * 255.0f + 0.5f);
        g = int(G * 255.0f + 0.5f);
        b = int(B * 255.0f + 0.5f);
    }
    return uint32_t(r << 16 | g << 8 | b);
}

// NSSS's World.calculateSkyTopColor: Alpha's fixed skyTopColor 0x88BBFF times the daylight
// (no biomes, so no temperature; the argument is kept for the callers)
inline RGB skyColour(float angle, double temperature) {
    (void)temperature;
    const uint32_t c = 0x88BBFF;
    float d = daylight(angle);
    return {float(c >> 16 & 255) / 255.0f * d, float(c >> 8 & 255) / 255.0f * d, float(c & 255) / 255.0f * d};
}

inline RGB fogColour(float angle, bool nether) {
    if (nether) return {0.2f, 0.03f, 0.03f};
    float d = daylight(angle);
    return {192.0f / 255.0f * (d * 0.94f + 0.06f), 216.0f / 255.0f * (d * 0.94f + 0.06f), 1.0f * (d * 0.91f + 0.09f)};
}

inline RGB cloudColour(float angle) {
    float d = daylight(angle);
    return {1.0f * (d * 0.9f + 0.1f), 1.0f * (d * 0.9f + 0.1f), 1.0f * (d * 0.85f + 0.15f)};
}

// func_4097_b: the sunrise/sunset glow, or false outside it
inline bool sunrise(float angle, float out[4]) {
    float span = 0.4f;
    float c = MathHelper::cos(angle * PI_F * 2.0f) - 0.0f;
    if (c >= -span && c <= span) {
        float f = (c - 0.0f) / span * 0.5f + 0.5f;
        float a = 1.0f - (1.0f - MathHelper::sin(f * PI_F)) * 0.99f;
        a *= a;
        out[0] = f * 0.3f + 0.7f;
        out[1] = f * f * 0.7f + 0.2f;
        out[2] = f * f * 0.0f + 0.2f;
        out[3] = a;
        return true;
    }
    return false;
}

// func_679_f
inline float starBrightness(float angle) {
    float f = 1.0f - (MathHelper::cos(angle * PI_F * 2.0f) * 2.0f + 0.75f);
    f = clamp01(f);
    return f * f * 0.5f;
}

inline uint32_t pack(RGB c) {
    auto b = [](float v) { return uint32_t(clamp01(v) * 255.0f + 0.5f); };
    return b(c.r) << 16 | b(c.g) << 8 | b(c.b);
}

} // namespace atmosphere
