#include "widgets/mediapaint.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "widgets/medialayout.h"

#include <QPainter>
#include <QPolygon>

namespace {

constexpr int cellWidth = 15;
constexpr int textLeft = 3;
constexpr int artInset = 4;

// Inside a sunken field's bevel and clear of its lip.
QRect fieldInside(const QRect& field) { return field.adjusted(2, 2, -2, -2); }

int baseline(const QRect& r, const QFontMetrics& metrics)
{
    return r.top() + (r.height() + metrics.capHeight()) / 2;
}

void triangle(QPainter& p, int x, int y, bool left)
{
    if (left)
        p.drawPolygon(QPolygon({ QPoint(x + 5, y - 3), QPoint(x + 5, y + 4), QPoint(x, y) }));
    else
        p.drawPolygon(QPolygon({ QPoint(x, y - 3), QPoint(x, y + 4), QPoint(x + 5, y) }));
}

void glyph(QPainter& p, QPoint centre, Transport which, bool playing)
{
    const int x = centre.x();
    const int y = centre.y();
    switch (which) {
    case Transport::Previous:
        triangle(p, x - 6, y, true);
        triangle(p, x - 1, y, true);
        break;
    case Transport::Stop:
        p.drawRect(x - 3, y - 3, 7, 7);
        break;
    case Transport::PlayPause:
        if (playing) {
            p.drawRect(x - 3, y - 3, 2, 7);
            p.drawRect(x + 1, y - 3, 2, 7);
        } else {
            p.drawPolygon(
                QPolygon({ QPoint(x - 2, y - 3), QPoint(x - 2, y + 4), QPoint(x + 3, y) }));
        }
        break;
    case Transport::Next:
        triangle(p, x - 4, y, false);
        triangle(p, x + 1, y, false);
        break;
    }
}

// Drawn rather than taken from an icon theme, which a render cannot count on.
void speaker(QPainter& p, const QRect& r)
{
    const int y = r.center().y();
    p.drawPolygon(QPolygon({ QPoint(r.left() + 2, y - 3), QPoint(r.left() + 6, y - 3),
        QPoint(r.left() + 11, r.top() + 2), QPoint(r.left() + 11, r.bottom() - 2),
        QPoint(r.left() + 6, y + 3), QPoint(r.left() + 2, y + 3) }));
}

} // namespace

void drawSelector(
    QPainter& p, const QRect& r, const Palette& palette, const QString& text, bool enabled)
{
    paint::sunken(p, r, palette, palette.tone(5));
    const QRect inside = fieldInside(r);
    const QRect cell(inside.right() + 1 - cellWidth, inside.top(), cellWidth, inside.height());
    paint::button(p, cell, palette, false);
    const QColor ink = enabled ? palette.ink : palette.tone(-4);
    const QPoint centre = cell.center();
    for (int row = 0; row < 4; ++row)
        p.fillRect(centre.x() - 3 + row, centre.y() - 2 + row, 7 - 2 * row, 1, ink);

    const int room = cell.left() - inside.left() - 2 * textLeft;
    const QFontMetrics metrics = p.fontMetrics();
    p.setPen(ink);
    p.drawText(inside.left() + textLeft, baseline(inside, metrics),
        metrics.elidedText(text, Qt::ElideRight, room));
}

void drawArt(QPainter& p, const QRect& well, const Palette& palette, const QImage& art)
{
    paint::trough(p, well, palette);
    const QRect box = well.adjusted(artInset, artInset, -artInset, -artInset);
    const QImage scaled = art.scaled(box.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    p.drawImage(box.left() + (box.width() - scaled.width()) / 2,
        box.top() + (box.height() - scaled.height()) / 2, scaled);
}

int titleRoom(const QRect& field) { return fieldInside(field).width() - 2 * textLeft; }

void drawTitle(QPainter& p, const QRect& field, const Palette& palette, const QString& title,
    bool enabled, int scroll, int span)
{
    paint::sunken(p, field, palette, palette.tone(5));
    const QRect inside = fieldInside(field);
    const int y = baseline(inside, p.fontMetrics());
    const int x = inside.left() + textLeft - scroll;
    p.save();
    p.setClipRect(inside);
    p.setPen(enabled ? palette.ink : palette.tone(-4));
    if (span > 0) {
        p.drawText(x, y, title + marqueeGap);
        p.drawText(x + span, y, title + marqueeGap);
    } else {
        p.drawText(x, y, title);
    }
    p.restore();
}

void drawTrack(QPainter& p, const QRect& track, const Palette& palette, int fill)
{
    paint::trough(p, track, palette);
    QRect filled = trackInside(track);
    filled.setWidth(fill);
    p.fillRect(filled, palette.fill);
}

void drawTransport(QPainter& p, const std::array<QRect, 4>& buttons, const Palette& palette,
    const std::array<bool, 4>& enabled, std::optional<Transport> down, bool playing)
{
    for (int i = 0; i < 4; ++i) {
        const auto which = Transport(i);
        const bool pressed = down == which;
        paint::button(p, buttons[i], palette, pressed);
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(enabled[i] ? palette.ink : palette.tone(-4));
        // Photon's whole press animation: the mark moves a pixel down and right.
        glyph(p, buttons[i].center() + QPoint(pressed, pressed), which, playing);
        p.restore();
    }
}

void drawVolume(
    QPainter& p, const MediaLayout& l, const Palette& palette, double volume, bool muted)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    p.setBrush(muted ? palette.tone(-4) : palette.ink);
    speaker(p, l.speaker);
    p.restore();
    paint::groove(p, l.groove, palette);
    // Five marks, ends included, spaced by the groove's width rather than a fixed pitch.
    const int travel = l.groove.width() - thumbSize.width();
    for (int i = 0; i < 5; ++i)
        paint::tick(p,
            { l.groove.left() + thumbSize.width() / 2 + qRound(travel * i / 4.0),
                l.thumbTop + thumbSize.height() - 1 },
            palette);
    paint::thumb(p, QRect(QPoint(thumbLeft(l.groove, volume), l.thumbTop), thumbSize), palette);
}
