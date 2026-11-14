#include "x11/trace.h"

#include <chrono>

Q_LOGGING_CATEGORY(traceX, "shelf.trace-x", QtWarningMsg)

double traceClock()
{
    using Milliseconds = std::chrono::duration<double, std::milli>;
    return Milliseconds(std::chrono::steady_clock::now().time_since_epoch()).count();
}
