#pragma once

#include <QFont>
#include <QPoint>
#include <QString>

#include <vector>

struct ClockLine {
    QString text;
    // The left end of the baseline.
    QPoint origin;
};

// One end of the clock, in one font.
struct ClockSide {
    QFont font;
    std::vector<ClockLine> lines;
};

struct ClockLayout {
    ClockSide time;
    ClockSide date;
    int height = 0;
};

// Each end's size, as a percentage of the size the width gives.
struct ClockSizes {
    int time = 100;
    int date = 100;

    bool operator==(const ClockSizes&) const = default;
};

// The clock `width` wide with `time` at the left end and `date` at the right, each broken into
// lines at its newlines and sized by `sizes`, in the family and weight of `family`.
ClockLayout clockLayout(int width, const QFont& family, const QString& time, const QString& date,
    const ClockSizes& sizes);
