#pragma once

#include <QStringList>
#include <QWidget>

#include <optional>
#include <vector>

struct Palette;

// A docked widget as edit mode sees it: its id and its slot, edge included.
struct EditSlot {
    QString id;
    QRect rect;
};

// Edit mode's layer over the panel: on each slot a bar with the widget's name, a grip to drag it
// by and buttons to pop it out and remove it, and under the slots a strip of the widgets that are
// not placed, to drag or click in. It takes every press, so the widgets under it get none.
class EditOverlay : public QWidget {
    Q_OBJECT

public:
    EditOverlay(const Palette& palette, QWidget* parent);

    // How tall the strip is with `count` widgets in it.
    static int stripHeight(int count);
    // The docked slots in this widget's coordinates, top to bottom, and the room above the strip:
    // a slot that runs past its bottom is flagged.
    void setDocked(std::vector<EditSlot> docked, const QRect& room);
    void setUnplaced(const QStringList& ids);

signals:
    // `before` is the id of the widget to go in front of, or empty to go last.
    void placeRequested(const QString& id, const QString& before);
    void popOutRequested(const QString& id);
    void removeRequested(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Button { PopOut, Remove };
    struct Press {
        QString id;
        QPoint at;
        std::optional<Button> button;
        bool fromStrip = false;
        bool dragging = false;
    };

    QRect barOf(const QRect& slot) const;
    QRect buttonOf(const QRect& bar, Button button) const;
    QRect strip() const;
    QRect entryOf(int index) const;
    std::optional<Press> pressAt(QPoint pos) const;
    qsizetype dropGap() const;
    void paintBar(QPainter& p, const QRect& bar, const QString& id, bool clipped) const;
    void paintDrop(QPainter& p) const;
    void paintStrip(QPainter& p) const;

    const Palette& m_palette;
    std::vector<EditSlot> m_docked;
    QRect m_room;
    QStringList m_unplaced;
    std::optional<Press> m_press;
    QPoint m_mouse;
};
