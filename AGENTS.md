# Working on jian

A desktop panel in C++20 and Qt 6 Widgets, on X11 through xcb. `docs/spec.md`
is the source of truth for what it does and in what order it gets built.
`CLAUDE.md` is a symlink to this file.

The Python program this replaced is in the history of `~/.fvwm`:
`git -C ~/.fvwm show 92b16fe^:bin/shelf-panel`, and likewise `photon*.py`.
Read it for reference. Never modify anything under `~/.fvwm`.

## Approach

"For each desired change, make the change easy (warning: this may be hard),
then make the easy change." (Kent Beck)

When a change fights the code, first reshape the code until the change is
small, in a commit of its own that keeps behavior as it was, then make it.
The same holds for the user: where a setting can be one general mechanism,
such as a format string, that they combine for themselves, prefer it to an
option per need.

## Build and test

    cmake --preset dev && cmake --build --preset dev && ctest --preset dev

`dev` builds with ASan and UBSan and treats warnings as errors. A commit must
build clean and pass under it.

## Scope

- Build what the spec says, nothing more. No options, hooks or parameters
  for needs that do not exist yet.
- If the code has to depart from the spec, change the spec in the same
  commit and say why in the message.
- Items marked OPEN or VERIFY in the spec are not yours to decide quietly.
  Test or ask, then record the result in the spec.

## Code

- The simplest thing that is correct. Prefer a function to a class, a value
  to a pointer, the standard library or Qt to anything hand-rolled.
- No interface with a single implementation, no wrapper that only forwards,
  no manager, helper or util grab-bags. Name a thing for what it is.
- One owner for every object. QObjects are owned by their parent; everything
  else is a value or a `std::unique_ptr`. No `shared_ptr`, no bare `new`
  without a parent, no globals or singletons.
- No exceptions, no macros, no `using namespace` in headers.
- Check what can fail where it enters the program: X replies, files, D-Bus,
  subprocesses. Handle it or `qWarning` once and carry on. Do not check for
  things that cannot happen.
- Nothing blocks in an event handler. No synchronous X round trip per
  window, no timer that polls for something that has a signal.
- Colors come from the palette as tones or named colors. No color literals
  outside `src/palette/` and tests.
- Functions fit on a screen. A file past about 400 lines wants splitting.
- Names: `PascalCase` types, `camelCase` functions and variables, `m_`
  members, `#pragma once`. Formatting is whatever `clang-format` produces.

## Comments

Most code needs none. Write one only for a reason the code cannot show: why
this way and not the obvious way, or a fact about X or Qt that bit us. Three
lines at most. No comment that restates the line below it, no banners, no
history ("now uses", "previously"), no notes addressed to a reviewer.

## Words on screen

A label is a noun or two: `Widgets`, `Candidates`, `Contrast`. A button is
a verb or two: `Reset`, `Open…`. No sentences, no explanatory paragraphs,
no hint text telling the user how the program works. If a control needs a
paragraph to be understood, the control is wrong; fix it or leave a tooltip
of one short line. Do not show raw values (hex codes, ids) where a swatch,
a mark or a name says it better.

## Tests

Pure logic (ramp, layout, file round trips, ordering) gets a Qt Test.
A bug fix starts with a test that fails. Do not test Qt, and do not write
tests that only repeat the implementation.

## Commits

One change per commit, working tree green. Subject in lower case, imperative,
`area: what changed`, under 60 characters. Add a body only when the reason
is not obvious, and keep it to a few plain sentences. No bullet lists of
files touched, no summaries of the diff.
