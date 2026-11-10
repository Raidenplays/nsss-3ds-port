#pragma once
// AxeolonAnimator: what an Axeolon mob is playing. One main animation at a time, crossfading
// into the next, overlays (hurt, attack, fidgets) added on top, plus the walk cycle, which runs
// by distance walked and fades in with walking speed. Missing animation names are ignored.
#include <string>
#include <vector>
#include "AxmModel.h"

class Animator {
public:
    float walkWeight = 1.0f; // 0 stops the walk cycle (sleeping, cutscenes)
    float lookWeight = 1.0f; // how much the head follows where the mob looks
    float strideScale = 1.0f;

    // AxeolonMobAnims bookkeeping
    int fidgetCooldown = 100;
    int lastHurtTime = 0, lastAttackTime = 0, lastCounter = 0;
    bool dancing = false;

    void play(const std::string& name, int fade = 6);
    void restart(const std::string& name, int fade);
    void overlay(const std::string& name, int fade);
    bool hasOverlay() const;
    void stop(int fade);
    const std::string* playing() const;
    void tick();
    // Pose the model (after resetPose): every track, then the walk cycle.
    void applyTo(axm::Model& model, float swing, float amount, float partial);

private:
    struct Track {
        std::string name;
        float start = 0, fadeIn = 1, fadeOutStart = -1, fadeOut = 1;
        bool overlay = false;
    };
    std::vector<Track> tracks;
    float now = 0;
    Track* current();
};
