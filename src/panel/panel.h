#pragma once

#include "panel/config.h"
#include "panel/dock.h"

#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class PopOut;
class ShelfWidget;
struct Palette;

// The top-level window down one side of the primary screen, holding a column of widgets. A
// widget can leave the column for a window of its own and come back to its place.
class Panel : public QWidget {
    Q_OBJECT

public:
    // Makes the widget config.ini calls `id`, or gives null for one that cannot run here.
    using Create = std::function<ShelfWidget*(const QString& id, QWidget* parent)>;

    Panel(const Palette& palette, Create create);
    ~Panel() override;

    // Replaces the widgets with those `order` names, `poppedOut` of them in windows of their own.
    // A widget that cannot run here keeps its place in the order.
    void arrange(const QStringList& order, const QStringList& poppedOut);
    // Where each widget's window was last, by id. A widget that pops out goes back there.
    void setWindowPositions(const std::map<QString, QPoint>& positions);
    void setSide(Qt::Edge side);
    void setWidth(int width);
    void setSeparation(Separation separation);
    // Redraws, and tells each widget.
    void paletteChanged();

    QStringList order() const;
    QStringList poppedOut() const;
    const std::map<QString, QPoint>& windowPositions() const { return m_windowPositions; }

    // Moves the widget `id` into a window of its own.
    void popOut(const QString& id);

signals:
    // Changes made on the panel itself, not through the setters.
    void widthDragged(int width);
    void sideSwitched(Qt::Edge side);
    // A widget popped out or came back, or a window holding one settled somewhere new.
    void arranged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Slot {
        QString id;
        ShelfWidget* widget = nullptr;
        // Holds the widget while it is out of the panel.
        std::unique_ptr<PopOut> window;
        // Where the slot is drawn, its edge included, while the widget is docked.
        QRect rect;

        bool docked() const { return widget && !window; }
    };
    struct Drag {
        int fromX = 0;
        int width = 0;
    };

    QRect frameRect() const;
    QRect slotArea() const;
    void place(int width);
    void relayout();
    Slot* find(const QString& id);
    void lift(Slot& slot);
    void dock(Slot& slot);

    const Palette& m_palette;
    Create m_create;
    Qt::Edge m_side = Qt::RightEdge;
    Separation m_separation = Separation::Tiles;
    std::vector<Slot> m_slots;
    std::map<QString, QPoint> m_windowPositions;
    // Carries the resize cursor over the frame, where a cursor on the panel would leak into
    // the widgets.
    QWidget m_grip { this };
    std::optional<Drag> m_drag;
    DockHints m_dock;
};
