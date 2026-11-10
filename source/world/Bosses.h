#pragma once
// AxeolonBosses: the world's boss record (<world>/axeolon_bosses.dat, gzipped NBT): each boss
// spawns once and is beaten once per world, and a few numbers kept with it (where Tabish's lair is).
#include <string>

namespace bosses {

void open(const std::string& worldDir); // "" = no saving (the built-in test world)
bool hasSpawned(const std::string& id);
void markSpawned(const std::string& id);
bool isDefeated(const std::string& id);
void markDefeated(const std::string& id);
void reset(const std::string& id); // /boss reset: neither spawned nor defeated
bool hasInt(const std::string& key);
int getInt(const std::string& key);
void setInt(const std::string& key, int v);

} // namespace bosses
