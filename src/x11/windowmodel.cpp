#include "x11/windowmodel.h"
#include "x11/connection.h"
#include "x11/reply.h"
#include "x11/trace.h"

#include <QIcon>

#include <algorithm>
#include <ranges>

namespace {

constexpr int iconSize = 32;
// A change of page or focus arrives as events spread over a millisecond or two, which one update
// should take in together.
constexpr int gatherMs = 5;
constexpr quint32 clientEvents = XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
// In 32-bit units: as much as any property holds.
constexpr quint32 wholeProperty = 1 << 24;
// ICCCM 4.1.4, and the EWMH source indication of a pager.
constexpr quint32 iconicState = 3;
constexpr quint32 fromPager = 2;
constexpr quint32 removeState = 0;
constexpr quint32 addState = 1;

xcb_get_property_cookie_t requestProperty(xcb_connection_t* c, quint32 window, xcb_atom_t atom)
{
    return xcb_get_property(c, false, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, wholeProperty);
}

bool contains(std::span<const quint32> values, quint32 value)
{
    return std::ranges::find(values, value) != values.end();
}

quint32 first(const PropertyReply& reply, quint32 otherwise)
{
    const std::span<const quint32> all = values(reply);
    return all.empty() ? otherwise : all.front();
}

QString title(const PropertyReply& netName, const PropertyReply& name)
{
    if (const QByteArrayView text = bytes(netName); !text.isEmpty())
        return QString::fromUtf8(text);
    if (name && name->type == XCB_ATOM_STRING)
        return QString::fromLatin1(bytes(name));
    return QString::fromUtf8(bytes(name));
}

QImage themeIcon(const Client& client)
{
    for (const QString& name : { client.windowClass, client.windowClass.toLower(), client.instance,
             client.instance.toLower() }) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!name.isEmpty() && !icon.isNull())
            return icon.pixmap(QSize(iconSize, iconSize), 1).toImage();
    }
    return QImage();
}

// The window may have gone, and its error is dropped here rather than reported by Qt.
void selectEvents(xcb_connection_t* c, quint32 window, quint32 events)
{
    xcb_discard_reply(
        c, xcb_change_window_attributes_checked(c, window, XCB_CW_EVENT_MASK, &events).sequence);
}

// Selecting events on one of the panel's own windows would replace the mask Qt has there.
bool ours(xcb_connection_t* c, quint32 window)
{
    const xcb_setup_t* setup = xcb_get_setup(c);
    return (window & ~setup->resource_id_mask) == setup->resource_id_base;
}

} // namespace

WindowModel::WindowModel(XConnection& x)
    : m_x(x)
    , m_screen(0, 0, x.screen().width_in_pixels, x.screen().height_in_pixels)
{
    m_update.setSingleShot(true);
    m_update.setInterval(gatherMs);
    connect(&m_update, &QTimer::timeout, this, &WindowModel::update);
    connect(&x, &XConnection::received, this, &WindowModel::handle);
    const Atoms& atoms = x.atoms();
    m_staleRoot
        = { atoms.clientList, atoms.clientListStacking, atoms.activeWindow, atoms.currentDesktop };
    update();
}

void WindowModel::setThumbnail(quint32 window, const QImage& thumbnail)
{
    if (Client* client = find(window)) {
        client->thumbnail = thumbnail;
        schedule();
    }
}

// Focus and stacking come apart under click to focus, where a window can keep the focus while
// buried, and minimizing on focus alone would put away a window the click meant to raise.
void WindowModel::toggle(quint32 window) const
{
    const auto client = std::ranges::find(m_shown, window, &Client::window);
    if (client == m_shown.end())
        return;
    const auto shown = [this](quint32 w) {
        return std::ranges::find(m_shown, w, &Client::window) != m_shown.end();
    };
    const auto stacked = m_stacking | std::views::reverse;
    const auto top = std::ranges::find_if(stacked, shown);
    if (client->focused && !client->minimized && top != stacked.end() && *top == window)
        minimize(window);
    else
        activate(window);
}

