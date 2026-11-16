#include "widgets/tasks.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "x11/requests.h"
#include "x11/trace.h"
#include "x11/windowmodel.h"

#include <QContextMenuEvent>
#include <QFormLayout>
#include <QHelpEvent>
#include <QMenu>
#include <QPainter>
#include <QSpinBox>
#include <QTextDocument>
#include <QToolTip>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr int rowIcon = 16;
constexpr int tileIcon = 24;
// Of a row: the icon's left edge, and the gaps either side of the title.
constexpr int iconLeft = 6;
constexpr int titleLeft = 6;
constexpr int titleRight = 4;
// Around a capture: the trough's margin in its row, and the capture's in the trough.
constexpr int troughMargin = 6;
constexpr int troughTop = 4;
constexpr int troughInset = 4;
constexpr int tileInset = 3;

QRect inside(const QRect& well)
{
    return well.adjusted(wellPadding, wellPadding, -wellPadding, -wellPadding);
}

QString titleOf(const Client& client)
{
    return client.title.isEmpty() ? u"(untitled)"_s : client.title;
}

void drawIcon(QPainter& p, const QImage& icon, const QRect& box)
{
    p.drawImage(
        box.topLeft(), icon.scaled(box.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// Filling `box`'s width at a fixed scale, top first, so that a shrinking box crops the capture
// rather than rescaling it.
void drawCropped(QPainter& p, const QImage& image, const QRect& box)
{
    const double scale
        = std::max(double(box.width()) / image.width(), double(largestThumbnail) / image.height());
    const QImage scaled = image.scaled(qRound(image.width() * scale),
        qRound(image.height() * scale), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p.save();
    p.setClipRect(box, Qt::IntersectClip);
    p.drawImage(box.left() + (box.width() - scaled.width()) / 2, box.top(), scaled);
    p.restore();
}

} // namespace

Tasks::Tasks(const Palette& palette, int thumbnailFloor, WindowModel* model, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_thumbnailFloor(thumbnailFloor)
    , m_model(model)
{
    setMouseTracking(true);
    if (m_model) {
        connect(m_model, &WindowModel::changed, this, [this] { setClients(m_model->shown()); });
        setClients(m_model->shown());
    }
}

QString Tasks::id() const { return u"tasks"_s; }

int Tasks::heightForWidth(int width) const { return naturalHeight(width, m_counts); }

QWidget* Tasks::settingsPage()
{
    auto* page = new QWidget;
    auto* floor = new QSpinBox(page);
    floor->setRange(rowHeight, largestThumbnail);
    floor->setValue(m_thumbnailFloor);
    floor->setSuffix(u" px"_s);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(u"Smallest thumbnail"_s, floor);
    connect(floor, &QSpinBox::valueChanged, this, &Tasks::setThumbnailFloor);
    return page;
}

void Tasks::setThumbnailFloor(int floor)
{
    m_thumbnailFloor = floor;
    update();
    emit thumbnailFloorChosen(floor);
}

int Tasks::minimumHeightForWidth(int width) const { return ::minimumHeight(width, m_counts); }

void Tasks::setClients(const std::vector<Client>& clients)
{
    const TaskCounts before = m_counts;
    m_tasks = taskOrder(clients);
    m_counts = countTasks(m_tasks);
    if (m_counts.pinned != before.pinned || m_counts.rows != before.rows
        || m_counts.thumbnails != before.thumbnails)
        emit heightChanged();
    update();
}

bool Tasks::event(QEvent* event)
{
    if (event->type() != QEvent::ToolTip)
        return ShelfWidget::event(event);
    const auto* help = static_cast<QHelpEvent*>(event);
    const std::optional<std::size_t> task = taskAt(layout(), help->pos());
    if (task && m_tasks[*task].pinned) {
        // A tooltip renders anything that looks like markup, and any program can set a title.
        QToolTip::showText(
            help->globalPos(), Qt::convertFromPlainText(titleOf(m_tasks[*task])), this);
    } else {
        QToolTip::hideText();
        event->ignore();
    }
    return true;
}

void Tasks::paintEvent(QPaintEvent*)
{
    const Layout l = layout();
    const std::optional<std::size_t> hovered = m_mouse ? taskAt(l, *m_mouse) : std::nullopt;
    QPainter p(this);
    p.fillRect(rect(), m_palette.tone(0));
    for (const QRect& well : { l.tiles, l.rows })
        if (!well.isNull())
            paint::sunken(p, well, m_palette, m_palette.tone(-1));
    for (std::size_t i = 0; i < m_tasks.size(); ++i) {
        const bool tile = m_tasks[i].pinned;
        p.setClipRect(inside(tile ? l.tiles : l.rows));
        if (tile)
            paintTile(p, l, i, i == hovered);
        else
            paintRow(p, l, i, i == hovered);
    }
    qCInfo(traceX, "%.3f tasks painted %zu", traceClock(), m_tasks.size());
}

void Tasks::mouseMoveEvent(QMouseEvent* event)
{
    const Layout l = layout();
    const std::optional<std::size_t> before = m_mouse ? taskAt(l, *m_mouse) : std::nullopt;
    m_mouse = event->position().toPoint();
    if (taskAt(l, *m_mouse) != before)
        update();
}

void Tasks::leaveEvent(QEvent*)
{
    m_mouse.reset();
    update();
}

void Tasks::mouseReleaseEvent(QMouseEvent* event)
{
    const std::optional<std::size_t> task = taskAt(layout(), event->position().toPoint());
    if (!m_model || !task)
        return;
    if (event->button() == Qt::LeftButton)
        m_model->toggle(m_tasks[*task].window);
    else if (event->button() == Qt::MiddleButton)
        closeClient(m_model->connection(), m_tasks[*task].window);
}

// Away from a task the event goes on to the panel and its own menu.
void Tasks::contextMenuEvent(QContextMenuEvent* event)
{
    const std::optional<std::size_t> task = taskAt(layout(), event->pos());
    if (!m_model || !task) {
        event->ignore();
        return;
    }
    const Client& client = m_tasks[*task];
    const quint32 window = client.window;
    const XConnection* x = &m_model->connection();
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(client.pinned ? u"Unpin"_s : u"Pin"_s, this,
        [x, window, pin = !client.pinned] { setClientPinned(*x, window, pin); });
    menu->addSeparator();
    if (client.minimized)
        menu->addAction(u"Restore"_s, this, [x, window] { activateClient(*x, window); });
    else
        menu->addAction(u"Minimize"_s, this, [x, window] { minimizeClient(*x, window); });
    menu->addAction(client.maximized ? u"Unmaximize"_s : u"Maximize"_s, this,
        [x, window, maximize = !client.maximized] { setClientMaximized(*x, window, maximize); });
    menu->addSeparator();
    menu->addAction(u"Close"_s, this, [x, window] { closeClient(*x, window); });
    menu->popup(event->globalPos());
}

void Tasks::wheelEvent(QWheelEvent* event)
{
    const int range = layout().scrollRange;
    if (range == 0) {
        event->ignore();
        return;
    }
    const int scroll = std::clamp(m_scroll, 0, range) - event->angleDelta().y() * rowHeight / 120;
    m_scroll = std::clamp(scroll, 0, range);
    update();
}

// Pinned tasks come first in m_tasks, so a task's index is also its place in the grid.
Tasks::Layout Tasks::layout() const
{
    const TaskFit fit = fitTasks(size(), m_counts, m_thumbnailFloor);
    Layout l;
    int top = 0;
    const int columns = tileColumns(width());
    if (m_counts.pinned > 0) {
        const int gridRows = (m_counts.pinned + columns - 1) / columns;
        l.tiles = QRect(0, 0, width(), 2 * wellPadding + gridRows * tileSize - etch);
        top = l.tiles.bottom() + 1 + wellGap;
    }
    if (m_counts.rows + m_counts.thumbnails > 0 || m_counts.pinned == 0)
        l.rows = QRect(0, top, width(), height() - top);
    const QRect tiles = inside(l.tiles);
    const QRect rows = inside(l.rows);
    l.scrollRange = std::max(0, fit.listHeight - rows.height());
    const std::vector<quint32> kept = keptThumbnails(m_tasks, fit.thumbnails);
    int y = rows.top() - std::clamp(m_scroll, 0, l.scrollRange);
    for (std::size_t i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].pinned) {
            l.rects.emplace_back(tiles.left() + int(i) % columns * tileSize,
                tiles.top() + int(i) / columns * tileSize, tileSize, tileSize);
            l.thumbnails.push_back(0);
            continue;
        }
        const bool shows = std::ranges::find(kept, m_tasks[i].window) != kept.end();
        const int thumbnail = shows ? fit.thumbnailHeight : 0;
        const int height = rowHeight + (shows ? thumbnailPadding + thumbnail : 0);
        l.rects.emplace_back(rows.left(), y, rows.width(), height);
        l.thumbnails.push_back(thumbnail);
        y += height;
    }
    return l;
}

std::optional<std::size_t> Tasks::taskAt(const Layout& layout, QPoint pos) const
{
    for (std::size_t i = 0; i < m_tasks.size(); ++i)
        if (layout.rects[i].contains(pos)
            && inside(m_tasks[i].pinned ? layout.tiles : layout.rows).contains(pos))
            return i;
    return std::nullopt;
}

void Tasks::paintTile(QPainter& p, const Layout& layout, std::size_t task, bool hovered) const
{
    const Client& client = m_tasks[task];
    const QRect r = layout.rects[task];
    const QRect tiles = inside(layout.tiles);
    const int columns = tileColumns(width());
    const QRect face = r.adjusted(0, 0, -etch, -etch);
    p.fillRect(face, faceFor(client, hovered));
    p.setPen(m_palette.tone(-4));
    p.drawLine(face.right() + 1, r.top(), face.right() + 1, face.bottom());
    p.setPen(m_palette.tone(3));
    p.drawLine(face.right() + 2, r.top(), face.right() + 2, face.bottom());
    const bool lastRow = int(task) / columns == (m_counts.pinned - 1) / columns;
    if (int(task) % columns == 0 && !lastRow)
        paint::divider(p, QRect(tiles.left(), r.bottom() - 1, tiles.width(), etch), m_palette);

    if (!client.icon.isNull()) {
        p.setOpacity(client.minimized ? 0.6 : 1);
        drawIcon(p, client.icon,
            QRect(face.left() + tileInset, face.top() + tileInset, tileIcon, tileIcon));
        p.setOpacity(1);
    } else {
        p.setPen(m_palette.ink);
        p.drawText(face, Qt::AlignCenter, client.title.isEmpty() ? u"?"_s : client.title.left(1));
    }
    if (client.focused) {
        p.setPen(m_palette.tone(-6));
        p.drawRect(face.adjusted(0, 0, -1, -1));
    }
}

// The focused task is outlined rather than given another face, so the mark survives whatever
// face it has: minimized and focused is a real state while a window comes back up.
void Tasks::paintRow(QPainter& p, const Layout& layout, std::size_t task, bool hovered) const
{
    const Client& client = m_tasks[task];
    const QRect r = layout.rects[task];
    p.fillRect(r, faceFor(client, hovered));
    paint::divider(p, QRect(r.left(), r.bottom() - 1, r.width(), etch), m_palette);

    const QRect icon(r.left() + iconLeft, r.top() + (rowHeight - rowIcon) / 2, rowIcon, rowIcon);
    if (!client.icon.isNull())
        drawIcon(p, client.icon, icon);
    const QFontMetrics metrics = fontMetrics();
    const int x = icon.right() + 1 + titleLeft;
    p.setPen(client.minimized ? m_palette.tone(-4) : m_palette.ink);
    p.drawText(x, r.top() + (rowHeight + metrics.capHeight()) / 2,
        metrics.elidedText(titleOf(client), Qt::ElideRight, r.right() - x - titleRight));

    if (const int thumbnail = layout.thumbnails[task]; thumbnail > 0) {
        const QRect trough(r.left() + troughMargin, r.top() + rowHeight + troughTop,
            r.width() - 2 * troughMargin, thumbnail + 2 * troughInset);
        paint::trough(p, trough, m_palette);
        drawCropped(p, client.thumbnail,
            trough.adjusted(troughInset, troughInset, -troughInset, -troughInset));
    }
    if (client.focused) {
        p.setPen(m_palette.tone(-6));
        p.drawRect(r.adjusted(0, 0, -1, -etch));
    }
}

QColor Tasks::faceFor(const Client& client, bool hovered) const
{
    return m_palette.tone(hovered ? 2 : client.minimized ? -1 : 0);
}
