#include "widgets/tasks.h"
#include "paint/bevel.h"
#include "palette/palette.h"
#include "x11/trace.h"
#include "x11/windowmodel.h"

#include <QContextMenuEvent>
#include <QFormLayout>
#include <QHelpEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QSpinBox>
#include <QTextDocument>
#include <QToolTip>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr int rowIcon = 16;
constexpr int largeIcon = 32;
// Of a row: the icon's left edge, and the gaps either side of the title.
constexpr int iconLeft = 6;
constexpr int titleLeft = 7;
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

void drawFitted(QPainter& p, const QImage& image, const QRect& box)
{
    const QImage scaled = image.scaled(
        box.size().boundedTo(image.size()), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    p.drawImage(box.left() + (box.width() - scaled.width()) / 2,
        box.top() + (box.height() - scaled.height()) / 2, scaled);
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

Tasks::Tasks(const Palette& palette, const TaskSizes& sizes, WindowModel* model, QWidget* parent)
    : ShelfWidget(parent)
    , m_palette(palette)
    , m_sizes(sizes)
    , m_model(model)
{
    setMouseTracking(true);
    if (m_model) {
        connect(m_model, &WindowModel::changed, this, [this] { setClients(m_model->shown()); });
        setClients(m_model->shown());
    }
}

QString Tasks::id() const { return u"tasks"_s; }

int Tasks::heightForWidth(int) const { return naturalHeight(m_counts, m_sizes); }

QWidget* Tasks::settingsPage()
{
    auto* page = new QWidget;
    const auto spinBox = [page](int value, int least) {
        auto* box = new QSpinBox(page);
        box->setRange(least, largestThumbnail);
        box->setValue(value);
        box->setSuffix(u" px"_s);
        return box;
    };
    auto* tile = spinBox(m_sizes.tile, iconTileHeight);
    auto* floor = spinBox(m_sizes.floor, rowHeight);
    auto* hint = new QLabel(u"A pinned window is a tile with its picture until the list runs out "
                            "of room, when the tiles drop to icons. A minimized window's picture "
                            "shrinks as the list fills, down to the floor, below which it gives "
                            "way to a plain row."_s,
        page);
    hint->setWordWrap(true);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->addRow(u"Pinned tile height"_s, tile);
    form->addRow(u"Smallest minimized picture"_s, floor);
    form->addRow(hint);
    for (QSpinBox* box : { tile, floor })
        connect(box, &QSpinBox::valueChanged, this,
            [this, tile, floor] { setSizes({ tile->value(), floor->value() }); });
    return page;
}

void Tasks::setSizes(const TaskSizes& sizes)
{
    m_sizes = sizes;
    emit heightChanged();
    update();
    emit sizesChosen(sizes);
}

int Tasks::minimumHeightForWidth(int) const { return ::minimumHeight(m_counts); }

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
        m_model->close(m_tasks[*task].window);
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
    WindowModel* model = m_model;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(client.pinned ? u"Unpin"_s : u"Pin"_s, this,
        [model, window, pin = !client.pinned] { model->setPinned(window, pin); });
    menu->addSeparator();
    if (client.minimized)
        menu->addAction(u"Restore"_s, this, [model, window] { model->activate(window); });
    else
        menu->addAction(u"Minimize"_s, this, [model, window] { model->minimize(window); });
    menu->addAction(client.maximized ? u"Unmaximize"_s : u"Maximize"_s, this,
        [model, window, maximize = !client.maximized] { model->setMaximized(window, maximize); });
    menu->addSeparator();
    menu->addAction(u"Close"_s, this, [model, window] { model->close(window); });
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
    const TaskFit fit = fitTasks(height(), m_counts, m_sizes);
    Layout l;
    int top = 0;
    if (m_counts.pinned > 0) {
        const int gridRows = (m_counts.pinned + 1) / 2;
        l.tiles = QRect(0, 0, width(), 2 * wellPadding + gridRows * fit.tileHeight - etch);
        top = l.tiles.bottom() + 1 + wellGap;
    }
    if (m_counts.rows + m_counts.thumbnails > 0 || m_counts.pinned == 0)
        l.rows = QRect(0, top, width(), height() - top);
    l.tileThumbnails = fit.tileHeight == m_sizes.tile;
    const QRect tiles = inside(l.tiles);
    const QRect rows = inside(l.rows);
    l.scrollRange = std::max(0, fit.listHeight - rows.height());
    const std::vector<quint32> kept = keptThumbnails(m_tasks, fit.thumbnails);
    int y = rows.top() - std::clamp(m_scroll, 0, l.scrollRange);
    for (std::size_t i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].pinned) {
            const int half = tiles.width() / 2;
            const bool left = i % 2 == 0;
            l.rects.emplace_back(left ? tiles.left() : tiles.left() + half,
                tiles.top() + int(i / 2) * fit.tileHeight, left ? half : tiles.width() - half,
                fit.tileHeight);
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

// A tile has an etched rule on its right when it is in the left column, and one under it; the
// well's own edge closes the rest.
void Tasks::paintTile(QPainter& p, const Layout& layout, std::size_t task, bool hovered) const
{
    const Client& client = m_tasks[task];
    const QRect r = layout.rects[task];
    const bool left = task % 2 == 0;
    const QRect face = r.adjusted(0, 0, left ? -etch : 0, -etch);
    p.fillRect(face, faceFor(client, hovered));
    if (left) {
        p.setPen(m_palette.tone(-4));
        p.drawLine(face.right() + 1, r.top(), face.right() + 1, face.bottom());
        p.setPen(m_palette.tone(3));
        p.drawLine(face.right() + 2, r.top(), face.right() + 2, face.bottom());
    }
    paint::divider(p, QRect(r.left(), r.bottom() - 1, r.width(), etch), m_palette);

    const QRect inner = face.adjusted(tileInset, tileInset, -tileInset, -tileInset);
    p.setOpacity(client.minimized ? 0.6 : 1);
    if (layout.tileThumbnails && !client.thumbnail.isNull()) {
        drawFitted(p, client.thumbnail, inner);
    } else if (!client.icon.isNull()) {
        const int size = std::min(largeIcon, inner.height());
        drawFitted(p, client.icon,
            QRect(inner.center().x() - size / 2, inner.center().y() - size / 2, size, size));
    } else {
        p.setPen(m_palette.ink);
        p.drawText(face, Qt::AlignCenter, titleOf(client).left(1));
    }
    p.setOpacity(1);
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
        drawFitted(p, client.icon, icon);
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
