#include "panel/panel.h"
#include "paint/bevel.h"
#include "paint/font.h"
#include "palette/palette.h"
#include "panel/editoverlay.h"
#include "panel/layout.h"
#include "panel/order.h"
#include "panel/popout.h"
#include "widgets/foreignslot.h"
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

// Between the panel and a widget's window the first time it pops out.
constexpr int windowGap = 8;
constexpr int bayMargin = 2;
constexpr int bayGap = 5;
constexpr int bayBevel = 1;
// An edited palette changes many times a second, and another program takes each color slowly.
constexpr int settleMs = 300;

// `raised` turned half round: shaded at the top and left, lit at the bottom and right.
void bay(QPainter& p, const QRect& r, const Palette& palette)
{
    p.save();
    p.setTransform(QTransform(-1, 0, 0, -1, r.left() + r.right(), r.top() + r.bottom()), true);
    paint::raised(p, r, palette);
    p.restore();
}

// The windows of other programs that `widget` holds stop taking input while it is edited.
void freeze(ShelfWidget& widget, bool frozen)
{
    for (ForeignSlot* slot : widget.findChildren<ForeignSlot*>())
        slot->setFrozen(frozen);
}

} // namespace

Panel::Panel(const Palette& palette, QStringList offered, Create create)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
    , m_palette(palette)
    , m_offered(std::move(offered))
    , m_create(std::move(create))
    , m_dock(winId())
{
    setWindowTitle(u"jian"_s);
    setFont(paint::font(QGuiApplication::font(), paint::textPoints));
    m_grip.setCursor(Qt::SplitHCursor);
    m_settle.setSingleShot(true);
    m_settle.setInterval(settleMs);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        for (const Slot& slot : m_slots)
            if (slot.widget)
                slot.widget->paletteSettled();
    });
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
    for (const QString& id : order)
        m_slots.push_back(make(id));
    relayout();
    for (Slot& slot : m_slots)
        if (slot.docked() && poppedOut.contains(slot.id))
            lift(slot);
    if (m_overlay)
        m_overlay->raise();
    emit widgetsChanged();
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

void Panel::paletteChanged()
{
    update();
    for (const Slot& slot : m_slots) {
        if (slot.widget)
            slot.widget->paletteChanged();
        if (slot.window)
            slot.window->update();
    }
    m_settle.start();
}

QStringList Panel::order() const
{
    QStringList ids;
    for (const Slot& slot : m_slots)
        ids.append(slot.id);
    return ids;
}

ShelfWidget* Panel::widget(const QString& id) const
{
    const auto slot = std::ranges::find(m_slots, id, &Slot::id);
    return slot == m_slots.end() ? nullptr : slot->widget;
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

void Panel::setEditing(bool editing)
{
    if (editing == this->editing())
        return;
    if (editing) {
        m_overlay = new EditOverlay(m_palette, this);
        connect(m_overlay, &EditOverlay::placeRequested, this, &Panel::put);
        connect(m_overlay, &EditOverlay::popOutRequested, this, &Panel::popOut);
        connect(m_overlay, &EditOverlay::removeRequested, this, &Panel::remove);
        m_overlay->show();
    } else {
        delete std::exchange(m_overlay, nullptr);
    }
    for (Slot& slot : m_slots)
        if (slot.docked())
            freeze(*slot.widget, editing);
    relayout();
    emit editingChanged(editing);
    if (!editing)
        emit arranged();
}

void Panel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    paint::frame(p, frameRect(), m_palette, m_side);
    for (const Slot& slot : m_slots)
        if (slot.docked() && !slot.widget->sunken())
            bay(p, slot.rect, m_palette);
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
    menu->addAction(editing() ? u"Done editing"_s : u"Edit widgets"_s, this,
        [this] { setEditing(!editing()); });
    menu->addAction(other == Qt::LeftEdge ? u"Dock left"_s : u"Dock right"_s, this, [this, other] {
        setSide(other);
        emit sideSwitched(other);
    });
    menu->addSeparator();
    menu->addAction(u"Settings…"_s, this, &Panel::settingsRequested);
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
    width = std::clamp(width, narrowestPanel, widestPanel);
    const int x = m_side == Qt::LeftEdge ? bounds.left() : bounds.right() + 1 - width;
    const QRect docked(x, bounds.top(), width, bounds.height());
    setGeometry(docked);
    m_dock.setStrut(m_side, docked, screen->virtualGeometry());
}

void Panel::relayout()
{
    QRect area = slotArea().adjusted(bayMargin, bayMargin, -bayMargin, -bayMargin);
    QStringList unplaced;
    if (m_overlay) {
        unplaced = ::unplaced(m_offered, order());
        area.setBottom(area.bottom() - EditOverlay::stripHeight(int(unplaced.size())));
    }
    std::vector<Slot*> docked;
    std::vector<SlotHeights> heights;
    for (Slot& slot : m_slots) {
        if (!slot.docked())
            continue;
        docked.push_back(&slot);
        const int bevel = slot.widget->sunken() ? 0 : bayBevel;
        const int width = area.width() - 2 * bevel;
        heights.push_back({ slot.widget->heightForWidth(width) + 2 * bevel,
            slot.widget->minimumHeightForWidth(width) + 2 * bevel, slot.widget->wantsSpare() });
    }
    const std::vector<QRect> rects = layoutSlots(area, bayGap, heights);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const int bevel = docked[i]->widget->sunken() ? 0 : bayBevel;
        docked[i]->rect = rects[i];
        docked[i]->widget->setGeometry(rects[i].adjusted(bevel, bevel, -bevel, -bevel));
    }
    if (m_overlay) {
        const QPoint origin = slotArea().topLeft();
        std::vector<EditSlot> edited;
        for (const Slot* slot : docked)
            edited.push_back({ slot->id, slot->rect.translated(-origin) });
        m_overlay->setGeometry(slotArea());
        m_overlay->setDocked(std::move(edited), area.translated(-origin));
        m_overlay->setUnplaced(unplaced);
    }
    update();
}

// A slot for `id`, holding a new widget unless the widget cannot run here.
Panel::Slot Panel::make(const QString& id)
{
    Slot slot;
    slot.id = id;
    slot.widget = m_create(id, this);
    if (slot.widget) {
        connect(slot.widget, &ShelfWidget::heightChanged, this, &Panel::relayout);
        freeze(*slot.widget, editing());
        slot.widget->show();
    }
    return slot;
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
    freeze(*slot.widget, false);
    relayout();
}

void Panel::dock(Slot& slot)
{
    m_windowPositions[slot.id] = slot.window->pos();
    slot.widget->setParent(this);
    freeze(*slot.widget, editing());
    slot.widget->show();
    slot.window.reset();
    if (m_overlay)
        m_overlay->raise();
    relayout();
    emit arranged();
}

void Panel::put(const QString& id, const QString& before)
{
    const bool made = !find(id);
    if (made) {
        Slot slot = make(id);
        if (!slot.widget)
            return;
        m_slots.push_back(std::move(slot));
        if (m_overlay)
            m_overlay->raise();
    }
    const QStringList order = placed(this->order(), id, before);
    std::ranges::sort(m_slots, { }, [&order](const Slot& slot) { return order.indexOf(slot.id); });
    relayout();
    if (made)
        emit widgetsChanged();
}

void Panel::remove(const QString& id)
{
    const auto slot = std::ranges::find(m_slots, id, &Slot::id);
    if (slot == m_slots.end())
        return;
    if (slot->docked())
        delete slot->widget;
    m_slots.erase(slot);
    relayout();
    emit widgetsChanged();
}
