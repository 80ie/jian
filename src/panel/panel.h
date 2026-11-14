#pragma once

#include "panel/dock.h"

#include <QWidget>

#include <optional>
#include <vector>

class ShelfWidget;
struct Palette;

// The top-level window down one side of the primary screen, holding a column of widgets.
class Panel : public QWidget {
    Q_OBJECT

public:
    explicit Panel(const Palette& palette);

    // Replaces the widgets, which must be children of the panel, and deletes the old ones.
    void setWidgets(std::vector<ShelfWidget*> widgets);
    void setSide(Qt::Edge side);
    void setWidth(int width);
    // Redraws, and tells each widget.
    void paletteChanged();

signals:
    // Changes made on the panel itself, not through the setters.
    void widthDragged(int width);
    void sideSwitched(Qt::Edge side);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Drag {
        int fromX = 0;
        int width = 0;
    };

    QRect frameRect() const;
    void place(int width);
    void relayout();

    const Palette& m_palette;
    Qt::Edge m_side = Qt::RightEdge;
    std::vector<ShelfWidget*> m_widgets;
    // Carries the resize cursor over the frame, where a cursor on the panel would leak into
    // the widgets.
    QWidget m_grip { this };
    std::optional<Drag> m_drag;
    DockHints m_dock;
};
