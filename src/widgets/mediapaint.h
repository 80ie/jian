#pragma once

#include <QColor>
#include <QImage>
#include <QLatin1StringView>
#include <QRect>
#include <QString>

#include <array>
#include <optional>

class QPainter;
struct MediaLayout;
struct Palette;

enum class Transport { Previous, Stop, PlayPause, Next };

// What separates the end of a scrolling title from its beginning coming round again.
inline constexpr QLatin1StringView marqueeGap("   ...   ");

void drawSelector(
    QPainter& p, const QRect& r, const Palette& palette, const QString& text, bool enabled);
void drawArt(QPainter& p, const QRect& well, const Palette& palette, const QImage& art);
// How wide a title can be before it has to scroll.
int titleRoom(const QRect& field);
// While `span` is not 0, the title is drawn `scroll` pixels into a loop of itself and the gap.
void drawTitle(QPainter& p, const QRect& field, const Palette& palette, const QString& title,
    bool enabled, int scroll, int span);
void drawTrack(QPainter& p, const QRect& track, const Palette& palette, int fill);
// The buttons' marks keep the reference's sizes whatever the buttons': a 7-pixel square, and
// triangles 5 wide by 7 tall. While `playing`, play is drawn as pause.
void drawTransport(QPainter& p, const std::array<QRect, 4>& buttons, const Palette& palette,
    const std::array<bool, 4>& enabled, std::optional<Transport> down, bool playing);
// The speaker, the groove with its scale, and the thumb at `volume`.
void drawVolume(
    QPainter& p, const MediaLayout& l, const Palette& palette, double volume, bool muted);
