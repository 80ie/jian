#include "panel/popout.h"
#include "palette/palette.h"
#include "widgets/catalog.h"
#include "widgets/shelfwidget.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QPainter>

#include <xcb/xcb.h>

namespace {

constexpr int margin = 4;
constexpr int settleMs = 500;

// Qt gives every window of a program the same WM_CLASS as it creates it, and the window manager
// reads it at map time, so it is replaced in between. fvwm matches Style names with case, so the
// class "Shelf" keeps the panel's `Style shelf` line off these windows.
void setWindowClass(WId window, const QString& instance)
{
    if (QGuiApplication::platformName() != u"xcb")
        return;
    xcb_connection_t* c
        = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->connection();
    const QByteArray value = instance.toUtf8() + '\0' + "Shelf" + '\0';
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, quint32(window), XCB_ATOM_WM_CLASS,
        XCB_ATOM_STRING, 8, value.size(), value.constData());
    xcb_flush(c);
}

} // namespace

PopOut::PopOut(ShelfWidget& widget, int width, const Palette& palette)
    : m_palette(palette)
    , m_widget(widget)
{
    setWindowTitle(widgetName(widget.id()));
    widget.setParent(this);
    widget.show();
    connect(&widget, &ShelfWidget::heightChanged, this, &PopOut::fit);
    resize(width + 2 * margin, 0);
    fit();
    m_settle.setSingleShot(true);
    m_settle.setInterval(settleMs);
    connect(&m_settle, &QTimer::timeout, this, [this] { emit settled(pos()); });
}

// Qt has made the window by now and has yet to map it.
void PopOut::showEvent(QShowEvent*)
{
    setWindowClass(winId(), QGuiApplication::applicationName() + u'-' + m_widget.id());
}

void PopOut::closeEvent(QCloseEvent* event)
{
    event->ignore();
    emit closing();
}

void PopOut::moveEvent(QMoveEvent*)
{
    if (isVisible())
        m_settle.start();
}

void PopOut::resizeEvent(QResizeEvent*)
{
    m_widget.setGeometry(rect().adjusted(margin, margin, -margin, -margin));
    fit();
}

void PopOut::paintEvent(QPaintEvent*) { QPainter(this).fillRect(rect(), m_palette.tone(0)); }

void PopOut::fit()
{
    const int height = m_widget.heightForWidth(width() - 2 * margin) + 2 * margin;
    if (height != this->height())
        resize(width(), height);
}
