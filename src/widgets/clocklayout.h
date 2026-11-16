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

// The clock `width` wide with `time` at the left end and `date` at the right, in the family and
// weight of `family`.
ClockLayout clockLayout(int width, const QFont& family, const QString& time, const QString& date);
