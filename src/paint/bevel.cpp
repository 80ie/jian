#include "paint/bevel.h"
#include "palette/palette.h"

#include <QLinearGradient>
#include <QPainter>

#include <algorithm>
#include <utility>

namespace {

// Photon shades by moving every channel the same amount, not by mixing in white or black.
QColor shifted(const QColor& color, int delta)
{
    const auto channel = [delta](int value) { return std::clamp(value + delta, 0, 255); };
    return QColor(channel(color.red()), channel(color.green()), channel(color.blue()));
}

} // namespace

namespace paint {

void raised(QPainter& p, const QRect& r, const Palette& palette)
{
    p.setPen(palette.tone(3));
    p.drawLine(r.topLeft(), r.topRight());
    p.drawLine(r.topLeft(), r.bottomLeft());
    p.setPen(palette.tone(-4));
    p.drawLine(r.bottomLeft(), r.bottomRight());
    p.drawLine(r.topRight(), r.bottomRight());
}

void sunken(QPainter& p, const QRect& r, const Palette& palette, const QColor& floor)
{
    const QRect outline = r.adjusted(0, 0, -2, -2);
    p.fillRect(outline.adjusted(1, 1, 0, 0), floor);
    p.setPen(palette.tone(-4));
    p.drawRect(outline);
    p.setPen(palette.tone(-2));
    p.drawLine(r.left() + 1, r.top() + 1, r.right() - 2, r.top() + 1);
    p.drawLine(r.left() + 1, r.top() + 1, r.left() + 1, r.bottom() - 2);
    p.setPen(palette.tone(1));
    p.drawLine(r.left() + 1, r.bottom(), r.right(), r.bottom());
    p.drawLine(r.right(), r.top() + 1, r.right(), r.bottom());
}

void trough(QPainter& p, const QRect& r, const Palette& palette)
{
    const QRect outline = r.adjusted(0, 0, -1, -1);
    p.fillRect(outline.adjusted(1, 1, 0, 0), palette.tone(-2));
    p.setPen(palette.tone(-6));
    p.drawRect(outline);
    p.setPen(palette.tone(-4));
    p.drawLine(outline.left() + 1, outline.top() + 1, outline.right() - 1, outline.top() + 1);
    p.drawLine(outline.left() + 1, outline.top() + 1, outline.left() + 1, outline.bottom() - 1);
}

void bar(QPainter& p, const QRect& r, const QColor& color)
{
    if (r.isEmpty())
        return;
    p.fillRect(r, color);
    p.setPen(shifted(color, 40));
    p.drawLine(r.topLeft(), r.topRight());
    p.drawLine(r.topLeft(), r.bottomLeft());
    p.setPen(shifted(color, -40));
    p.drawLine(r.bottomLeft(), r.bottomRight());
    p.drawLine(r.topRight(), r.bottomRight());
}

void groove(QPainter& p, const QRect& r, const Palette& palette)
{
    int y = r.top();
    for (int tone : { -1, -3, -4, -6, 3 }) {
        p.setPen(palette.tone(tone));
        p.drawLine(r.left(), y, r.right(), y);
        ++y;
    }
}

void button(QPainter& p, const QRect& r, const Palette& palette, bool down)
{
    QColor lit = palette.tone(3);
    QColor shaded = palette.tone(-3);
    if (down)
        std::swap(lit, shaded);
    const QRect inner = r.adjusted(1, 1, -1, -1);
    QLinearGradient gradient(0, inner.top(), 0, inner.bottom());
    gradient.setColorAt(0, lit);
    gradient.setColorAt(1, shaded);
    p.fillRect(inner, gradient);
    p.setPen(palette.tone(-6));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setPen(lit);
    p.drawLine(inner.topLeft(), inner.topRight());
    p.drawLine(inner.topLeft(), inner.bottomLeft());
    p.setPen(shaded);
    p.drawLine(inner.bottomLeft(), inner.bottomRight());
    p.drawLine(inner.topRight(), inner.bottomRight());
}

void thumb(QPainter& p, const QRect& r, const Palette& palette)
{
    QLinearGradient gradient(0, r.top() + 1, 0, r.bottom() - 1);
    gradient.setColorAt(0, palette.tone(4));
    gradient.setColorAt(1, palette.tone(-1));
    p.fillRect(r.adjusted(1, 1, -1, -1), gradient);
    p.setPen(palette.tone(-6));
    p.drawRect(r.adjusted(0, 0, -1, -1));
}

void frame(QPainter& p, const QRect& r, const Palette& palette, Qt::Edge side)
{
    const int step = side == Qt::RightEdge ? 1 : -1;
    int x = side == Qt::RightEdge ? r.left() : r.right();
    for (int tone : { -6, 3, 0, 0, -3, -6, 3 }) {
        p.setPen(palette.tone(tone));
        p.drawLine(x, r.top(), x, r.bottom());
        x += step;
    }
}

void divider(QPainter& p, const QRect& r, const Palette& palette)
{
    p.setPen(palette.tone(-4));
    p.drawLine(r.left(), r.top(), r.right(), r.top());
    p.setPen(palette.tone(3));
    p.drawLine(r.left(), r.top() + 1, r.right(), r.top() + 1);
}

void tick(QPainter& p, QPoint at, const Palette& palette)
{
    p.fillRect(at.x(), at.y(), 1, 2, palette.tone(-3));
    p.fillRect(at.x() + 1, at.y(), 1, 2, palette.tone(-5));
}

} // namespace paint
