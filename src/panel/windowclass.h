#pragma once

#include <QString>
#include <qwindowdefs.h>

// Sets WM_CLASS of `window`, which Qt has made and has yet to map, to `instance` and "Shelf", so
// that a window manager rule can pick it out. Without X it does nothing.
void setWindowClass(WId window, const QString& instance);
