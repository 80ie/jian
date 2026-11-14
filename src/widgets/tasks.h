#pragma once

#include "widgets/shelfwidget.h"
#include "widgets/tasklayout.h"

#include <optional>
#include <vector>

class WindowModel;
struct Palette;

// The windows on the page in view: pinned ones as a grid of tiles, the rest as rows, minimized
// ones last with their last capture under them.
class Tasks : public ShelfWidget {
    Q_OBJECT

public:
    // Without a model it shows what setClients() gives it and acts on nothing.
    Tasks(const Palette& palette, const TaskSizes& sizes, WindowModel* model, QWidget* parent);

    QString id() const override;
    int heightForWidth(int width) const override;
    int minimumHeightForWidth(int width) const override;
    bool wantsSpare() const override { return true; }
    // The pinned tile's height and the thumbnail floor.
    QWidget* settingsPage() override;
    void setClients(const std::vector<Client>& clients);
    void setSizes(const TaskSizes& sizes);

signals:
    // setSizes() changed them.
    void sizesChosen(const TaskSizes& sizes);

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
        bool tileThumbnails = false;
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
    TaskSizes m_sizes;
    WindowModel* m_model;
    std::vector<Client> m_tasks;
    TaskCounts m_counts;
    std::optional<QPoint> m_mouse;
    int m_scroll = 0;
};
