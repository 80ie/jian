#include "panel/panel.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "panel/layout.h"
#include "widgets/shelfwidget.h"

#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr int narrowest = 110;
constexpr int widest = 420;

struct SlotLook {
    // Between the panel's edges and the slots, between one slot and the next, and between a
    // slot's edge and its widget.
    int margin = 0;
    int gap = 0;
    int inset = 0;
    // The tone between the slots.
    int ground = 0;
    void (*around)(QPainter& p, const QRect& slot, const Palette& palette) = nullptr;
    void (*between)(QPainter& p, const QRect& gap, const Palette& palette) = nullptr;
};

void tile(QPainter& p, const QRect& r, const Palette& palette)
{
    p.fillRect(r, palette.tone(0));
    paint::raised(p, r, palette);
}

// `raised` turned half round: shaded at the top and left, lit at the bottom and right.
void recessed(QPainter& p, const QRect& r, const Palette& palette)
{
    p.save();
    p.setTransform(QTransform(-1, 0, 0, -1, r.left() + r.right(), r.top() + r.bottom()), true);
    paint::raised(p, r, palette);
    p.restore();
}

void groove(QPainter& p, const QRect& gap, const Palette& palette)
{
    paint::groove(p, QRect(gap.left(), gap.center().y() - 2, gap.width(), 5), palette);
}

SlotLook slotLook(Separation separation)
{
    switch (separation) {
    case Separation::Tiles:
        return { 3, 5, 3, -2, tile, nullptr };
    case Separation::Bays:
        return { 2, 5, 1, 0, recessed, nullptr };
    case Separation::Grooves:
        return { 0, 13, 0, 0, nullptr, groove };
    }
    return { };
}

} // namespace

Panel::Panel(const Palette& palette)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
    , m_palette(palette)
    , m_dock(winId())
{
    setWindowTitle(u"shelf"_s);
    m_grip.setCursor(Qt::SplitHCursor);
    connect(QGuiApplication::primaryScreen(), &QScreen::geometryChanged, this,
        [this] { place(width()); });
}

void Panel::setWidgets(std::vector<ShelfWidget*> widgets)
{
    for (ShelfWidget* widget : m_widgets)
        delete widget;
    m_widgets = std::move(widgets);
    for (ShelfWidget* widget : m_widgets) {
        connect(widget, &ShelfWidget::heightChanged, this, &Panel::relayout);
        widget->show();
    }
    relayout();
}

void Panel::setSide(Qt::Edge side)
{
    m_side = side;
    place(width());
    m_grip.setGeometry(frameRect());
    relayout();
    update();
}

void Panel::setWidth(int width) { place(width); }

void Panel::setSeparation(Separation separation)
{
    m_separation = separation;
    relayout();
    update();
}

void Panel::paletteChanged()
{
    update();
    for (ShelfWidget* widget : m_widgets)
        widget->paletteChanged();
}

void Panel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    paint::frame(p, frameRect(), m_palette, m_side);
    const SlotLook look = slotLook(m_separation);
    p.fillRect(slotArea(), m_palette.tone(look.ground));
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        if (look.around)
            look.around(p, m_slots[i], m_palette);
        if (look.between && i > 0)
            look.between(p,
                QRect(m_slots[i].left(), m_slots[i - 1].bottom() + 1, m_slots[i].width(), look.gap),
                m_palette);
    }
}

void Panel::resizeEvent(QResizeEvent*)
{
    m_grip.setGeometry(frameRect());
    relayout();
}

// Presses on the frame land on m_grip, which passes them up unhandled.
void Panel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && frameRect().contains(event->position().toPoint()))
        m_drag = Drag { qRound(event->globalPosition().x()), width() };
}

void Panel::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_drag)
        return;
    const int moved = qRound(event->globalPosition().x()) - m_drag->fromX;
    place(m_drag->width + (m_side == Qt::RightEdge ? -moved : moved));
}

void Panel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_drag)
        return;
    const bool resized = width() != m_drag->width;
    m_drag.reset();
    if (resized)
        emit widthDragged(width());
}

void Panel::contextMenuEvent(QContextMenuEvent* event)
{
    const Qt::Edge other = m_side == Qt::RightEdge ? Qt::LeftEdge : Qt::RightEdge;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(other == Qt::LeftEdge ? u"Dock left"_s : u"Dock right"_s, this, [this, other] {
        setSide(other);
        emit sideSwitched(other);
    });
    menu->popup(event->globalPos());
}

QRect Panel::slotArea() const
{
    return rect().adjusted(m_side == Qt::RightEdge ? paint::frameWidth : 0, 0,
        m_side == Qt::LeftEdge ? -paint::frameWidth : 0, 0);
}

QRect Panel::frameRect() const
{
    const int x = m_side == Qt::RightEdge ? 0 : width() - paint::frameWidth;
    return QRect(x, 0, paint::frameWidth, height());
}

void Panel::place(int width)
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    const QRect bounds = screen->geometry();
    width = std::clamp(width, narrowest, widest);
    const int x = m_side == Qt::LeftEdge ? bounds.left() : bounds.right() + 1 - width;
    const QRect docked(x, bounds.top(), width, bounds.height());
    setGeometry(docked);
    m_dock.setStrut(m_side, docked, screen->virtualGeometry());
}

void Panel::relayout()
{
    const SlotLook look = slotLook(m_separation);
    const QRect area = slotArea().adjusted(look.margin, look.margin, -look.margin, -look.margin);
    const int width = area.width() - 2 * look.inset;
    std::vector<SlotHeights> heights;
    for (const ShelfWidget* widget : m_widgets)
        heights.push_back({ widget->heightForWidth(width) + 2 * look.inset,
            widget->minimumHeightForWidth(width) + 2 * look.inset, widget->wantsSpare() });
    m_slots = layoutSlots(area, look.gap, heights);
    for (std::size_t i = 0; i < m_slots.size(); ++i)
        m_widgets[i]->setGeometry(
            m_slots[i].adjusted(look.inset, look.inset, -look.inset, -look.inset));
    update();
}
