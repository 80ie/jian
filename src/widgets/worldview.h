#pragma once

#include "widgets/shelfwidget.h"

#include <QProcess>
#include <QSize>
#include <QStringList>
#include <QTimer>

class ForeignSlot;
class WindowModel;
struct Palette;

// Whether fvwm takes commands here: FvwmMFL's socket is there and FvwmCommand is installed.
bool fvwmReachable();
// The pager's size inside a World View `width` wide, on a screen of `screen`, with desk labels
// `labelHeight` tall: two desks stacked, each a label over a page in the screen's proportions.
QSize pagerSize(int width, QSize screen, int labelHeight);
// `font` as an Xft font name fvwm can load.
QString xftName(const QFont& font);
// The fvwm commands that color the pager from the palette.
QStringList pagerColorsets(const Palette& palette);
// The fvwm commands that start the pager afresh at `size`, its labels in the Xft font `font`.
QStringList pagerLaunch(const Palette& palette, const QString& font, QSize size);

// FvwmPager in a foreign slot, showing desks 0 and 1. FvwmPager sizes its grid when it starts,
// so a new size means a new pager. Without a window model it is an empty well and sends fvwm
// nothing.
class WorldView : public ShelfWidget {
    Q_OBJECT

public:
    WorldView(const Palette& palette, WindowModel* windows, QWidget* parent);
    ~WorldView() override;

    QString id() const override;
    int heightForWidth(int width) const override;
    void paletteSettled() override;
    // The screen whose proportions the pages take. It follows the primary screen's virtual
    // geometry as that changes.
    void setScreen(QSize size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void launch();
    void failed(const QByteArray& why);
    void relaunchSoon();
    void send(const QStringList& commands);
    void flush();

    const Palette& m_palette;
    QSize m_screen;
    ForeignSlot* m_slot = nullptr;
    QTimer m_settle;
    // The size the pager was last started at, and whether its window has yet to arrive.
    QSize m_launched;
    bool m_launching = false;
    QTimer m_deadline;
    int m_retries = 0;
    QProcess m_fvwm;
    QStringList m_queued;
};
