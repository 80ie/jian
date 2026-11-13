#pragma once

#include <QRect>

#include <span>
#include <vector>

// What the widget in a slot asks for at the panel's width.
struct SlotHeights {
    int natural = 0;
    int minimum = 0;
    bool wantsSpare = false;
};

// Stacks the slots down `area`, `gap` apart, sharing out its height as spec section 4 says.
// Slots that do not fit even at their minimum run past the bottom.
std::vector<QRect> layoutSlots(
    const QRect& area, int gap, std::span<const SlotHeights> slotHeights);
