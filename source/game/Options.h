#pragma once
// GameSettings: the player's options, kept in sdmc:/3ds/axeolon3ds/options.txt in the Java game's
// "key:value" lines (music, sound, invertYMouse, mouseSensitivity, bobView, difficulty...).
#include <string>

struct Options {
    float music = 1.0f, sound = 1.0f;
    bool invertY = false;            // invertYMouse
    float sensitivity = 0.5f;        // mouseSensitivity: 0.5 is the normal look speed
    bool bobView = true;
    int difficulty = 2;              // 0 peaceful, 1 easy, 2 normal, 3 hard
    int renderDistance = 0;          // blocks the renderer draws (0: leave as it is)
    bool showStats = false;          // fps and memory on the Menu tab
    int dynamicLights = 0;           // AxeolonVideo.dynamicLights: 0 off, 1 fast, 2 fancy
    int craftStyle = 0;              // 0: the Java crafting grid, 1: Pocket Edition's recipe list (MATTIS)

    void load();
    void save() const;
    // what sensitivity does to the look speed (GameSettings: mouseSensitivity * 0.6 + 0.2, cubed, x 8,
    // scaled so 0.5 is 1.0)
    float lookScale() const;
    static const char* difficultyName(int d);
};

extern Options gOptions;
