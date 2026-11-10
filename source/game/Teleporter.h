#pragma once
// Teleporter: where a player coming through a Nether portal ends up - the nearest portal within
// 128 blocks of where they arrive, or a new one built next to them (with an obsidian ledge if
// it has to hang in the air).
class World;
class Player;

namespace teleporter {
void place(World& w, Player& p); // func_4107_a
} // namespace teleporter
