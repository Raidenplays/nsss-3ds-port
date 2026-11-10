#pragma once
// AxeolonAchievements / AxeolonProgress: the achievements, kept as the player's "AxAchievements"
// string (comma-separated ids, as Java saves them) in Inventory::achievements.
#include <string>

class Inventory;

namespace progress {

struct Achievement {
    const char* id; const char* name; const char* description; const char* parent;
    int icon; int x, y; const char* unlocks;
};
extern const Achievement ALL[];
extern const int COUNT;

constexpr const char* NEW_PAGE = "newPage", *NIGHT_WATCH = "nightWatch", *THIEF = "thiefInTheNight",
                      *OUT_COLD = "outCold", *SPARK = "sparkOfMagic", *PACK_HUNTER = "packHunter",
                      *LEADER = "leaderOfThePack", *SADDLE_UP = "saddleUp", *BONE_HOUND = "boneHound",
                      *READY_COLD = "readyForTheCold", *SNOW = "whereTheSnowNeverMelts", *WAKE = "wakeTheBeast",
                      *KING = "kingOfTheCold";

const Achievement* get(const std::string& id);
bool has(const Inventory& inv, const std::string& id);
bool visible(const Inventory& inv, const Achievement& a); // earned, or its parent is
bool isRare(const std::string& id);                       // boss kills: "Challenge complete!"

// AxeolonProgress.grant: adds it and queues the toast(s). False if already earned.
bool grant(Inventory& inv, const std::string& id);

// AxeolonToasts: what's waiting to slide in
struct Toast { std::string title; unsigned colour; std::string text; int icon; bool rare; };
bool popToast(Toast& out);
void clearToasts();

} // namespace progress
