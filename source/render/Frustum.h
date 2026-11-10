#pragma once
// View-frustum culling for 16^3 sections (replaces Beta's Frustrum /
// ClippingHelperImplementation). Planes are built straight from the camera
// angles, so no matrix maths per frame.
#include <cmath>

struct Frustum {
    float px, py, pz;
    float n[4][3]; // inward plane normals (left, right, top, bottom)

    void build(float camX, float camY, float camZ, float yaw, float pitch, float fovYRad, float aspect) {
        px = camX; py = camY; pz = camZ;
        const float cy = std::cos(yaw), sy = std::sin(yaw);
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float f[3] = {sy * cp, -sp, -cy * cp};   // forward
        const float r[3] = {cy, 0.0f, sy};             // right
        const float u[3] = {r[1] * f[2] - r[2] * f[1], // up = right x forward
                            r[2] * f[0] - r[0] * f[2],
                            r[0] * f[1] - r[1] * f[0]};
        const float hv = fovYRad * 0.5f;
        const float hh = std::atan(std::tan(hv) * aspect);
        const float ch = std::cos(hh), sh = std::sin(hh), cv = std::cos(hv), sv = std::sin(hv);
        for (int i = 0; i < 3; i++) {
            n[0][i] = r[i] * ch + f[i] * sh;  // left
            n[1][i] = -r[i] * ch + f[i] * sh; // right
            n[2][i] = -u[i] * cv + f[i] * sv; // top
            n[3][i] = u[i] * cv + f[i] * sv;  // bottom
        }
    }

    bool sphereVisible(float x, float y, float z, float radius) const {
        const float dx = x - px, dy = y - py, dz = z - pz;
        for (int p = 0; p < 4; p++)
            if (n[p][0] * dx + n[p][1] * dy + n[p][2] * dz < -radius) return false;
        return true;
    }
};