void WindowModel::activate(quint32 window) const
{
    send(window, m_x.atoms().activeWindow, { fromPager, XCB_CURRENT_TIME, 0, 0, 0 });
}

void WindowModel::minimize(quint32 window) const
{
    send(window, m_x.atoms().wmChangeState, { iconicState, 0, 0, 0, 0 });
}

void WindowModel::close(quint32 window) const
{
    send(window, m_x.atoms().closeWindow, { XCB_CURRENT_TIME, fromPager, 0, 0, 0 });
}

void WindowModel::setMaximized(quint32 window, bool maximized) const
{
    const Atoms& atoms = m_x.atoms();
    send(window, atoms.wmState,
        { maximized ? addState : removeState, atoms.stateMaximizedVert, atoms.stateMaximizedHorz,
            fromPager, 0 });
}

void WindowModel::setPinned(quint32 window, bool pinned) const
{
    const Atoms& atoms = m_x.atoms();
    send(window, atoms.wmState,
        { pinned ? addState : removeState, atoms.stateSticky, 0, fromPager, 0 });
}

void WindowModel::handle(const xcb_generic_event_t* event)
{
    const Atoms& atoms = m_x.atoms();
    const quint32 root = m_x.screen().root;
    switch (event->response_type & ~0x80) {
    case XCB_CONFIGURE_NOTIFY: {
        const auto* configure = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
        if (configure->window == root) {
            m_screen = QRect(0, 0, configure->width, configure->height);
            schedule();
            break;
        }
        Client* client = find(configure->window);
        if (!client)
            break;
        const bool synthetic = event->response_type & 0x80;
        qCInfo(traceX, "%.3f configure 0x%x %d,%d %s", traceClock(), configure->window,
            configure->x, configure->y, synthetic ? "synthetic" : "real");
        // ICCCM 4.1.5: a window manager that moves a client tells it where in root coordinates
        // with a synthetic event. A real one has coordinates within the frame.
        if (synthetic) {
            const int border = 2 * configure->border_width;
            client->geometry = QRect(
                configure->x, configure->y, configure->width + border, configure->height + border);
        } else {
            m_unplaced.insert(client->window);
        }
        schedule();
        break;
    }
    case XCB_REPARENT_NOTIFY: {
        const auto* reparent = reinterpret_cast<const xcb_reparent_notify_event_t*>(event);
        if (find(reparent->window)) {
            m_unplaced.insert(reparent->window);
            schedule();
        }
        break;
    }
    case XCB_PROPERTY_NOTIFY: {
        const auto* change = reinterpret_cast<const xcb_property_notify_event_t*>(event);
        if (change->window == root) {
            const std::array watched { atoms.clientList, atoms.clientListStacking,
                atoms.activeWindow, atoms.currentDesktop };
            if (contains(watched, change->atom)) {
                qCInfo(traceX, "%.3f root property %u", traceClock(), change->atom);
                m_staleRoot.insert(change->atom);
                schedule();
            }
            break;
        }
        if (!find(change->window))
            break;
        for (Field field : { Field::Title, Field::Icon, Field::State, Field::Desktop }) {
            if (contains(atomsOf(field), change->atom)) {
                qCInfo(traceX, "%.3f property 0x%x %u", traceClock(), change->window, change->atom);
                m_stale.emplace(change->window, field);
                schedule();
            }
        }
        break;
    }
    }
}

void WindowModel::schedule()
{
    if (!m_update.isActive())
        m_update.start();
}

