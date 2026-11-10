// CommandHandler.java, line for line where this version has the pieces (the debug-only commands -
// /portal, /biome, /mana - are off in the mod's release builds, so they're unknown here too)
#include "Commands.h"

#include "audio/Sound.h"
#include "game/Inventory.h"
#include "game/ItemEntities.h"
#include "game/ItemTable.h"
#include "game/Mobs.h"
#include "game/Player.h"
#include "game/Progress.h"
#include "world/Bosses.h"
#include "world/Nbt.h"
#include "world/World.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace commands {
namespace {

bool gBad = false; // a number didn't parse (no exceptions on the 3DS): the command says so and stops

std::string lower(std::string s) {
    for (char& ch : s) ch = char(std::tolower((unsigned char)ch));
    return s;
}
bool same(const std::string& a, const char* b) { return lower(a) == lower(b); }

long long parseLong(const std::string& s) {
    if (s.empty()) { gBad = true; return 0; }
    size_t i = s[0] == '-' || s[0] == '+' ? 1 : 0;
    if (i == s.size()) { gBad = true; return 0; }
    for (size_t k = i; k < s.size(); ++k)
        if (!std::isdigit((unsigned char)s[k])) { gBad = true; return 0; }
    return std::strtoll(s.c_str(), nullptr, 10);
}
int parseInt(const std::string& s) { return int(parseLong(s)); }
double parseDouble(const std::string& s) {
    char* end = nullptr;
    double v = std::strtod(s.c_str(), &end);
    if (s.empty() || !end || *end) { gBad = true; return 0; }
    return v;
}

// "~" = where you are, "~5" = five past it
double coordinate(const std::string& text, double current) {
    if (!text.empty() && text[0] == '~') return current + (text.size() > 1 ? parseDouble(text.substr(1)) : 0.0);
    return parseDouble(text);
}

void help(Context& c) {
    c.say("Commands:");
    c.say("/gamemode <survival|creative>");
    c.say("/give <id> [amount] [damage]");
    c.say("/tp <x> <y> <z>  (~ = your position)");
    c.say("/time <set|add> <number|day|night>");
    c.say("/achievement <list | grant <id|all> | revoke <id|all>>");
    c.say("/music <name | stop>  (e.g. /music mechanolith)");
    c.say("/boss spawn tabish | lair | reset <id> | status <id>");
    c.say("/save-all");
}

void gamemode(Context& c, const std::vector<std::string>& a) {
    if (a.size() < 2) {
        c.say("Usage: /gamemode <survival|creative>");
        return;
    }
    std::string mode = lower(a[1]);
    int type;
    if (mode == "survival" || mode == "s" || mode == "0") type = 0;
    else if (mode == "creative" || mode == "c" || mode == "1") type = 1;
    else {
        c.say("Unknown game mode: " + a[1]);
        return;
    }
    c.creative = type == 1;
    c.inv.creative = c.creative;
    c.player.invulnerable = c.creative;
    if (!c.creative) c.flying = false;
    c.world.info.gameType = type;
    c.say(std::string("Game mode set to ") + (type == 1 ? "Creative" : "Survival") + ".");
}

void give(Context& c, const std::vector<std::string>& a) {
    if (a.size() < 2) {
        c.say("Usage: /give <id> [amount] [damage]");
        return;
    }
    int id = parseInt(a[1]);
    int amount = a.size() > 2 ? parseInt(a[2]) : 1;
    int damage = a.size() > 3 ? parseInt(a[3]) : 0;
    if (gBad) return;
    if (id <= 0 || id >= 32000 || !itemInfo(id, 0)) {
        c.say("There's no item with ID " + std::to_string(id) + ".");
        return;
    }
    ItemStack s;
    s.id = uint16_t(id);
    s.count = amount;
    s.damage = int16_t(damage);
    if (!c.inv.addStack(s) && s.count > 0) // dropPlayerItem: in front of you
        c.items.spawn(c.player.posX, c.player.posY - 0.3, c.player.posZ, s, 40);
    c.say("Gave " + std::to_string(amount) + " of item " + std::to_string(id) + ".");
}

void teleport(Context& c, const std::vector<std::string>& a) {
    if (a.size() < 4) {
        c.say("Usage: /tp <x> <y> <z>");
        return;
    }
    const double eye = 1.62; // yOffset
    double x = coordinate(a[1], c.player.posX);
    double y = coordinate(a[2], c.player.posY - eye);
    double z = coordinate(a[3], c.player.posZ);
    if (gBad) return;
    c.player.place(x, y + eye, z, c.player.rotationYaw, c.player.rotationPitch);
    c.player.motionX = c.player.motionY = c.player.motionZ = 0;
    c.player.fallDistance = 0;
    c.say("Teleported to " + std::to_string(int(x)) + ", " + std::to_string(int(y)) + ", " + std::to_string(int(z)) + ".");
}

void time(Context& c, const std::vector<std::string>& a) {
    if (a.size() < 3) {
        c.say("Usage: /time <set|add> <number|day|night>");
        return;
    }
    long long v;
    if (same(a[2], "day")) v = 1000;
    else if (same(a[2], "night")) v = 13000;
    else v = parseLong(a[2]);
    if (gBad) return;
    if (same(a[1], "set")) {
        c.world.info.time = v;
        c.say("Set the time to " + std::to_string(v) + ".");
    } else if (same(a[1], "add")) {
        c.world.info.time += v;
        c.say("Added " + std::to_string(v) + " to the time.");
    } else {
        c.say("Usage: /time <set|add> <number|day|night>");
    }
}

bool revoke(Inventory& inv, const std::string& id) {
    std::vector<std::string> keep;
    std::stringstream ss(inv.achievements);
    std::string t;
    bool gone = false;
    while (std::getline(ss, t, ',')) {
        if (t == id) gone = true;
        else if (!t.empty()) keep.push_back(t);
    }
    inv.achievements.clear();
    for (size_t i = 0; i < keep.size(); ++i) inv.achievements += (i ? "," : "") + keep[i];
    return gone;
}

void achievement(Context& c, const std::vector<std::string>& a) {
    std::string sub = a.size() > 1 ? lower(a[1]) : "list";
    if (sub == "list") {
        int n = 0;
        for (int i = 0; i < progress::COUNT; ++i) n += progress::has(c.inv, progress::ALL[i].id);
        c.say("Achievements (" + std::to_string(n) + " of " + std::to_string(progress::COUNT) + "):");
        for (int i = 0; i < progress::COUNT; ++i) {
            const progress::Achievement& ac = progress::ALL[i];
            c.say(std::string(progress::has(c.inv, ac.id) ? "[x] " : "[ ] ") + ac.id + " - " + ac.name);
        }
        return;
    }
    if (sub != "grant" && sub != "revoke") {
        c.say("Usage: /achievement <list | grant <id|all> | revoke <id|all>>");
        return;
    }
    if (a.size() < 3) {
        c.say("Which one? Type /achievement list for the IDs, or use all.");
        return;
    }
    bool grant = sub == "grant", all = same(a[2], "all"), known = false;
    int changed = 0;
    for (int i = 0; i < progress::COUNT; ++i) {
        const progress::Achievement& ac = progress::ALL[i];
        if (!all && !same(a[2], ac.id)) continue;
        known = true;
        if (grant && !progress::has(c.inv, ac.id)) {
            progress::grant(c.inv, ac.id);
            ++changed;
        } else if (!grant && progress::has(c.inv, ac.id)) {
            revoke(c.inv, ac.id);
            ++changed;
        }
    }
    if (all) progress::clearToasts(); // no 12 toasts in a row
    if (changed == 0) c.say(known ? "Nothing to change." : "No achievement called " + a[2] + ". Type /achievement list.");
    else c.say(std::string(grant ? "Granted " : "Revoked ") + std::to_string(changed) + " achievement" + (changed == 1 ? "." : "s."));
}

void boss(Context& c, const std::vector<std::string>& a) {
    std::string sub = a.size() > 1 ? lower(a[1]) : "";
    if (sub == "spawn" && a.size() > 2 && same(a[2], "tabish")) {
        // asleep 12 blocks ahead, arena radius 22 (EntityTabish + setArena)
        float yaw = c.player.rotationYaw * 3.14159265f / 180.0f;
        double bx = c.player.posX - std::sin(yaw) * 12.0, bz = c.player.posZ + std::cos(yaw) * 12.0;
        double by = c.player.posY - 1.62;
        nbt::TagPtr t = nbt::makeCompound();
        t->setString("id", "Tabish");
        nbt::TagPtr pos = nbt::makeList(nbt::Double);
        pos->items.push_back(nbt::makeDouble(bx));
        pos->items.push_back(nbt::makeDouble(by));
        pos->items.push_back(nbt::makeDouble(bz));
        t->set("Pos", std::move(pos));
        nbt::TagPtr mo = nbt::makeList(nbt::Double);
        for (int i = 0; i < 3; ++i) mo->items.push_back(nbt::makeDouble(0.0));
        t->set("Motion", std::move(mo));
        nbt::TagPtr ro = nbt::makeList(nbt::Float);
        ro->items.push_back(nbt::makeFloat(c.player.rotationYaw + 90.0f));
        ro->items.push_back(nbt::makeFloat(0.0f));
        t->set("Rotation", std::move(ro));
        t->setShort("Health", 200);
        t->setInt("ArenaX", int(std::floor(bx)));
        t->setInt("ArenaY", int(std::floor(by)));
        t->setInt("ArenaZ", int(std::floor(bz)));
        t->setInt("ArenaR", 22);
        c.mobs.adopt(std::move(t));
        c.say("Tabish is asleep 12 blocks ahead (arena radius 22). Switch to survival and walk up to her.");
    } else if (sub == "spawn") {
        c.say("The Test Brute isn't in this version. Try /boss spawn tabish.");
    } else if (sub == "lair") {
        if (!bosses::hasInt("lair.x")) {
            c.say("Tabish's lair hasn't been picked yet - explore a little first.");
            return;
        }
        std::string y = bosses::hasInt("lair.floor") ? ", floor at y " + std::to_string(bosses::getInt("lair.floor")) : "";
        c.say("Tabish's lair: x " + std::to_string(bosses::getInt("lair.x")) + ", z " + std::to_string(bosses::getInt("lair.z")) + y +
              (bosses::hasSpawned("structure.ice_cave") ? " (built)" : " (not built yet - go near it)") + ".");
    } else if (sub == "reset" && a.size() > 2) {
        bosses::reset(a[2]);
        c.say("Reset the boss record for " + a[2] + ".");
    } else if (sub == "status" && a.size() > 2) {
        c.say(a[2] + ": spawned " + (bosses::hasSpawned(a[2]) ? "true" : "false") + ", defeated " +
              (bosses::isDefeated(a[2]) ? "true" : "false"));
    } else {
        c.say("/boss spawn tabish | lair | reset <id> | status <id>");
    }
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
    std::vector<std::string> a;
    {
        std::stringstream ss(input.substr(1));
        std::string w;
        while (ss >> w) a.push_back(w);
    }
    if (a.empty()) {
        c.say("Unknown command. Type /help for a list.");
        return;
    }
    std::string cmd = lower(a[0]);
    gBad = false;
    {
        if (cmd == "help") help(c);
        else if (cmd == "gamemode") gamemode(c, a);
        else if (cmd == "give") give(c, a);
        else if (cmd == "tp") teleport(c, a);
        else if (cmd == "time") time(c, a);
        else if (cmd == "music") {
            if (a.size() < 2 || same(a[1], "stop")) {
                sound::stopMusic();
                c.say("Stopped the music.");
            } else {
                sound::playMusic(a[1]);
                c.say("Playing " + a[1] + ". Type /music stop to stop it.");
            }
        } else if (cmd == "boss") boss(c, a);
        else if (cmd == "achievement" || cmd == "ach") achievement(c, a);
        else if (cmd == "save-all") {
            c.saveNow = true;
            c.say("Saved the world.");
        } else if (cmd == "season" || cmd == "structure") {
            c.say("/" + cmd + " isn't in this version yet.");
        } else c.say("Unknown command. Type /help for a list.");
    }
    if (gBad) c.say("That's not a valid number.");
}

} // namespace commands
