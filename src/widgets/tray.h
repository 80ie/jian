#pragma once

#include "widgets/shelfwidget.h"

#include <QProcess>
#include <QRect>
#include <QStringList>
#include <QTimer>

#include <span>

class ForeignSlot;
class WindowModel;
struct Palette;

// The stalonetray command line for a tray `columns` icons wide on the well's floor. It names no
// rc file, so that nothing but these arguments sets it up.
QStringList trayArguments(const Palette& palette, int columns);
// How many icons fit across a tray widget `width` wide.
int trayColumns(int width);
// The height of a tray widget `width` wide holding `icons`, at least one row.
int trayHeight(int width, int icons);
// How many of stalonetray's child windows, at `geometries`, are icons: it keeps two 1x1 windows of
// its own there.
int countIcons(std::span<const QRect> geometries);

// The system tray: stalonetray, launched and stopped by the panel and held in a foreign slot, as
// tall as its icons need. Without a window model it is an empty well and launches nothing.
class Tray : public ShelfWidget {
    Q_OBJECT

public:
    Tray(const Palette& palette, WindowModel* windows, QWidget* parent);
    ~Tray() override;

    QString id() const override;
    int heightForWidth(int width) const override;
    void paletteChanged() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void restart();
    void start();
    void measure();
    void setIcons(int icons);

    const Palette& m_palette;
    WindowModel* m_windows;
    ForeignSlot* m_slot = nullptr;
    QProcess m_process;
    bool m_restarting = false;
    int m_columns = 0;
    int m_icons = 0;
    QTimer m_measure;
};
