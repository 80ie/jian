#pragma once

#include <QLoggingCategory>

// What --trace-x turns on: when X events arrive, when the window model updates and when the
// tasks repaint, enough to time a page switch.
Q_DECLARE_LOGGING_CATEGORY(traceX)

// Milliseconds on the monotonic clock, to line trace lines up with each other.
double traceClock();
