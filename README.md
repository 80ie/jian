# shelf

A desktop panel for X11 in C++20 and Qt 6 Widgets, replacing a Python panel
written for fvwm. `docs/spec.md` says what it does and the order it is built
in; `AGENTS.md` holds the rules for working on it.

    cmake --preset dev && cmake --build --preset dev && ctest --preset dev

`dev` builds with ASan and UBSan, `release` without. `shelf` runs the panel,
reading `~/.config/shelf/config.ini` and `palette.ini`; `shelf --only clock`
runs one widget as a plain window. `shelf --settings` opens the settings
window, in the running panel if there is one. To draw the panel or the painters without
a display: `QT_QPA_PLATFORM=offscreen build/dev/shelf --render panel.png` or
`--render-sheet sheet.png`. `shelf --import-scheme scheme.json palette.ini`
converts one of the old wallust schemes.
