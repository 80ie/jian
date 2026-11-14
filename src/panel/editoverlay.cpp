#include "panel/editoverlay.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "panel/order.h"
#include "widgets/catalog.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace {

constexpr int barHeight = 20;
constexpr int buttonSize = 16;
constexpr int buttonGap = 2;
constexpr int gripLeft = 5;
constexpr int nameLeft = 15;
constexpr int stripGap = 5;
constexpr int stripPadding = 3;
constexpr int entryHeight = 20;
constexpr int entryGap = 2;
constexpr int flagHeight = 16;
constexpr double veil = 0.6;

int baseline(const QRect& r, const QFontMetrics& metrics)
{
    return r.top() + (r.height() + metrics.capHeight()) / 2;
}

// Two columns of the scale's marks.
void grip(QPainter& p, const QRect& r, const Palette& palette)
{
    const int top = r.center().y() - 5;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 2; ++column)
            paint::tick(p, { r.left() + gripLeft + 3 * column, top + 3 * row }, palette);
}

void popOutGlyph(QPainter& p, QPoint centre)
{
    p.drawRect(centre.x() - 4, centre.y() - 3, 8, 7);
    p.drawLine(centre.x() - 4, centre.y() - 2, centre.x() + 4, centre.y() - 2);
}

void removeGlyph(QPainter& p, QPoint centre)
{
    for (int dx : { 0, 1 }) {
        p.drawLine(centre.x() - 3 + dx, centre.y() - 3, centre.x() + 3 + dx, centre.y() + 3);
        p.drawLine(centre.x() + 3 + dx, centre.y() - 3, centre.x() - 3 + dx, centre.y() + 3);
    }
}

} // namespace

EditOverlay::EditOverlay(const Palette& palette, QWidget* parent)
    : QWidget(parent)
    , m_palette(palette)
{
    setMouseTracking(true);
}

int EditOverlay::stripHeight(int count)
{
    if (count == 0)
        return 0;
    return stripGap + 2 * stripPadding + count * entryHeight + (count - 1) * entryGap;
}

void EditOverlay::setDocked(std::vector<EditSlot> docked, const QRect& room)
{
    m_docked = std::move(docked);
    m_room = room;
    update();
}

void EditOverlay::setUnplaced(const QStringList& ids)
{
    m_unplaced = ids;
    update();
}

void EditOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setClipRect(m_room);
    QStringList unseen;
    for (const EditSlot& slot : m_docked) {
        if (slot.rect.top() > m_room.bottom() - barHeight) {
            unseen.append(widgetName(slot.id));
            continue;
        }
        p.setOpacity(veil);
        p.fillRect(slot.rect, m_palette.tone(0));
        p.setOpacity(1);
        paintBar(p, barOf(slot.rect), slot.id, slot.rect.bottom() > m_room.bottom());
    }
    if (!unseen.isEmpty()) {
        const QRect flag(
            m_room.left(), m_room.bottom() + 1 - flagHeight, m_room.width(), flagHeight);
        p.fillRect(flag, m_palette.selection);
        p.setPen(m_palette.tone(5));
        p.drawText(flag.adjusted(stripPadding, 0, -stripPadding, 0),
            Qt::AlignLeft | Qt::AlignVCenter,
            fontMetrics().elidedText(u"No room: "_s + unseen.join(u", "_s), Qt::ElideRight,
                flag.width() - 2 * stripPadding));
    }
    p.setClipping(false);
    paintStrip(p);
    if (m_press && m_press->dragging)
        paintDrop(p);
}

void EditOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_press = pressAt(event->position().toPoint());
    update();
}

