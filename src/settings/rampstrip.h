#pragma once

#include "palette/palette.h"

#include <QPixmap>
#include <QWidget>

#include <optional>

// "face" for tone 0, else the tone's signed number.
QString toneName(int tone);

// How big lists draw their palettes.
constexpr QSize stripSize(320, 24);

// A picture of `palette` `size` big: its tones darkest first and, after a gap, its named colors.
QPixmap paletteStrip(const Palette& palette, QSize size);

// The ramp as one row of swatches, darkest first, each numbered under it. The face is tone 0 and
// stands taller than the rest; an overridden tone carries a dot.
class RampStrip : public QWidget {
    Q_OBJECT

public:
    explicit RampStrip(QWidget* parent);

    void setColors(const Palette& palette);
    // Outlines the tone last chosen.
    void setCurrent(std::optional<int> tone);
    QSize sizeHint() const override;

signals:
    void clicked(int tone);
    void resetRequested(int tone);
    void hovered(std::optional<int> tone);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QRect swatch(int tone) const;
    std::optional<int> toneAt(QPoint pos) const;
    void hover(std::optional<int> tone);

    Palette m_palette;
    std::optional<int> m_current;
    std::optional<int> m_hovered;
};
