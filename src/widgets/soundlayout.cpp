#include "widgets/soundlayout.h"

#include <algorithm>

namespace {

constexpr int fieldInset = 4;
constexpr int fieldHeight = 19;
constexpr int gap = 3;
// The groove sits this far into the thumb's height, and its ends this far from the widget's.
constexpr int grooveDrop = 6;
constexpr int grooveLeft = 28;
constexpr int grooveRight = 24;
constexpr int shortestGroove = 20;
constexpr QRect speaker(2, 1, 16, 16);

} // namespace

SoundLayout soundLayout(int width)
{
    SoundLayout l;
    const int inner = width - 2 * fieldInset;
    l.thumbTop = fieldInset;
    l.speaker = speaker.translated(fieldInset, l.thumbTop);
    l.groove = QRect(fieldInset + grooveLeft, l.thumbTop + grooveDrop,
        std::max(shortestGroove, inner - grooveLeft - grooveRight), 5);
    l.output = QRect(fieldInset, l.thumbTop + thumbSize.height() + gap, inner, fieldHeight);
    l.height = l.output.bottom() + 1 + fieldInset;
    return l;
}

int thumbLeft(const QRect& groove, double volume)
{
    return groove.left() + qRound((groove.width() - thumbSize.width()) * volume);
}

double volumeAt(const QRect& groove, int x)
{
    const int travel = std::max(1, groove.width() - thumbSize.width());
    return std::clamp(double(x - groove.left() - thumbSize.width() / 2) / travel, 0.0, 1.0);
}
