#pragma once

#include "paint/font.h"

#include <QGuiApplication>
#include <QWidget>

// What the panel asks of the widget in each slot. A widget paints only its own rectangle and
// does not know whether it is docked.
class ShelfWidget : public QWidget {
    Q_OBJECT

public:
    explicit ShelfWidget(QWidget* parent)
        : QWidget(parent)
    {
        setFont(paint::font(QGuiApplication::font(), paint::textPoints));
    }

    // Stable across versions: config.ini names widgets by it.
    virtual QString id() const = 0;
    // The natural height.
    int heightForWidth(int width) const override = 0;
    virtual int minimumHeightForWidth(int width) const { return heightForWidth(width); }
    // Takes a share of the height the panel has left over.
    virtual bool wantsSpare() const { return false; }
    virtual bool sunken() const { return false; }
    virtual void paletteChanged() { update(); }
    // Where a widget hands colors to another program, which would fall behind a drag in the editor.
    virtual void paletteSettled() { }
    // A page for the settings window that changes this widget's settings, or null if it has none.
    // The caller owns it.
    virtual QWidget* settingsPage() { return nullptr; }

signals:
    void heightChanged();
};
