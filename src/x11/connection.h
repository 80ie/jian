#pragma once

#include "x11/atoms.h"

#include <QAbstractNativeEventFilter>
#include <QObject>

#include <memory>

// Qt's own connection to the X server, shared by everything that talks to X, and the one place
// its events come in.
class XConnection : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    // Nothing unless Qt runs on xcb.
    static std::unique_ptr<XConnection> open();
    XConnection(xcb_connection_t* connection, const xcb_screen_t& screen, const Atoms& atoms);

    xcb_connection_t* xcb() const { return m_connection; }
    const xcb_screen_t& screen() const { return m_screen; }
    const Atoms& atoms() const { return m_atoms; }
    // Whether `window` was created on this connection, which is to say by Qt for the panel.
    bool owns(quint32 window) const;

signals:
    // Every event, Qt's included, before Qt handles it.
    void received(const xcb_generic_event_t* event);

protected:
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;

private:
    xcb_connection_t* m_connection;
    const xcb_screen_t& m_screen;
    Atoms m_atoms;
};
