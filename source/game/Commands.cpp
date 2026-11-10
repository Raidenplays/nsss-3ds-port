// NSSS GuiChat.handleCommand, command for command (§ colour codes become plain text here).
#include "Commands.h"

#include "game/IdMap.h"
#include "game/Inventory.h"
#include "game/ItemEntities.h"
#include "game/ItemTable.h"
#include "game/Mobs.h"
#include "game/Player.h"
#include "world/Nbt.h"
#include "world/World.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace commands {
namespace {

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower((unsigned char)ch));
    return s;
}
bool is(const std::string& a, const char* b) { return lower(a) == b; }

// Integer.parseInt; false when it isn't a number (Java throws, the command reports invalid syntax)
bool parseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    size_t i = s[0] == '-' || s[0] == '+' ? 1 : 0;
    if (i == s.size()) return false;
    for (size_t k = i; k < s.size(); ++k)
        if (!std::isdigit((unsigned char)s[k])) return false;
    out = int(std::strtol(s.c_str(), nullptr, 10));
    return true;
}

int idMapGet(std::string name) { // IdMap.getItem
    name = lower(name);
    std::string t;
    for (char ch : name) if (ch != '_' && ch != '-') t += ch;
    for (int i = 0; i < ID_NAME_COUNT; ++i)
        if (t == ID_NAMES[i].name) return ID_NAMES[i].id;
    return -1;
}

int highestBlock(World& w, int x, int z) { // getHighestBlock
    for (int i = 128; i > 0; --i)
        if (w.getBlockId(x, i, z) != 0) return i + 2;
    return 0;
}

void setPosition(Player& p, double x, double y, double z) { // EntityPlayer.setPosition (y is the feet + yOffset)
    p.place(x, y + 1.62, z, p.rotationYaw, p.rotationPitch);
    p.motionX = p.motionY = p.motionZ = 0;
}

bool summon(Context& c, const std::string& name, double x, double y, double z) {
    static const char* const MOBS[][2] = {{"slime", "Slime"}, {"mob", "Mob"}, {"monster", "Monster"}, {"zombie", "Zombie"},
                                          {"creeper", "Creeper"}, {"skeleton", "Skeleton"}, {"sheep", "Sheep"}, {"pig", "Pig"},
                                          {"cow", "Cow"}, {"chicken", "Chicken"}, {"giant", "Giant"}, {"spider", "Spider"}};
    const char* id = nullptr;
    for (auto& m : MOBS)
        if (is(name, m[0])) id = m[1];
    if (!id) return false; // ("player" and "lightning" come with entities and weather)
    nbt::TagPtr t = nbt::makeCompound();
    t->setString("id", id);
    nbt::TagPtr pos = nbt::makeList(nbt::Double);
    pos->items.push_back(nbt::makeDouble(x));
    pos->items.push_back(nbt::makeDouble(y));
    pos->items.push_back(nbt::makeDouble(z));
    t->set("Pos", std::move(pos));
    nbt::TagPtr mo = nbt::makeList(nbt::Double);
    for (int i = 0; i < 3; ++i) mo->items.push_back(nbt::makeDouble(0.0));
    t->set("Motion", std::move(mo));
    nbt::TagPtr ro = nbt::makeList(nbt::Float);
    ro->items.push_back(nbt::makeFloat(float(std::rand() % 360)));
    ro->items.push_back(nbt::makeFloat(0.0f));
    t->set("Rotation", std::move(ro));
    c.mobs.adopt(std::move(t));
    return true;
}

void give(Context& c, int id, int size, bool sizeGiven) {
    const ItemInfo* info = itemInfo(id, 0);
    if (!info) {
        c.say("Invalid item: " + std::to_string(id));
        return;
    }
    if (id >= 256 && !sizeGiven) size = 64; // (Item.maxStackSize; refined with the item port)
    c.say("Giving " + std::to_string(size) + " of item " + std::to_string(id));
    ItemStack s;
    s.id = uint16_t(id);
    s.count = size;
    c.items.spawn(c.player.posX, c.player.posY - 1.62, c.player.posZ, s, 10);
}

} // namespace

