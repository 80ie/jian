#pragma once

#include <QByteArrayView>

#include <xcb/xcb.h>

#include <cstdlib>
#include <memory>
#include <span>

struct FreeReply {
    void operator()(void* reply) const { std::free(reply); }
};

// An xcb reply, which xcb allocates with malloc.
template <typename T> using Reply = std::unique_ptr<T, FreeReply>;
using PropertyReply = Reply<xcb_get_property_reply_t>;

// The reply to `cookie`, or null if the request failed, as one about another client's window can
// at any moment. The error is taken here so that Qt does not report it.
template <typename T, typename Cookie>
Reply<T> take(xcb_connection_t* c, T* (*reply)(xcb_connection_t*, Cookie, xcb_generic_error_t**),
    Cookie cookie)
{
    xcb_generic_error_t* error = nullptr;
    Reply<T> result(reply(c, cookie, &error));
    std::free(error);
    return result;
}

// Drops the error, if any, of a request on another client's window, which may have gone, so
// that Qt does not report it.
inline void discard(xcb_connection_t* c, xcb_void_cookie_t cookie)
{
    xcb_discard_reply(c, cookie.sequence);
}

// A property's items, or nothing if it is missing or has the other format.
std::span<const quint32> values(const PropertyReply& reply);
QByteArrayView bytes(const PropertyReply& reply);
