#pragma once

#include <QRect>
#include <QSize>

#include <array>
#include <chrono>

// The volume slider's thumb.
constexpr QSize thumbSize(10, 17);

// Where each part of the media widget goes. The heights are the Python shelf's, which took them
// off the reference screenshot.
struct MediaLayout {
    QRect source;
    // Null when there is nothing for the well to show.
    QRect art;
    QRect artRule;
    QRect title;
    // Null when the trackbar is hidden.
    QRect track;
    std::array<QRect, 4> buttons;
    QRect volumeRule;
    QRect speaker;
    QRect groove;
    int thumbTop = 0;
    QRect output;
    int height = 0;
};

// The layout `width` wide, with a well in the proportions of `art` if it is not empty.
MediaLayout mediaLayout(int width, QSize art, bool track);

// The thumb's left edge on `groove` at `volume`, which runs from 0 to 1.
int thumbLeft(const QRect& groove, double volume);
// The volume a press at `x` on `groove` asks for.
double volumeAt(const QRect& groove, int x);

// Where the trackbar's fill goes, inside its trough.
QRect trackInside(const QRect& track);
// How wide the fill is `position` into a track `length` long.
int trackFill(
    const QRect& track, std::chrono::microseconds position, std::chrono::microseconds length);
// The position a press at `x` on the trackbar asks for.
std::chrono::microseconds trackPosition(
    const QRect& track, int x, std::chrono::microseconds length);
// How long playing at `rate` takes to move the fill by a pixel.
std::chrono::milliseconds trackStep(
    const QRect& track, std::chrono::microseconds length, double rate);

// A title too wide for its field scrolls by a pixel a step and comes round again after a gap.
// `span` is the width of the title and the gap, 0 for a title that fits and stands still.
int marqueeStep(int offset, int span);
