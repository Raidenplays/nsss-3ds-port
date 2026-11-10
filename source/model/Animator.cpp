#include "Animator.h"
#include <algorithm>
#include <cmath>

namespace {
float timeIn(const axm::Animation& a, float seconds) {
    if (a.length <= 0.0f) return 0.0f;
    if (a.loop == 1) return std::fmod(seconds, a.length);
    if (seconds > a.length) return a.loop == 2 ? a.length : -1.0f;
    return seconds;
}
float smooth(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return x * x * (3.0f - 2.0f * x);
}
} // namespace

Animator::Track* Animator::current() {
    for (int i = int(tracks.size()) - 1; i >= 0; --i)
        if (tracks[i].fadeOutStart < 0.0f && !tracks[i].overlay) return &tracks[i];
    return nullptr;
}

void Animator::play(const std::string& name, int fade) {
    Track* cur = current();
    if (cur && cur->name == name && cur->fadeOutStart < 0.0f) return;
    for (Track& t : tracks)
        if (t.fadeOutStart < 0.0f && !t.overlay) {
            t.fadeOutStart = now;
            t.fadeOut = float(std::max(1, fade));
        }
    Track t;
    t.name = name;
    t.start = now;
    t.fadeIn = float(std::max(1, fade));
    tracks.push_back(t);
}

void Animator::restart(const std::string& name, int fade) {
    Track* cur = current();
    if (cur && cur->name == name) {
        cur->fadeOutStart = now;
        cur->fadeOut = 1.0f;
    }
    play(name, fade);
}

void Animator::overlay(const std::string& name, int fade) {
    for (Track& t : tracks)
        if (t.overlay && t.name == name && t.fadeOutStart < 0.0f) {
            t.fadeOutStart = now;
            t.fadeOut = 2.0f;
        }
    Track t;
    t.name = name;
    t.start = now;
    t.fadeIn = float(std::max(1, fade));
    t.overlay = true;
    tracks.push_back(t);
}

bool Animator::hasOverlay() const {
    for (const Track& t : tracks)
        if (t.overlay && t.fadeOutStart < 0.0f) return true;
    return false;
}

void Animator::stop(int fade) {
    for (Track& t : tracks)
        if (t.fadeOutStart < 0.0f) {
            t.fadeOutStart = now;
            t.fadeOut = float(std::max(1, fade));
        }
}

const std::string* Animator::playing() const {
    for (int i = int(tracks.size()) - 1; i >= 0; --i)
        if (tracks[i].fadeOutStart < 0.0f && !tracks[i].overlay) return &tracks[i].name;
    return nullptr;
}

void Animator::tick() {
    now += 1.0f;
    for (int i = int(tracks.size()) - 1; i >= 0; --i) {
        const Track& t = tracks[i];
        if (t.fadeOutStart >= 0.0f && now - t.fadeOutStart > t.fadeOut) tracks.erase(tracks.begin() + i);
    }
}

void Animator::applyTo(axm::Model& model, float swing, float amount, float partial) {
    float t = now + partial;
    for (Track& tr : tracks) {
        const axm::Animation* a = model.animation(tr.name);
        if (!a) {
            if (tr.overlay && tr.fadeOutStart < 0.0f) { tr.fadeOutStart = now; tr.fadeOut = 1.0f; } // not in this model
            continue;
        }
        float w = std::min(1.0f, (t - tr.start) / tr.fadeIn);
        if (tr.fadeOutStart >= 0.0f) w = std::min(w, std::max(0.0f, 1.0f - (t - tr.fadeOutStart) / tr.fadeOut));
        float raw = (t - tr.start) / 20.0f;
        float sec = timeIn(*a, raw);
        if (tr.overlay) {
            // overlays fade out over their last 4 ticks and then end
            if (a->loop != 1 && raw > a->length) {
                if (tr.fadeOutStart < 0.0f) { tr.fadeOutStart = now; tr.fadeOut = 1.0f; }
                continue;
            }
            if (a->loop != 1) w = std::min(w, std::max(0.0f, (a->length - raw) * 20.0f / 4.0f));
        }
        if (sec < 0.0f) continue; // a finished "once" animation goes back to rest
        w = smooth(w);
        model.apply(a, sec, w);
    }
    if (walkWeight > 0.0f && amount > 0.01f) {
        if (const axm::Animation* walk = model.animation("walk")) {
            float sec = timeIn(*walk, swing / 9.4316f * strideScale * walk->length);
            if (sec >= 0.0f) model.apply(walk, sec, std::min(1.0f, amount * 1.6f) * walkWeight);
        }
    }
}
