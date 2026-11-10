#pragma once
// CraftingInventoryCreativeCB (Axeolon): every block and item, sorted next to its relatives and split into
// titled sections, each section starting on a fresh row of nine.
#include <string>
#include <vector>

#include "game/Inventory.h"

namespace creative {

struct Row {
    std::string title;   // a section title row (no items) when not empty
    ItemStack items[9];  // otherwise up to nine stacks (count 1)
};

const std::vector<Row>& rows(); // built once
constexpr int VISIBLE_ROWS = 6;

} // namespace creative