void EditOverlay::mouseMoveEvent(QMouseEvent* event)
{
    m_mouse = event->position().toPoint();
    if (!m_press) {
        const std::optional<Press> under = pressAt(m_mouse);
        setCursor(under && !under->button ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    if (!m_press->button && !m_press->dragging
        && (m_mouse - m_press->at).manhattanLength() >= QApplication::startDragDistance()) {
        m_press->dragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
    if (m_press->dragging)
        update();
}

void EditOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_press)
        return;
    const Press press = *std::exchange(m_press, std::nullopt);
    const QPoint pos = event->position().toPoint();
    setCursor(Qt::ArrowCursor);
    update();
    if (press.button) {
        const auto slot = std::ranges::find(m_docked, press.id, &EditSlot::id);
        if (slot == m_docked.end() || !buttonOf(barOf(slot->rect), *press.button).contains(pos))
            return;
        if (*press.button == Button::PopOut)
            emit popOutRequested(press.id);
        else
            emit removeRequested(press.id);
    } else if (press.dragging && m_room.contains(pos)) {
        const qsizetype gap = dropGap();
        emit placeRequested(
            press.id, gap < qsizetype(m_docked.size()) ? m_docked[gap].id : QString());
    } else if (!press.dragging && press.fromStrip) {
        emit placeRequested(press.id, QString());
    }
}

QRect EditOverlay::barOf(const QRect& slot) const
{
    return QRect(slot.left(), slot.top(), slot.width(), std::min(barHeight, slot.height()));
}

QRect EditOverlay::buttonOf(const QRect& bar, Button button) const
{
    const int right = bar.right() - 2 - (button == Button::PopOut ? buttonSize + buttonGap : 0);
    return QRect(
        right + 1 - buttonSize, bar.center().y() + 1 - buttonSize / 2, buttonSize, buttonSize);
}

QRect EditOverlay::strip() const
{
    const int height = stripHeight(int(m_unplaced.size()));
    return QRect(
        m_room.left(), rect().bottom() + 1 - height + stripGap, m_room.width(), height - stripGap);
}

QRect EditOverlay::entryOf(int index) const
{
    const QRect inside = strip().adjusted(stripPadding, stripPadding, -stripPadding, -stripPadding);
    return QRect(inside.left(), inside.top() + index * (entryHeight + entryGap), inside.width(),
        entryHeight);
}

std::optional<EditOverlay::Press> EditOverlay::pressAt(QPoint pos) const
{
    for (const EditSlot& slot : m_docked) {
        const QRect bar = barOf(slot.rect);
        if (!bar.contains(pos) || !m_room.contains(pos))
            continue;
        for (Button button : { Button::PopOut, Button::Remove })
            if (buttonOf(bar, button).contains(pos))
                return Press { slot.id, pos, button };
        return Press { slot.id, pos, std::nullopt };
    }
    for (int i = 0; i < m_unplaced.size(); ++i)
        if (entryOf(i).contains(pos))
            return Press { m_unplaced[i], pos, std::nullopt, true };
    return std::nullopt;
}

qsizetype EditOverlay::dropGap() const
{
    std::vector<QRect> stack;
    for (const EditSlot& slot : m_docked)
        stack.push_back(slot.rect);
    return gapAt(stack, m_mouse.y());
}

void EditOverlay::paintBar(QPainter& p, const QRect& bar, const QString& id, bool clipped) const
{
    p.fillRect(bar, clipped ? m_palette.selection : m_palette.tone(0));
    paint::raised(p, bar, m_palette);
    grip(p, bar, m_palette);
    const bool pressed = m_press && m_press->id == id && m_press->button;
    for (Button button : { Button::PopOut, Button::Remove }) {
        const QRect r = buttonOf(bar, button);
        paint::button(p, r, m_palette, pressed && *m_press->button == button);
        p.setPen(m_palette.ink);
        if (button == Button::PopOut)
            popOutGlyph(p, r.center());
        else
            removeGlyph(p, r.center());
    }
    const int left = bar.left() + nameLeft;
    const int right = buttonOf(bar, Button::PopOut).left() - 4;
    QString name = widgetName(id);
    if (clipped)
        name += u" (clipped)"_s;
    p.setPen(clipped ? m_palette.tone(5) : m_palette.ink);
    p.drawText(left, baseline(bar, fontMetrics()),
        fontMetrics().elidedText(name, Qt::ElideRight, right - left));
}

void EditOverlay::paintDrop(QPainter& p) const
{
    const qsizetype gap = dropGap();
    const qsizetype count = qsizetype(m_docked.size());
    int y = m_room.top();
    if (count > 0 && gap == 0)
        y = m_docked.front().rect.top() - 2;
    else if (count > 0 && gap == count)
        y = m_docked.back().rect.bottom();
    else if (count > 0)
        y = (m_docked[gap - 1].rect.bottom() + m_docked[gap].rect.top()) / 2 - 1;
    y = std::clamp(y, m_room.top(), m_room.bottom() - 2);
    const QRect ghost(m_room.left(), m_mouse.y() - barHeight / 2, m_room.width(), barHeight);
    paintBar(p, ghost, m_press->id, false);
    p.fillRect(QRect(m_room.left(), y, m_room.width(), 3), m_palette.selection);
}

void EditOverlay::paintStrip(QPainter& p) const
{
    if (m_unplaced.isEmpty())
        return;
    paint::sunken(p, strip(), m_palette, m_palette.tone(-1));
    for (int i = 0; i < m_unplaced.size(); ++i) {
        const QRect entry = entryOf(i);
        paint::button(p, entry, m_palette, false);
        grip(p, entry, m_palette);
        p.setPen(m_palette.ink);
        p.drawText(entry.left() + nameLeft, baseline(entry, fontMetrics()),
            fontMetrics().elidedText(
                widgetName(m_unplaced[i]), Qt::ElideRight, entry.width() - nameLeft - 4));
    }
}
