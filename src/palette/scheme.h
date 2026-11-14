#pragma once

#include "palette/palette.h"

#include <QString>

#include <optional>

// A wallust scheme, as the old palette tooling kept them in palette/schemes/*.json, made into a
// palette by the mapping its roles.j2 used (spec 8.3).
std::optional<Palette> importScheme(const QString& path);
