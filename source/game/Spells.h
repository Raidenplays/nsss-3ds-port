#pragma once
// Axeolon spells (Spell, ItemSpellBook, ItemSpellPage, SpellBookInput).
// A spell book (item 1001) keeps its spells in its item data:
//   "Spells" = byte array of spell ids, in the order they were learned
//   "Uses"   = byte array of uses left for each (255 = never runs out)
//   "Selected" = which one is selected
// A spell page (item 1002, damage = spell id, "Eternal" tag from boss chests) writes into the book.
#include <string>
#include <vector>
#include <cstdint>

struct ItemStack;
class Inventory;
class World;
class Player;
class Mobs;

namespace spells {

constexpr int BOOK = 1001, PAGE = 1002;
constexpr int PERMANENT = 255, MAX_USES = 250;
constexpr int COUNT = 6;

struct Spell { const char* key; const char* name; int mana, lootWeight, usesPerPage; };
extern const Spell SPELLS[COUNT];
inline const Spell* get(int id) { return id >= 0 && id < COUNT ? &SPELLS[id] : nullptr; }
inline int iconOf(int id) { return 224 + id; }

std::vector<uint8_t> spellsIn(const ItemStack& book);
int usesOf(const ItemStack& book, int index);
int selectedIndex(const ItemStack& book);
void setSelected(ItemStack& book, int index);

// ItemSpellPage.onItemRightClick: returns the chat line (page used up when `used`)
std::string usePage(Inventory& inv, ItemStack& page, bool& used);
// ItemSpellBook.castSelected. `fizzled` = play random.fizz. Chat line (if any) goes into msg.
// True if a spell went off (the "Spark of Magic" achievement).
bool castSelected(ItemStack& book, World& w, Player& p, Mobs& mobs, bool creative, bool editWorld, std::string& msg);

}
