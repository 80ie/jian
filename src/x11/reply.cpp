#include "x11/reply.h"

std::span<const quint32> values(const PropertyReply& reply)
{
    if (!reply || reply->format != 32)
        return { };
    return { static_cast<const quint32*>(xcb_get_property_value(reply.get())),
        std::size_t(xcb_get_property_value_length(reply.get())) / sizeof(quint32) };
}

QByteArrayView bytes(const PropertyReply& reply)
{
    if (!reply || reply->format != 8)
        return { };
    return { static_cast<const char*>(xcb_get_property_value(reply.get())),
        xcb_get_property_value_length(reply.get()) };
}