void handle(Context& c, const std::string& raw) {
    std::string input = raw;
    while (!input.empty() && (input.back() == ' ' || input.back() == '\n')) input.pop_back();
    if (input.empty()) return;
    if (input[0] != '/') {
        c.say("<Player> " + input);
        return;
    }
    if (c.world.cheatsDisabled) return; // the world was made with Commands: OFF
    std::vector<std::string> a;
    {
        std::stringstream ss(input.substr(1));
        std::string w;
        while (std::getline(ss, w, ' ')) a.push_back(w); // command.split(" ")
    }
    if (a.empty()) return;
    const std::string bad = "Invalid syntax for command: " + input.substr(1);
    World& w = c.world;

    if (is(a[0], "give") || is(a[0], "i")) {
        if (a.size() >= 2 && a.size() <= 3) {
            int id;
            if (!parseInt(a[1], id)) {
                id = idMapGet(a[1]);
                if (id == -1) { c.say("Invalid item: " + a[1]); return; }
            }
            int size = 64;
            if (a.size() == 3 && !parseInt(a[2], size)) { c.say(bad); return; }
            give(c, id, size, a.size() == 3);
        } else if (a.size() < 2) {
            c.say("This requires 2 or more arguments!");
        } else if (a.size() == 4) {
            int id, size, damage;
            if (!parseInt(a[1], id) || !parseInt(a[2], size) || !parseInt(a[3], damage)) { c.say(bad); return; }
            c.say("Giving " + std::to_string(size) + " of item " + std::to_string(id) + " with damage " + std::to_string(damage));
            ItemStack s;
            s.id = uint16_t(id);
            s.count = size;
            s.damage = int16_t(id < 256 ? 0 : damage);
            if (id < 256) c.say("Damage invalid for block, ignoring");
            if (!itemInfo(id, 0)) { c.say("Invalid item: " + std::to_string(id)); return; }
            c.items.spawn(c.player.posX, c.player.posY - 1.62, c.player.posZ, s, 10);
        } else {
            c.say("This command cannot accept more than 3 arguments!");
        }
        return;
    }
    if (a.size() == 2) {
        std::string v = lower(a[1]);
        if (is(a[0], "setwinter")) {
            if (v != "true" && v != "false") { c.say("You cannot set winter to " + v + "!"); return; }
            w.snowCovered = v == "true";
            w.weather = w.snowCovered ? World::PRECIPITATION : World::CLEAR;
            w.season = w.snowCovered ? World::WINTER : World::SUMMER;
            return;
        }
        if (is(a[0], "setseason")) {
            static const char* const S[] = {"summer", "fall", "winter", "spring", "cycle"};
            for (int i = 0; i < 5; ++i) if (v == S[i]) { w.season = i; return; }
            c.say("You cannot set season to " + v + "!");
            return;
        }
        if (is(a[0], "setweather")) {
            if (v == "clear") w.weather = World::CLEAR;
            else if (v == "precipitation") w.weather = World::PRECIPITATION;
            else if (v == "seasonal") w.weather = World::SEASONAL;
            else c.say("You cannot set weather to " + v + "!");
            return;
        }
        if (is(a[0], "summon")) {
            if (!summon(c, a[1], c.player.posX, c.player.posY - 1.62 + 0.5, c.player.posZ))
                c.say("A " + a[1] + " is not a valid entity!");
            return;
        }
        if (is(a[0], "gamemode")) {
            if (v == "survival" || v == "s" || v == "0") {
                c.creative = false;
                c.flying = false;
                c.say("Your game mode has been set to survival.");
            } else if (v == "creative" || v == "c" || v == "1") {
                c.creative = true;
                c.say("Your game mode has been set to creative.");
            } else {
                c.say(a[1] + " is not a valid game mode!");
                return;
            }
            c.inv.creative = c.creative;
            c.player.invulnerable = c.creative;
            return;
        }
    } else if (a.size() == 3) {
        if (is(a[0], "time")) {
            int64_t& t = w.info.time;
            if (is(a[1], "add")) {
                int ticks;
                if (!parseInt(a[2], ticks)) { c.say(bad); return; }
                t += ticks;
            } else if (is(a[1], "set")) {
                if (is(a[2], "day")) t = t + (24000 - t % 24000);
                else if (is(a[2], "night")) t = t + (24000 - t % 13000);
                else {
                    int ticks;
                    if (!parseInt(a[2], ticks)) { c.say(bad); return; }
                    t = ticks;
                }
            } else {
                c.say("Invalid parameter for time command: " + a[1] + ". Use 'add' or 'set'");
            }
            return;
        }
    } else if (a.size() == 4) {
        if (is(a[0], "teleport") || is(a[0], "tp")) {
            int x, y, z;
            if (!parseInt(a[1], x) || !parseInt(a[2], y) || !parseInt(a[3], z)) { c.say(bad); return; }
            y = highestBlock(w, x, z);
            setPosition(c.player, x, y, z);
            return;
        }
    } else if (a.size() == 5) {
        if (is(a[0], "summon")) {
            int x, y, z;
            if (!parseInt(a[2], x) || !parseInt(a[3], y) || !parseInt(a[4], z)) { c.say(bad); return; }
            if (!summon(c, a[1], x, y, z)) c.say("A " + a[1] + " is not a valid entity!");
            return;
        }
    } else if (a.size() == 1) {
        if (is(a[0], "spawn")) {
            int y = highestBlock(w, w.info.spawnX, w.info.spawnZ);
            setPosition(c.player, w.info.spawnX, y, w.info.spawnZ);
            return;
        }
        if (is(a[0], "help")) {
            c.say("---------------Command help (Page 1 of 1)---------------");
            c.say("/teleport/tp <x> <y> <z> - teleport to a location");
            c.say("/give <item> [quantity] - gives requested item");
            c.say("/i <item> [quantity] - alias to give");
            c.say("/time <set/add> <ticks> - changes the time");
            c.say("/setwinter <true/false> - sets the world's climate");
            c.say("/setseason <true/false> - sets the world's season");
            c.say("/setweather <clear/precipitation/seasonal> - sets the world's weather");
            c.say("/summon <entity> - spawns the requested entity");
            c.say("/spawn - sends you to the spawn location");
            c.say("/gamemode <survival/creative> - sets your current gamemode");
            c.say("/noclip - toggles noclip");
            c.say("-----------------------------------------------------");
            return;
        }
        if (is(a[0], "season")) {
            static const char* const S[] = {"summer", "fall", "winter", "spring", "cycle"};
            c.say(std::string("The current season is ") + S[w.season]);
            return;
        }
        if (is(a[0], "time_d")) { c.say("The current day is " + std::to_string(w.info.time / 24000)); return; }
        if (is(a[0], "time_t")) { c.say("The current time is " + std::to_string(w.info.time)); return; }
        if (is(a[0], "noclip")) {
            c.flying = !c.flying; // (noClip itself comes with the player port)
            c.say(std::string("Noclip is now ") + (c.flying ? "on" : "off"));
            return;
        }
    }
    c.say("Invalid command: " + input.substr(1));
}

} // namespace commands
