#pragma once

#include <QColor>
#include <QString>

#include <map>
#include <optional>

constexpr int darkestTone = -6;
constexpr int lightestTone = 5;
constexpr int darkestChrome = -2;
constexpr int lightestChrome = 1;

// How far the OKLab lightness of `tone` lies from the face's at contrast 1.
double lightnessOffset(int tone);

struct Palette {
    QColor face { 0xa2, 0x94, 0x8a };
    double contrast = 1;
    QColor ink { 0x02, 0x02, 0x00 };
    QColor fill { 0x58, 0x4a, 0x0c };
    QColor selection { 0x7a, 0x11, 0x0f };
    QColor pagerDesk { 0x5c, 0x30, 0x0e };
    // The window manager's, which only exports use.
    QColor chrome { 0x78, 0x34, 0x0a };
    std::map<int, QColor> overrides;

    // Tones run from darkestTone to lightestTone, the face at 0.
    QColor tone(int number) const;
    // A shorter ramp of the chrome, from darkestChrome to lightestChrome, the chrome at 0.
    QColor chromeTone(int number) const;

    bool operator==(const Palette&) const = default;
};

// `palette` with `tone` overridden by the face's complement at full chroma, to show on the panel
// where the tone is used.
Palette withToneShown(Palette palette, int tone);

std::optional<Palette> loadPalette(const QString& path);
bool savePalette(const Palette& palette, const QString& path);
