#include "Options.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

Options gOptions;

namespace {
const char* PATH = "sdmc:/3ds/nsss3ds/options.txt";
}

void Options::load() {
    FILE* f = std::fopen(PATH, "r");
    if (!f) return;
    char line[128];
    while (std::fgets(line, sizeof line, f)) {
        char* c = std::strchr(line, ':');
        if (!c) continue;
        *c = 0;
        const char* k = line;
        const char* v = c + 1;
        auto isTrue = [&]() { return std::strncmp(v, "true", 4) == 0; };
        if (!std::strcmp(k, "music")) music = float(std::atof(v));
        else if (!std::strcmp(k, "sound")) sound = float(std::atof(v));
        else if (!std::strcmp(k, "invertYMouse")) invertY = isTrue();
        else if (!std::strcmp(k, "mouseSensitivity")) sensitivity = float(std::atof(v));
        else if (!std::strcmp(k, "bobView")) bobView = isTrue();
        else if (!std::strcmp(k, "difficulty")) difficulty = std::atoi(v) & 3;
        else if (!std::strcmp(k, "renderDistance")) renderDistance = std::atoi(v);
        else if (!std::strcmp(k, "showStats")) showStats = isTrue();
        else if (!std::strcmp(k, "dynamicLights")) dynamicLights = std::atoi(v) % 3;
        else if (!std::strcmp(k, "craftStyle")) craftStyle = std::atoi(v) & 1;
    }
    std::fclose(f);
    auto clamp01 = [](float& x) { x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; };
    clamp01(music);
    clamp01(sound);
    clamp01(sensitivity);
}

void Options::save() const {
    FILE* f = std::fopen(PATH, "w");
    if (!f) return;
    std::fprintf(f, "music:%g\nsound:%g\ninvertYMouse:%s\nmouseSensitivity:%g\nbobView:%s\ndifficulty:%d\n", music, sound,
                 invertY ? "true" : "false", sensitivity, bobView ? "true" : "false", difficulty);
    std::fprintf(f, "renderDistance:%d\nshowStats:%s\ndynamicLights:%d\ncraftStyle:%d\n", renderDistance, showStats ? "true" : "false",
                 dynamicLights, craftStyle);
    std::fclose(f);
}

float Options::lookScale() const {
    float f = sensitivity * 0.6f + 0.2f;
    float k = f * f * f * 8.0f;
    const float mid = 0.5f * 0.6f + 0.2f;
    return k / (mid * mid * mid * 8.0f);
}

const char* Options::difficultyName(int d) {
    static const char* const N[4] = {"Peaceful", "Easy", "Normal", "Hard"};
    return N[d & 3];
}
