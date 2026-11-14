#include "widgets/medialayout.h"

#include <algorithm>
#include <cmath>

using namespace std::chrono;

namespace {

constexpr int fieldHeight = 19;
constexpr int buttonHeight = 22;
constexpr int buttonGap = 2;
constexpr int trackHeight = 10;
constexpr int ruleHeight = 2;
constexpr int gap = 3;
// The groove sits this far into the thumb's height, and its ends this far from the widget's.
constexpr int grooveDrop = 6;
constexpr int grooveLeft = 28;
constexpr int grooveRight = 24;
constexpr int shortestGroove = 20;
constexpr QRect speaker(2, 1, 16, 16);
// Faster than this a moving fill only costs repaints.
constexpr milliseconds quickestStep(40);

} // namespace

MediaLayout mediaLayout(int width, QSize art, bool track)
{
    MediaLayout l;
    int y = 0;
    const auto take = [&y](int height) {
        const int top = y;
        y += height + gap;
        return top;
    };
    l.source = QRect(0, take(fieldHeight), width, fieldHeight);
    if (!art.isEmpty()) {
        const int side = std::min(width, qRound(double(width) * art.height() / art.width()));
        l.art = QRect(0, take(side), width, side);
        l.artRule = take(ruleHeight);
    }
    l.title = QRect(0, take(fieldHeight), width, fieldHeight);
    if (track)
        l.track = QRect(0, take(trackHeight), width, trackHeight);
    const int buttonsTop = take(buttonHeight);
    const int room = std::max(4, width - 3 * buttonGap);
    int x = 0;
    for (int i = 0; i < 4; ++i) {
        const int buttonWidth = room / 4 + (i < room % 4);
        l.buttons[i] = QRect(x, buttonsTop, buttonWidth, buttonHeight);
        x += buttonWidth + buttonGap;
    }
    l.rule = take(ruleHeight);
    l.thumbTop = take(thumbSize.height());
    l.speaker = speaker.translated(0, l.thumbTop);
    l.groove = QRect(grooveLeft, l.thumbTop + grooveDrop,
        std::max(shortestGroove, width - grooveLeft - grooveRight), 5);
    l.output = QRect(0, y, width, fieldHeight);
    l.height = l.output.bottom() + 1;
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

QRect trackInside(const QRect& track) { return track.adjusted(2, 2, -1, -1); }

int trackFill(const QRect& track, microseconds position, microseconds length)
{
    const double done = std::clamp(double(position.count()) / length.count(), 0.0, 1.0);
    return qRound(trackInside(track).width() * done);
}

microseconds trackPosition(const QRect& track, int x, microseconds length)
{
    const QRect inside = trackInside(track);
    const double done = std::clamp(double(x - inside.left()) / inside.width(), 0.0, 1.0);
    return microseconds(std::llround(length.count() * done));
}

milliseconds trackStep(const QRect& track, microseconds length, double rate)
{
    const double pixel = length.count() / (trackInside(track).width() * rate);
    return std::max(quickestStep, duration_cast<milliseconds>(duration<double, std::micro>(pixel)));
}

int marqueeStep(int offset, int span) { return span > 0 ? (offset + 1) % span : 0; }
