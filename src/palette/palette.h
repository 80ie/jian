#pragma once

#include <QColor>

#include <map>

constexpr int darkestTone = -6;
constexpr int lightestTone = 5;

struct Palette {
    QColor face { 0xa2, 0x94, 0x8a };
    double contrast = 1;
    QColor ink { 0x02, 0x02, 0x00 };
    QColor fill { 0x58, 0x4a, 0x0c };
    QColor selection { 0x7a, 0x11, 0x0f };
    QColor pagerDesk { 0x5c, 0x30, 0x0e };
    std::map<int, QColor> overrides;

    // Tones run from darkestTone to lightestTone, the face at 0.
    QColor tone(int number) const;

    bool operator==(const Palette&) const = default;
};
