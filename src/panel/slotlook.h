#pragma once

#include "panel/config.h"

#include <QRect>

class QPainter;
struct Palette;

// How the panel draws the edges of its slots and spaces them for `separation`.
struct SlotLook {
    // Between the panel's edges and the slots, between one slot and the next, and between a
    // slot's edge and its widget.
    int margin = 0;
    int gap = 0;
    int inset = 0;
    // The tone between the slots.
    int ground = 0;
    void (*around)(QPainter& p, const QRect& slot, const Palette& palette) = nullptr;
    void (*between)(QPainter& p, const QRect& gap, const Palette& palette) = nullptr;
};

SlotLook slotLook(Separation separation);
