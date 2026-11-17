#include "widgets/medialayout.h"

#include <algorithm>
#include <cmath>

using namespace std::chrono;

namespace {

constexpr int fieldInset = 4;
constexpr int ruleInset = 2;
constexpr int fieldHeight = 19;
constexpr int buttonHeight = 22;
constexpr int buttonGap = 2;
constexpr int trackHeight = 10;
constexpr int ruleHeight = 2;
constexpr int gap = 3;
// Faster than this a moving fill only costs repaints.
constexpr milliseconds quickestStep(40);

} // namespace

MediaLayout mediaLayout(int width, QSize art, bool track)
{
    MediaLayout l;
    const int inner = width - 2 * fieldInset;
    int y = fieldInset;
    const auto take = [&y](int height) {
        const int top = y;
        y += height + gap;
        return top;
    };
    const auto field = [&](int height) { return QRect(fieldInset, take(height), inner, height); };
    const auto rule
        = [&] { return QRect(ruleInset, take(ruleHeight), width - 2 * ruleInset, ruleHeight); };
    l.source = field(fieldHeight);
    if (!art.isEmpty()) {
        l.art = field(std::min(inner, qRound(double(inner) * art.height() / art.width())));
        l.artRule = rule();
    }
    l.title = field(fieldHeight);
    if (track)
        l.track = field(trackHeight);
    const int buttonsTop = take(buttonHeight);
    const int room = std::max(4, inner - 3 * buttonGap);
    int x = fieldInset;
    for (int i = 0; i < 4; ++i) {
        const int buttonWidth = room / 4 + (i < room % 4);
        l.buttons[i] = QRect(x, buttonsTop, buttonWidth, buttonHeight);
        x += buttonWidth + buttonGap;
    }
    l.volumeRule = rule();
    l.sound = soundLayout(width, y);
    l.height = l.sound.height;
    return l;
}

QRect trackInside(const QRect& track)
{
    return QRect(track.left() + 1, track.top() + 1, track.width() - 2, track.height() - 3);
}

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