// Every request goes out before any reply is waited for, so a pass costs one round trip, and
// one more if windows were added.
void WindowModel::update()
{
    const double started = traceClock();
    const std::vector<RootRequest> root = requestRoot();
    const std::vector<FieldRequest> fields = requestFields(m_stale);
    const std::vector<PlaceRequest> places = requestPlaces(m_unplaced);
    m_stale.clear();
    m_unplaced.clear();
    const std::vector<quint32> added = applyRoot(root);
    applyFields(fields);
    applyPlaces(places);
    add(added);
    const int roundTrips = !(root.empty() && fields.empty() && places.empty()) + !added.empty();

    std::vector<Client> shown;
    for (Client& client : m_clients) {
        client.focused = client.window == m_active;
        if (isShown(client, m_screen, m_desktop))
            shown.push_back(client);
    }
    const bool differs = shown != m_shown;
    qCInfo(traceX, "%.3f update in %.3f ms, %d round trips, %zu shown%s", traceClock(),
        traceClock() - started, roundTrips, shown.size(), differs ? ", changed" : "");
    if (differs) {
        m_shown = std::move(shown);
        emit changed();
    }
}

std::vector<WindowModel::RootRequest> WindowModel::requestRoot()
{
    std::vector<RootRequest> requests;
    for (xcb_atom_t atom : m_staleRoot)
        requests.push_back({ atom, requestProperty(m_x.xcb(), m_x.screen().root, atom) });
    m_staleRoot.clear();
    return requests;
}

// Returns the windows the client list gained.
std::vector<quint32> WindowModel::applyRoot(const std::vector<RootRequest>& requests)
{
    const Atoms& atoms = m_x.atoms();
    std::vector<quint32> added;
    for (const RootRequest& request : requests) {
        const PropertyReply reply = take(m_x.xcb(), xcb_get_property_reply, request.cookie);
        if (request.atom == atoms.activeWindow) {
            m_active = first(reply, XCB_NONE);
        } else if (request.atom == atoms.currentDesktop) {
            m_desktop = first(reply, 0);
        } else if (request.atom == atoms.clientListStacking) {
            const std::span<const quint32> stacking = values(reply);
            m_stacking.assign(stacking.begin(), stacking.end());
        } else if (request.atom == atoms.clientList) {
            std::vector<Client> clients;
            for (quint32 window : values(reply)) {
                if (Client* client = find(window)) {
                    clients.push_back(std::move(*client));
                } else if (!ours(m_x.xcb(), window)) {
                    clients.emplace_back().window = window;
                    added.push_back(window);
                }
            }
            m_clients = std::move(clients);
        }
    }
    return added;
}

std::vector<WindowModel::FieldRequest> WindowModel::requestFields(
    const std::set<std::pair<quint32, Field>>& fields)
{
    std::vector<FieldRequest> requests;
    for (const auto& [window, field] : fields) {
        FieldRequest& request = requests.emplace_back(window, field);
        const std::array<xcb_atom_t, 2> atoms = atomsOf(field);
        for (std::size_t i = 0; i < atoms.size(); ++i)
            if (atoms[i] != XCB_ATOM_NONE)
                request.cookies[i] = requestProperty(m_x.xcb(), window, atoms[i]);
    }
    return requests;
}

void WindowModel::applyFields(const std::vector<FieldRequest>& requests)
{
    const Atoms& atoms = m_x.atoms();
    for (const FieldRequest& request : requests) {
        const PropertyReply reply = take(m_x.xcb(), xcb_get_property_reply, request.cookies[0]);
        PropertyReply second;
        if (atomsOf(request.field)[1] != XCB_ATOM_NONE)
            second = take(m_x.xcb(), xcb_get_property_reply, request.cookies[1]);
        Client* client = find(request.window);
        if (!client)
            continue;
        switch (request.field) {
        case Field::Title:
            client->title = title(reply, second);
            break;
        case Field::Icon: {
            // The instance and then the class, each ending in a NUL.
            const QList<QByteArray> names = bytes(second).toByteArray().split('\0');
            client->instance = QString::fromLatin1(names.value(0));
            client->windowClass = QString::fromLatin1(names.value(1));
            client->icon = decodeIcon(values(reply), iconSize);
            if (client->icon.isNull())
                client->icon = themeIcon(*client);
            break;
        }
        case Field::State: {
            const std::span<const quint32> state = values(reply);
            const auto has = [state](xcb_atom_t atom) { return contains(state, atom); };
            if (has(atoms.stateHidden) && !client->minimized)
                client->minimizedOrder = ++m_minimizations;
            client->minimized = has(atoms.stateHidden);
            client->maximized = has(atoms.stateMaximizedVert) && has(atoms.stateMaximizedHorz);
            client->pinned = has(atoms.stateSticky);
            client->skipTaskbar = has(atoms.stateSkipTaskbar);
            break;
        }
        case Field::Desktop:
            client->desktop = first(reply, allDesktops);
            break;
        }
    }
}

