#include "Progress.h"
#include <deque>
#include "Inventory.h"

namespace progress {

const Achievement ALL[] = {
    {NEW_PAGE, "A New Page", "Open the Almanac", nullptr, 1006, 0, 60, nullptr},
    {NIGHT_WATCH, "Night Watch", "Survive your first night", NEW_PAGE, 347, 46, 15, nullptr},
    {THIEF, "Thief in the Night", "Catch a raccoon raiding a chest", NIGHT_WATCH, 319, 92, 0, nullptr},
    {OUT_COLD, "Out Cold", "Hit a ghoul with a snowball", NIGHT_WATCH, 332, 92, 30, nullptr},
    {SPARK, "Spark of Magic", "Cast your first spell", NEW_PAGE, 1001, 0, 110, nullptr},
    {PACK_HUNTER, "Pack Hunter", "Kill a wolf", NEW_PAGE, 334, 46, 60, "Wolf Charm recipe"},
    {LEADER, "Leader of the Pack", "Tame a wolf", PACK_HUNTER, 352, 92, 60, nullptr},
    {SADDLE_UP, "Saddle Up", "Ride a wolf", LEADER, 329, 138, 38, nullptr},
    {BONE_HOUND, "Bone Hound", "Your wolf kills a skeleton", LEADER, 262, 138, 82, nullptr},
    {READY_COLD, "Ready for the Cold", "Wear a full set of iron armour", NEW_PAGE, 307, 46, 110, "A new page in The Hunt"},
    {SNOW, "Where the Snow Never Melts", "Enter the bear's cave", READY_COLD, 80, 92, 110, nullptr},
    {WAKE, "Wake the Beast", "Wake the bear", SNOW, 332, 138, 125, nullptr},
    {KING, "King of the Cold", "Kill the bear", WAKE, 79, 184, 125, "Bear and Spawn Charm recipes"},
};
const int COUNT = int(sizeof(ALL) / sizeof(ALL[0]));

namespace {
std::deque<Toast> queue;
}

const Achievement* get(const std::string& id) {
    for (int i = 0; i < COUNT; ++i)
        if (id == ALL[i].id) return &ALL[i];
    return nullptr;
}

bool has(const Inventory& inv, const std::string& id) {
    const std::string& s = inv.achievements;
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find(',', p);
        if (e == std::string::npos) e = s.size();
        if (s.compare(p, e - p, id) == 0 && e - p == id.size()) return true;
        p = e + 1;
    }
    return false;
}

bool visible(const Inventory& inv, const Achievement& a) {
    return has(inv, a.id) || !a.parent || has(inv, a.parent);
}

bool isRare(const std::string& id) { return id == KING; }

bool grant(Inventory& inv, const std::string& id) {
    const Achievement* a = get(id);
    if (!a || has(inv, id)) return false;
    if (!inv.achievements.empty()) inv.achievements += ',';
    inv.achievements += id;
    Toast t{"Achievement get!", 0xFFFF55, a->name, a->icon, isRare(id)};
    if (t.rare) {
        t.title = "Challenge complete!";
        t.colour = 0xFF55FF;
    }
    queue.push_back(t);
    if (a->unlocks) queue.push_back(Toast{"", 0, "Check the Almanac to see what you unlocked", 1006, false});
    return true;
}

bool popToast(Toast& out) {
    if (queue.empty()) return false;
    out = queue.front();
    queue.pop_front();
    return true;
}

void clearToasts() { queue.clear(); }

} // namespace progress
