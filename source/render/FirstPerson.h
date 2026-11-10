#pragma once
// ItemRenderer.renderItemInFirstPerson: the held block or item (or the bare arm) in the bottom
// right corner, swinging when you dig, hit or place, dipping when you change items, bobbing as
// you walk (EntityRenderer.setupViewBobbing).
#include <vector>
#include "model/AxmModel.h"
#include "game/Inventory.h"

class Renderer;
class Player;
class World;
struct Camera;

class FirstPerson {
public:
    // ItemRenderer.updateEquippedItem (once a tick)
    void tick(const Inventory& inv, bool blocking = false);
    void swingOffhand(); // AxeolonBattleInput.offhandSwing
    // After the world, before the 2D overlay. bright: World.getLightBrightness where the player is.
    void draw(Renderer& r, const Player& p, const Camera& cam, float partial, float bright);

private:
    ItemStack shown;       // itemToRender
    int shownSlot = -1;    // field_20099_f
    float equipped = 0, prevEquipped = 0;
    std::vector<axm::ModelVertex> tris;
    ItemStack offShown;
    int offSwing = 0;
    float raise_ = 0, prevRaise = 0; // the shield coming up
    void drawOffhand(Renderer& r, const Player& p, const float baseM[16], float partial, float bright);
    void addQuad(const float m[16], const float p[4][3], const float uv[4][2], float shade);
};
