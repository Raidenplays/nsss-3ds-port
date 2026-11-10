#pragma once
// NSSS GuiBuild's palette (Session.creativeInventory, then Session.creativeInventoryItems), in the
// jar's order: 45 to a page, blocks first.
namespace buildmenu {
extern const short BLOCKS[];
extern const int BLOCK_COUNT;
extern const short ITEMS[];
extern const int ITEM_COUNT;
constexpr int PER_PAGE = 45, MAX_TIER = 5;
inline int total() { return BLOCK_COUNT + ITEM_COUNT; }
inline int idAt(int i) { return i < BLOCK_COUNT ? BLOCKS[i] : i < total() ? ITEMS[i - BLOCK_COUNT] : 0; }
// GuiBuild's maxInvSize for a tier (the last page stops at the end of the list)
inline int pageEnd(int tier) {
    int n = (tier + 1) * PER_PAGE;
    if (n >= total()) n -= PER_PAGE - total() % PER_PAGE;
    return n;
}
} // namespace buildmenu
