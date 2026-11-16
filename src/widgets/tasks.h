#pragma once

#include "widgets/shelfwidget.h"
#include "widgets/tasklayout.h"

#include <optional>
#include <vector>

class WindowModel;
struct Palette;

// The windows on the page in view: pinned ones as a strip of icon tiles, the rest as rows,
// minimized ones last with their last capture under them.
class Tasks : public ShelfWidget {
    Q_OBJECT

public:
    // Without a model it shows what setClients() gives it and acts on nothing. `floor` is the
    // least height a capture shrinks to before it gives way to a plain row.
    Tasks(const Palette& palette, int floor, WindowModel* model, QWidget* parent);

    QString id() const override;
    int heightForWidth(int width) const override;
    int minimumHeightForWidth(int width) const override;
    bool wantsSpare() const override { return true; }
    // The thumbnail floor.
    QWidget* settingsPage() override;
    void setClients(const std::vector<Client>& clients);
    void setFloor(int floor);

signals:
    // setFloor() changed it.
    void floorChosen(int floor);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct Layout {
        QRect tiles;
        QRect rows;
        int scrollRange = 0;
        // For each task, where it is drawn and the height of the capture under it.
        std::vector<QRect> rects;
        std::vector<int> thumbnails;
    };

    Layout layout() const;
    std::optional<std::size_t> taskAt(const Layout& layout, QPoint pos) const;
    void paintTile(QPainter& p, const Layout& layout, std::size_t task, bool hovered) const;
    void paintRow(QPainter& p, const Layout& layout, std::size_t task, bool hovered) const;
    QColor faceFor(const Client& client, bool hovered) const;

    const Palette& m_palette;
    int m_floor;
    WindowModel* m_model;
    std::vector<Client> m_tasks;
    TaskCounts m_counts;
    std::optional<QPoint> m_mouse;
    int m_scroll = 0;
};
