#pragma once
// Free-fly camera for now. Phase 5 replaces this with a ported EntityPlayer +
// collision; the camera then just follows the player's eyes.
#include <cmath>
#if defined(__3DS__) || defined(__ANDROID__)
#include <3ds.h>
#include <citro3d.h>
#endif

struct Camera {
    float x = 0, y = 70, z = 0;
    float yaw = 0;    // radians, 0 = looking toward -Z, positive turns right
    float pitch = 0;  // radians, positive looks down
    float speed = 6.0f; // blocks per second
    bool fast = false;
    // EntityRenderer.hurtCameraEffect (radians): a roll toward the hit, and falling over when dead
    float hurtRoll = 0, hurtYaw = 0, deathRoll = 0;
    // EntityRenderer.setupViewBobbing (radians / blocks): sway and dip while walking
    float bobX = 0, bobY = 0, bobRoll = 0, bobPitch = 0;

    void forward(float& fx, float& fy, float& fz) const {
        float cp = std::cos(pitch);
        fx = std::sin(yaw) * cp;
        fy = -std::sin(pitch);
        fz = -std::cos(yaw) * cp;
    }

    // lookX/lookY in -1..1, moveX/moveZ in -1..1 (moveZ > 0 = forward)
    void update(float lookX, float lookY, float moveX, float moveZ, float moveY, float dt) {
        const float lookSpeed = 2.4f; // radians/sec at full deflection
        yaw += lookX * lookSpeed * dt;
        pitch -= lookY * lookSpeed * dt;
        const float lim = 1.55f; // ~89 degrees
        if (pitch > lim) pitch = lim;
        if (pitch < -lim) pitch = -lim;

        // Horizontal movement like Beta's flying (no pitch component)
        float fx = std::sin(yaw), fz = -std::cos(yaw); // forward
        float rx = std::cos(yaw), rz = std::sin(yaw);  // right
        float s = speed * dt * (fast ? 3.0f : 1.0f);
        x += (fx * moveZ + rx * moveX) * s;
        z += (fz * moveZ + rz * moveX) * s;
        y += moveY * s;
    }

#if defined(__3DS__) || defined(__ANDROID__)
    void viewMatrix(C3D_Mtx& out) const {
        Mtx_Identity(&out);
        if (deathRoll != 0.0f) Mtx_RotateZ(&out, deathRoll, true);
        if (hurtRoll != 0.0f) {
            Mtx_RotateY(&out, -hurtYaw, true);
            Mtx_RotateZ(&out, -hurtRoll, true);
            Mtx_RotateY(&out, hurtYaw, true);
        }
        if (bobX != 0.0f || bobY != 0.0f || bobRoll != 0.0f || bobPitch != 0.0f) {
            Mtx_Translate(&out, bobX, bobY, 0.0f, true);
            Mtx_RotateZ(&out, bobRoll, true);
            Mtx_RotateX(&out, bobPitch, true);
        }
        Mtx_RotateX(&out, pitch, true);
        Mtx_RotateY(&out, yaw, true);
        Mtx_Translate(&out, -x, -y, -z, true);
    }
#endif
};
