#include "panel/popout.h"
#include "palette/palette.h"
#include "panel/windowclass.h"
#include "widgets/catalog.h"
#include "widgets/shelfwidget.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QPainter>

namespace {

constexpr int margin = 4;
constexpr int settleMs = 500;

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
