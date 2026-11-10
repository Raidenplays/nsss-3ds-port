#pragma once
// NSSS's chat commands (GuiChat.handleCommand), when the world allows them (cheatsDisabled off).
// Plain text is said as "<Player> text"; a leading slash runs a command. Lines go back through say().
#include <functional>
#include <string>

class World;
class Player;
class Inventory;
class ItemEntities;
class Mobs;

namespace commands {

struct Context {
    World& world;
    Player& player;
    Inventory& inv;
    ItemEntities& items;
    Mobs& mobs;
    bool& creative;
    bool& flying;
    bool& saveNow;
    std::function<void(const std::string&)> say;
};

void handle(Context& c, const std::string& input);

} // namespace commands
