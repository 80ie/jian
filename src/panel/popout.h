#pragma once

#include <QTimer>
#include <QWidget>

class ShelfWidget;
struct Palette;

// A widget out of the panel, in a normal window of its own whose WM_CLASS is "shelf-<id>",
// "Shelf", so that a window manager rule can pick it out. The window keeps to the widget's
// natural height at its width.
class PopOut : public QWidget {
    Q_OBJECT

public:
    // Takes `widget` from wherever it is, to hold it `width` wide.
    PopOut(ShelfWidget& widget, int width, const Palette& palette);

    ShelfWidget& widget() const { return m_widget; }

signals:
    // The window was closed, and the widget wants to go back.
    void closing();
    // The window's frame has stayed at `at` for a moment after moving.
    void settled(QPoint at);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void fit();

    const Palette& m_palette;
    ShelfWidget& m_widget;
    QTimer m_settle;
};
