#pragma once

#include <QRect>
#include <QSize>

#include <array>

// The volume slider's thumb.
constexpr QSize thumbSize(10, 17);

// Where each part of the media widget goes. The heights are the Python shelf's, which took them
// off the reference screenshot.
struct MediaLayout {
    QRect source;
    // Null when there is nothing for the well to show.
    QRect art;
    // The top rows of the dividers under the art and above the volume.
    int artRule = 0;
    QRect title;
    std::array<QRect, 4> buttons;
    int rule = 0;
    QRect speaker;
    QRect groove;
    int thumbTop = 0;
    QRect output;
    int height = 0;
};

// The layout `width` wide, with a well in the proportions of `art` if it is not empty.
MediaLayout mediaLayout(int width, QSize art);

// The thumb's left edge on `groove` at `volume`, which runs from 0 to 1.
int thumbLeft(const QRect& groove, double volume);
// The volume a press at `x` on `groove` asks for.
double volumeAt(const QRect& groove, int x);

// A title too wide for its field scrolls by a pixel a step and comes round again after a gap.
// `span` is the width of the title and the gap, 0 for a title that fits and stands still.
int marqueeStep(int offset, int span);