std::vector<WindowModel::PlaceRequest> WindowModel::requestPlaces(const std::set<quint32>& windows)
{
    std::vector<PlaceRequest> requests;
    for (quint32 window : windows)
        requests.push_back({ window, xcb_get_geometry(m_x.xcb(), window),
            xcb_translate_coordinates(m_x.xcb(), window, m_x.screen().root, 0, 0) });
    return requests;
}

void WindowModel::applyPlaces(const std::vector<PlaceRequest>& requests)
{
    for (const PlaceRequest& request : requests) {
        const auto size = take(m_x.xcb(), xcb_get_geometry_reply, request.size);
        const auto origin = take(m_x.xcb(), xcb_translate_coordinates_reply, request.origin);
        const auto client = std::ranges::find(m_clients, request.window, &Client::window);
        if (client == m_clients.end())
            continue;
        if (!size || !origin) {
            m_clients.erase(client);
            continue;
        }
        const int border = 2 * size->border_width;
        client->geometry = QRect(origin->dst_x - size->border_width,
            origin->dst_y - size->border_width, size->width + border, size->height + border);
    }
}

// Events are selected before anything is read, so no change falls between.
void WindowModel::add(const std::vector<quint32>& windows)
{
    std::set<std::pair<quint32, Field>> fields;
    for (quint32 window : windows) {
        selectEvents(m_x.xcb(), window, clientEvents);
        for (Field field : { Field::Title, Field::Icon, Field::State, Field::Desktop })
            fields.emplace(window, field);
    }
    const std::vector<FieldRequest> fieldRequests = requestFields(fields);
    const std::vector<PlaceRequest> placeRequests
        = requestPlaces({ windows.begin(), windows.end() });
    applyFields(fieldRequests);
    applyPlaces(placeRequests);
}

std::array<xcb_atom_t, 2> WindowModel::atomsOf(Field field) const
{
    const Atoms& atoms = m_x.atoms();
    switch (field) {
    case Field::Title:
        return { atoms.wmName, XCB_ATOM_WM_NAME };
    case Field::Icon:
        return { atoms.wmIcon, XCB_ATOM_WM_CLASS };
    case Field::State:
        return { atoms.wmState, XCB_ATOM_NONE };
    case Field::Desktop:
        return { atoms.wmDesktop, XCB_ATOM_NONE };
    }
    return { };
}

void WindowModel::send(quint32 window, xcb_atom_t type, std::array<quint32, 5> data) const
{
    xcb_client_message_event_t message { };
    message.response_type = XCB_CLIENT_MESSAGE;
    message.format = 32;
    message.window = window;
    message.type = type;
    std::ranges::copy(data, message.data.data32);
    xcb_send_event(m_x.xcb(), false, m_x.screen().root,
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
        reinterpret_cast<const char*>(&message));
    xcb_flush(m_x.xcb());
}

Client* WindowModel::find(quint32 window)
{
    const auto client = std::ranges::find(m_clients, window, &Client::window);
    return client == m_clients.end() ? nullptr : &*client;
}
