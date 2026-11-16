#include "paint/sheet.h"
#include "paint/bevel.h"
#include "palette/palette.h"

#include <QPainter>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {

constexpr int sheetWidth = 180;
constexpr int margin = 6;
constexpr int labelX = paint::frameWidth + margin;
constexpr int sampleX = 64;
constexpr int sampleWidth = sheetWidth - margin - sampleX;
constexpr int lineHeight = 14;
constexpr int rowGap = 6;
constexpr int swatchWidth = 13;
constexpr int swatchHeight = 20;
constexpr int rampHeight = 2 + swatchHeight + 2 + lineHeight;

struct Sample {
    QString name;
    QSize size;
    void (*paint)(QPainter&, const QRect&, const Palette&);
};

int rowHeight(const Sample& sample) { return std::max(sample.size.height(), lineHeight); }

void well(QPainter& p, const QRect& r, const Palette& palette)
{
    paint::sunken(p, r, palette, palette.tone(-1));
}

void meter(QPainter& p, const QRect& r, const Palette& palette)
{
    paint::trough(p, r, palette);
    paint::bar(
        p, QRect(r.left() + 1, r.top() + 1, (r.width() - 2) * 3 / 5, r.height() - 3), palette.fill);
}

void buttons(QPainter& p, const QRect& r, const Palette& palette)
{
    const int width = (r.width() - 2) / 2;
    paint::button(p, QRect(r.left(), r.top(), width, r.height()), palette, false);
    paint::button(p, QRect(r.right() + 1 - width, r.top(), width, r.height()), palette, true);
}

void scale(QPainter& p, const QRect& r, const Palette& palette)
{
    for (int i = 0; i < 5; ++i)
        paint::tick(p, { r.left() + i * (r.width() - 2) / 4, r.top() }, palette);
}

void ramp(QPainter& p, int y, const Palette& palette)
{
    const QRect swatches(
        labelX + 2, y + 2, swatchWidth * (lightestTone - darkestTone + 1), swatchHeight);
    paint::sunken(p, swatches.adjusted(-2, -2, 2, 2), palette, palette.tone(0));
    p.setPen(palette.ink);
    for (int tone = darkestTone; tone <= lightestTone; ++tone) {
        const int x = swatches.left() + (tone - darkestTone) * swatchWidth;
        p.fillRect(x, swatches.top(), swatchWidth, swatchHeight, palette.tone(tone));
        p.drawText(QRect(x, swatches.bottom() + 3, swatchWidth, lineHeight), Qt::AlignCenter,
            QString::number(tone));
    }
}

} // namespace

QImage paint::sheet(const Palette& palette)
{
    const Sample samples[] {
        { u"raised"_s, { sampleWidth, 20 }, raised },
        { u"sunken"_s, { sampleWidth, 20 }, well },
        { u"trough"_s, { sampleWidth, 16 }, trough },
        { u"bar"_s, { sampleWidth, 16 }, meter },
        { u"groove"_s, { sampleWidth, 5 }, groove },
        { u"button"_s, { sampleWidth, 22 }, buttons },
        { u"thumb"_s, { 10, 17 }, thumb },
        { u"divider"_s, { sampleWidth, 2 }, divider },
        { u"tick"_s, { sampleWidth, 2 }, scale },
    };
    int height = margin + rampHeight + margin;
    for (const Sample& sample : samples)
        height += rowHeight(sample) + rowGap;

    QImage image(sheetWidth, height, QImage::Format_RGB32);
    QPainter p(&image);
    p.fillRect(image.rect(), palette.tone(0));
    frame(p, QRect(0, 0, frameWidth, height), palette, Qt::RightEdge);

    // Spelled out in full, or the golden image would follow the desktop's font settings.
    QFont font(u"DejaVu Sans"_s);
    font.setPixelSize(10);
    font.setWeight(QFont::Normal);
    p.setFont(font);

    int y = margin;
    for (const Sample& sample : samples) {
        const int row = rowHeight(sample);
        p.setPen(palette.ink);
        p.drawText(
            QRect(labelX, y, sampleX - labelX, row), Qt::AlignLeft | Qt::AlignVCenter, sample.name);
        const QPoint at(sampleX, y + (row - sample.size.height()) / 2);
        sample.paint(p, QRect(at, sample.size), palette);
        y += row + rowGap;
    }
    ramp(p, y, palette);
    p.end();
    return image;
}
