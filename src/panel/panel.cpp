#include "panel/panel.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "panel/layout.h"
#include "panel/popout.h"
#include "panel/slotlook.h"
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
// Between the panel and a widget's window the first time it pops out.
constexpr int windowGap = 8;

} // namespace

Panel::Panel(const Palette& palette, Create create)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
    , m_palette(palette)
    , m_create(std::move(create))
    , m_dock(winId())
{
    setWindowTitle(u"shelf"_s);
    m_grip.setCursor(Qt::SplitHCursor);
    connect(QGuiApplication::primaryScreen(), &QScreen::geometryChanged, this,
        [this] { place(width()); });
}

// A window takes its widget with it; the docked ones go with the panel.
Panel::~Panel() = default;

void Panel::arrange(const QStringList& order, const QStringList& poppedOut)
{
    for (Slot& slot : m_slots)
        if (slot.docked())
            delete slot.widget;
    m_slots.clear();
    for (const QString& id : order) {
        Slot slot;
        slot.id = id;
        slot.widget = m_create(id, this);
        if (slot.widget) {
            connect(slot.widget, &ShelfWidget::heightChanged, this, &Panel::relayout);
            slot.widget->show();
        }
        m_slots.push_back(std::move(slot));
    }
    relayout();
    for (Slot& slot : m_slots)
        if (slot.docked() && poppedOut.contains(slot.id))
            lift(slot);
}

void Panel::setWindowPositions(const std::map<QString, QPoint>& positions)
{
    m_windowPositions = positions;
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
    for (const Slot& slot : m_slots) {
        if (slot.widget)
            slot.widget->paletteChanged();
        if (slot.window)
            slot.window->update();
    }
}

QStringList Panel::order() const
{
    QStringList ids;
    for (const Slot& slot : m_slots)
        ids.append(slot.id);
    return ids;
}

QStringList Panel::poppedOut() const
{
    QStringList ids;
    for (const Slot& slot : m_slots)
        if (slot.window)
            ids.append(slot.id);
    return ids;
}

void Panel::popOut(const QString& id)
{
    if (Slot* slot = find(id); slot && slot->docked()) {
        lift(*slot);
        emit arranged();
    }
}

void Panel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    paint::frame(p, frameRect(), m_palette, m_side);
    const SlotLook look = slotLook(m_separation);
    p.fillRect(slotArea(), m_palette.tone(look.ground));
    const Slot* above = nullptr;
    for (const Slot& slot : m_slots) {
        if (!slot.docked())
            continue;
        if (look.around)
            look.around(p, slot.rect, m_palette);
        if (look.between && above)
            look.between(p,
                QRect(slot.rect.left(), above->rect.bottom() + 1, slot.rect.width(), look.gap),
                m_palette);
        above = &slot;
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
    std::vector<Slot*> docked;
    std::vector<SlotHeights> heights;
    for (Slot& slot : m_slots) {
        if (!slot.docked())
            continue;
        docked.push_back(&slot);
        heights.push_back({ slot.widget->heightForWidth(width) + 2 * look.inset,
            slot.widget->minimumHeightForWidth(width) + 2 * look.inset,
            slot.widget->wantsSpare() });
    }
    const std::vector<QRect> rects = layoutSlots(area, look.gap, heights);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        docked[i]->rect = rects[i];
        docked[i]->widget->setGeometry(
            rects[i].adjusted(look.inset, look.inset, -look.inset, -look.inset));
    }
    update();
}

Panel::Slot* Panel::find(const QString& id)
{
    const auto slot = std::ranges::find(m_slots, id, &Slot::id);
    return slot == m_slots.end() ? nullptr : &*slot;
}

// The first time, the window opens beside the panel, level with the slot as far as the screen
// allows.
void Panel::lift(Slot& slot)
{
    const QRect docked(slot.widget->mapToGlobal(QPoint(0, 0)), slot.widget->size());
    slot.window = std::make_unique<PopOut>(*slot.widget, docked.width(), m_palette);
    PopOut& window = *slot.window;
    const QString id = slot.id;
    // Queued, so that the window is not deleted inside its own close event.
    connect(
        &window, &PopOut::closing, this,
        [this, id] {
            if (Slot* slot = find(id); slot && slot->window)
                dock(*slot);
        },
        Qt::QueuedConnection);
    connect(&window, &PopOut::settled, this, [this, id](QPoint at) {
        m_windowPositions[id] = at;
        emit arranged();
    });
    // The place is the frame's, as pos() reads it back. Qt takes it so only for a window it has
    // yet to make; one made already goes by its inside, which fvwm put 11 pixels low.
    QPoint at = docked.topLeft();
    if (const auto remembered = m_windowPositions.find(id); remembered != m_windowPositions.end()) {
        at = remembered->second;
    } else {
        const QRect bounds = screen()->geometry();
        at.setX(m_side == Qt::LeftEdge ? geometry().right() + 1 + windowGap
                                       : geometry().left() - windowGap - window.width());
        at.setY(std::clamp(at.y(), bounds.top(), bounds.bottom() + 1 - window.height()));
    }
    window.move(at);
    window.show();
    relayout();
}

void Panel::dock(Slot& slot)
{
    m_windowPositions[slot.id] = slot.window->pos();
    slot.widget->setParent(this);
    slot.widget->show();
    slot.window.reset();
    relayout();
    emit arranged();
}
