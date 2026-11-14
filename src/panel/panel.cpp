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
constexpr int slotGap = 3;

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
    const QRect area = rect().adjusted(m_side == Qt::RightEdge ? paint::frameWidth : 0, 0,
        m_side == Qt::LeftEdge ? -paint::frameWidth : 0, 0);
    std::vector<SlotHeights> heights;
    for (const ShelfWidget* widget : m_widgets)
        heights.push_back({ widget->heightForWidth(area.width()),
            widget->minimumHeightForWidth(area.width()), widget->wantsSpare() });
    const std::vector<QRect> rects = layoutSlots(area, slotGap, heights);
    for (std::size_t i = 0; i < rects.size(); ++i)
        m_widgets[i]->setGeometry(rects[i]);
}
