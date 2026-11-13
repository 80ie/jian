#include "panel/layout.h"

#include <algorithm>

namespace {

// Takes `amount` from the slots in `group` as evenly as their minimums allow, and returns
// what could not be taken.
int shrink(std::vector<int>& heights, std::span<const SlotHeights> slotHeights,
    std::vector<std::size_t> group, int amount)
{
    const auto room = [&](std::size_t i) { return heights[i] - slotHeights[i].minimum; };
    // Least room first, so that a share one slot cannot give falls to the rest.
    std::ranges::stable_sort(group, { }, room);
    int count = int(group.size());
    for (std::size_t i : group) {
        const int taken = std::min(room(i), (amount + count - 1) / count);
        heights[i] -= taken;
        amount -= taken;
        --count;
    }
    return amount;
}

} // namespace

std::vector<QRect> layoutSlots(const QRect& area, int gap, std::span<const SlotHeights> slotHeights)
{
    std::vector<int> heights;
    std::vector<std::size_t> spare;
    std::vector<std::size_t> others;
    int left = area.height() - gap * (int(slotHeights.size()) - 1);
    for (std::size_t i = 0; i < slotHeights.size(); ++i) {
        heights.push_back(slotHeights[i].natural);
        left -= slotHeights[i].natural;
        (slotHeights[i].wantsSpare ? spare : others).push_back(i);
    }

    if (left > 0 && !spare.empty()) {
        const int count = int(spare.size());
        for (int n = 0; n < count; ++n)
            heights[spare[n]] += left / count + (n < left % count ? 1 : 0);
    } else if (left < 0) {
        shrink(heights, slotHeights, others, shrink(heights, slotHeights, spare, -left));
    }

    std::vector<QRect> rects;
    int y = area.top();
    for (int height : heights) {
        rects.emplace_back(area.left(), y, area.width(), height);
        y += height + gap;
    }
    return rects;
}
