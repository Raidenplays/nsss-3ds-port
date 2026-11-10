#pragma once
// The menus, laid out like Minecraft: Pocket Edition 0.8 (touch screens in GUI units, buttons
// cut from gui/spritesheet.png), with Axeolon's own logo, panorama and splashes.
#include <memory>
#include <string>
#include <vector>

namespace ui {

struct Touch {
    float x = 0, y = 0;   // GUI units
    bool down = false;    // finger on the screen
    bool pressed = false; // went down this frame
    bool released = false;
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual void layout() {}
    virtual void tick(float dt) { (void)dt; }
    virtual void render(const Touch& t) = 0;
    virtual void touch(const Touch& t) = 0;
    virtual bool back() { return false; } // the Android back button; true = handled
};

// Touch::TButton: a nine-patch button that darkens while held
struct Button {
    int id = 0;
    float x = 0, y = 0, w = 0, h = 0;
    std::string label;
    bool held = false;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    void render(const Touch& t);
};

void setScreen(std::unique_ptr<Screen> s);
Screen* current();
std::unique_ptr<Screen> makeTitle();
std::unique_ptr<Screen> makeSelectWorld();
std::unique_ptr<Screen> makeBuy();

void tick(float dt);
void frame(const Touch& t);

// what the menus decided (a world to play, or quitting)
struct Choice {
    std::string world;
    bool created = false;
    int gameType = 0;
    bool quit = false;
};
void setChoice(const Choice* c); // null: nothing decided yet
bool choiceMade();
const Choice& choice();

} // namespace ui
