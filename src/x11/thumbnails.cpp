#include "x11/thumbnails.h"
#include "x11/connection.h"
#include "x11/reply.h"
#include "x11/trace.h"
#include "x11/windowmodel.h"

#include <QImage>

#include <xcb/composite.h>
#include <xcb/damage.h>

#include <algorithm>

namespace {

const QSize largest(400, 400);
constexpr int paceMs = 1000;

// The window may have gone, and its error is dropped here rather than reported by Qt.
void discard(xcb_connection_t* c, xcb_void_cookie_t cookie)
{
    xcb_discard_reply(c, cookie.sequence);
}

} // namespace

QImage thumbnailFrom(xcb_get_image_reply_t& reply, int width, int height)
{
    if ((reply.depth != 24 && reply.depth != 32)
        || xcb_get_image_data_length(&reply) < width * height * 4)
        return QImage();
    auto* pixels = reinterpret_cast<quint32*>(xcb_get_image_data(&reply));
    // Above a 24-bit pixel is a byte of padding, which QImage::Format_RGB32 wants opaque.
    if (reply.depth == 24)
        std::for_each(pixels, pixels + width * height, [](quint32& pixel) { pixel |= 0xff000000; });
    const QImage image(reinterpret_cast<const uchar*>(pixels), width, height, width * 4,
        reply.depth == 32 ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
    // The image wraps the reply, and scaled() hands back the image itself when it fits already.
    if (image.width() <= largest.width() && image.height() <= largest.height())
        return image.copy();
    return image.scaled(largest, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

std::unique_ptr<Thumbnails> Thumbnails::open(XConnection& x, WindowModel& model)
{
    xcb_connection_t* c = x.xcb();
    const xcb_query_extension_reply_t* composite = xcb_get_extension_data(c, &xcb_composite_id);
    const xcb_query_extension_reply_t* damage = xcb_get_extension_data(c, &xcb_damage_id);
    if (composite && composite->present && damage && damage->present) {
        // Both extensions must be told the version the client speaks before they are used.
        const auto compositeCookie = xcb_composite_query_version(c, 0, 2);
        const auto damageCookie = xcb_damage_query_version(c, 1, 1);
        const auto compositeVersion = take(c, xcb_composite_query_version_reply, compositeCookie);
        const auto damageVersion = take(c, xcb_damage_query_version_reply, damageCookie);
        if (compositeVersion && damageVersion
            && (compositeVersion->major_version > 0 || compositeVersion->minor_version >= 2))
            return std::make_unique<Thumbnails>(x, model, damage->first_event + XCB_DAMAGE_NOTIFY);
    }
    qWarning("no Composite 0.2 or Damage extension: tasks show icons only");
    return nullptr;
}

Thumbnails::Thumbnails(XConnection& x, WindowModel& model, quint8 damageNotify)
    : m_x(x)
    , m_model(model)
    , m_damageNotify(damageNotify)
{
    m_next.setSingleShot(true);
    connect(&m_next, &QTimer::timeout, this, &Thumbnails::captureNext);
    m_pace.setInterval(paceMs);
    connect(&m_pace, &QTimer::timeout, this, &Thumbnails::captureDamaged);
    connect(&x, &XConnection::received, this, &Thumbnails::handle);
    connect(&model, &WindowModel::changed, this, &Thumbnails::update);
    update();
}

// Redirection gives a window storage of its own to name and read, and automatic redirection
// leaves drawing it on screen to the server. A window coming back into view is asked again;
// the server refuses a second request from the same client, and the error is dropped.
void Thumbnails::update()
{
    xcb_connection_t* c = m_x.xcb();
    std::vector<quint32> shown;
    std::vector<quint32> pinned;
    quint32 focused = 0;
    QSize focusedSize;
    for (const Client& client : m_model.shown()) {
        shown.push_back(client.window);
        if (std::ranges::find(m_shown, client.window) == m_shown.end())
            discard(c,
                xcb_composite_redirect_window_checked(
                    c, client.window, XCB_COMPOSITE_REDIRECT_AUTOMATIC));
        if (client.focused && !client.minimized) {
            focused = client.window;
            focusedSize = client.geometry.size();
        }
        if (client.pinned && !client.minimized)
            pinned.push_back(client.window);
    }
    m_shown = std::move(shown);
    follow(focused, focusedSize);
    watch(pinned);
    xcb_flush(c);
}

// Storage named before a resize keeps the old size and stops changing, so it is named again.
void Thumbnails::follow(quint32 window, QSize size)
{
    if (window == m_focused.window && size == m_focusedSize)
        return;
    if (window == m_focused.window)
        discard(m_x.xcb(), xcb_free_pixmap_checked(m_x.xcb(), m_focused.pixmap));
    else if (m_focused.window)
        queue(m_focused);
    m_focused = window ? Capture { window, name(window) } : Capture { };
    m_focusedSize = size;
}

void Thumbnails::watch(const std::vector<quint32>& windows)
{
    xcb_connection_t* c = m_x.xcb();
    std::erase_if(m_watched, [&](const auto& watched) {
        const bool gone = std::ranges::find(windows, watched.first) == windows.end();
        if (gone)
            discard(c, xcb_damage_destroy_checked(c, watched.second.damage));
        return gone;
    });
    for (quint32 window : windows) {
        if (m_watched.contains(window))
            continue;
        const quint32 damage = xcb_generate_id(c);
        discard(c, xcb_damage_create_checked(c, damage, window, XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY));
        m_watched.emplace(window, Watch { damage, false });
        queue({ window, 0 });
    }
}

void Thumbnails::handle(const xcb_generic_event_t* event)
{
    if (event->response_type != m_damageNotify)
        return;
    const auto* notify = reinterpret_cast<const xcb_damage_notify_event_t*>(event);
    const auto watched = std::ranges::find_if(m_watched,
        [notify](const auto& watched) { return watched.second.damage == notify->damage; });
    if (watched == m_watched.end())
        return;
    watched->second.damaged = true;
    if (!m_pace.isActive()) {
        captureDamaged();
        m_pace.start();
    }
}

// A damage object at the non-empty level reports once and then waits for its damage to be
// subtracted, so subtracting only here paces the events as well as the captures.
void Thumbnails::captureDamaged()
{
    bool captured = false;
    for (auto& [window, watched] : m_watched) {
        if (!watched.damaged)
            continue;
        discard(
            m_x.xcb(), xcb_damage_subtract_checked(m_x.xcb(), watched.damage, XCB_NONE, XCB_NONE));
        watched.damaged = false;
        captured = true;
        queue({ window, 0 });
    }
    if (!captured)
        m_pace.stop();
}

void Thumbnails::queue(const Capture& capture)
{
    const bool queued
        = capture.pixmap == 0 && std::ranges::any_of(m_queue, [&](const Capture& other) {
              return other.window == capture.window && other.pixmap == 0;
          });
    if (!queued)
        m_queue.push_back(capture);
    if (!m_next.isActive())
        m_next.start();
}

// Reading a large window is a round trip carrying megabytes, so one capture is taken for each
// turn of the event loop and other events are handled in between.
void Thumbnails::captureNext()
{
    if (m_queue.empty())
        return;
    const double started = traceClock();
    const Capture capture = m_queue.front();
    m_queue.pop_front();
    xcb_connection_t* c = m_x.xcb();
    const quint32 pixmap = capture.pixmap ? capture.pixmap : name(capture.window);
    const auto size = take(c, xcb_get_geometry_reply, xcb_get_geometry(c, pixmap));
    if (size) {
        const auto image = take(c, xcb_get_image_reply,
            xcb_get_image(
                c, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap, 0, 0, size->width, size->height, ~0u));
        if (image) {
            const QImage thumbnail = thumbnailFrom(*image, size->width, size->height);
            if (!thumbnail.isNull())
                m_model.setThumbnail(capture.window, thumbnail);
            qCInfo(traceX, "%.3f captured 0x%x at %dx%d in %.3f ms", traceClock(), capture.window,
                size->width, size->height, traceClock() - started);
        }
    }
    discard(c, xcb_free_pixmap_checked(c, pixmap));
    if (!m_queue.empty())
        m_next.start();
}

quint32 Thumbnails::name(quint32 window) const
{
    const quint32 pixmap = xcb_generate_id(m_x.xcb());
    discard(m_x.xcb(), xcb_composite_name_window_pixmap_checked(m_x.xcb(), window, pixmap));
    return pixmap;
}
