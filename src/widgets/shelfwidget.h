#pragma once

#include <QWidget>

// What the panel asks of the widget in each slot. A widget paints only its own rectangle and
// does not know whether it is docked.
class ShelfWidget : public QWidget {
    Q_OBJECT

public:
    using QWidget::QWidget;

    // Stable across versions: config.ini names widgets by it.
    virtual QString id() const = 0;
    // The natural height.
    int heightForWidth(int width) const override = 0;
    virtual int minimumHeightForWidth(int width) const { return heightForWidth(width); }
    // Takes a share of the height the panel has left over.
    virtual bool wantsSpare() const { return false; }
    // The palette it draws from has changed. A widget that hands colors to another program
    // hands them over again.
    virtual void paletteChanged() { update(); }

signals:
    void heightChanged();
};
