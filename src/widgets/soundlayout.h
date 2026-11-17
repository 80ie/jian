#pragma once

#include <QRect>
#include <QSize>

// The volume slider's thumb.
constexpr QSize thumbSize(10, 17);

// Where each part of the sound widget goes.
struct SoundLayout {
    QRect speaker;
    QRect groove;
    int thumbTop = 0;
    QRect output;
    int height = 0;
};

// The layout `width` wide.
SoundLayout soundLayout(int width);

// The thumb's left edge on `groove` at `volume`, which runs from 0 to 1.
int thumbLeft(const QRect& groove, double volume);
// The volume a press at `x` on `groove` asks for.
double volumeAt(const QRect& groove, int x);
