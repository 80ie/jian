# shelf

A desktop panel for X11 in C++20 and Qt 6 Widgets, replacing a Python panel
written for fvwm. `docs/spec.md` says what it does and the order it is built
in; `AGENTS.md` holds the rules for working on it.

    cmake --preset dev && cmake --build --preset dev && ctest --preset dev

`dev` builds with ASan and UBSan, `release` without. To see the painters:
`QT_QPA_PLATFORM=offscreen build/dev/shelf --render sheet.png`.
