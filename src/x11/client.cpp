#include "x11/client.h"

#include <cstring>

bool isShown(const Client& client, const QRect& screen, quint32 desktop)
{
    return !client.skipTaskbar && (client.desktop == desktop || client.desktop == allDesktops)
        && client.geometry.intersects(screen);
}

QImage decodeIcon(std::span<const quint32> value, int size)
{
    // Down to `size`, a narrower image is nearer; below it, a wider one.
    const auto nearer = [size](quint32 width, quint32 than) {
        return than < quint32(size) ? width > than : width >= quint32(size) && width < than;
    };
    std::span<const quint32> best;
    quint32 bestWidth = 0;
    while (value.size() >= 2) {
        const quint32 width = value[0];
        const quint32 height = value[1];
        value = value.subspan(2);
        if (width == 0 || height == 0 || quint64(width) * height > value.size())
            break;
        if (best.empty() || nearer(width, bestWidth)) {
            best = value.first(width * height);
            bestWidth = width;
        }
        value = value.subspan(width * height);
    }
    if (best.empty())
        return QImage();
    const int width = int(bestWidth);
    const int height = int(best.size() / bestWidth);
    QImage image(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y)
        std::memcpy(image.scanLine(y), best.data() + y * width, width * sizeof(quint32));
    return image;
}
