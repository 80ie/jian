# Shelf rewrite: spec

Status: draft 2, 2026-11-13. Nothing here is built. Items marked **OPEN** need
a decision; items marked **VERIFY** are claims to test before relying on them.

## 1. Goals

1. A panel that is an application in its own right: it owns its configuration,
   its palette and its settings GUI, and runs without fvwm.
2. Code a person can read top to bottom. C++20, Qt 6 Widgets, xcb. No
   generators, no templating engine, no environment variables as a data
   channel.
3. Palettes are suggestions. Generation proposes, the user edits, and every
   color on screen traces back to one line in one file.
4. Widgets are self-contained units that can be rearranged, removed and
   popped out at runtime.
5. Parity or better for the widgets being ported. No new widgets in this
   round.

Non-goals for this round: the dockapp tray, a new system monitor, notes,
pomodoro, calendar, the Winamp-style player skin and analyzer. Section 12
records them so the architecture leaves room, and nothing more.

## 2. Scope

| current piece | fate |
|---|---|
| `shelf-panel` frame, layout, resize, side switch | ported, layout model replaced (section 4) |
| clock | ported |
| World View (FvwmPager swallow) | ported as an fvwm-only widget (section 7.2) |
| tasks | ported, pinned grid and overflow redesigned (section 7.3) |
| media | ported, plus trackbar and source selector (section 7.4) |
| tray (stalonetray swallow) | ported (section 7.5) |
| meters | dropped |
| launcher rows (`item` lines, all commented out today) | dropped for now |
| group headers and collapsing | removed, replaced by edit mode (section 5) |
| `bin/palette`, `palette/templates/`, `roles.j2`, wallust | replaced by the palette model, editor, in-process extraction and exports (sections 8, 9) |
| `lib/Thumbnail` as the tasks thumbnail source | replaced by capture in the panel (section 7.3) |
| `photon_palette.py` reading `colorsets` | removed; the panel never reads fvwm files |
| `shelf-snapshot`, `palette sweep` | replaced by an offscreen render mode and the editor's candidate view |

## 3. Architecture

One process. Built-in widgets are `QWidget` subclasses living in it, sharing
one palette object, one set of painters and one X connection. This is what
makes pop-out cheap: a popped-out widget is the same object reparented to a
top-level window, with no second process and no IPC.

Windows that belong to other programs (the pager, the tray, later dockapps)
go through one class, `ForeignSlot`, which wraps `QWindow::fromWinId` and
`createWindowContainer` and handles finding the window, reparenting it,
resizing it and noticing when it dies. The current code does this twice,
separately, in `photon_pager.py` and `photon_tray.py`.

### 3.1 Source layout

Its own repository under `~/projects/`, name undecided (`<name>` below).

```
src/
  main.cpp            argument parsing, single-instance check, startup
  panel/              Panel window, slot layout, edit mode, pop-out, struts
  palette/            Palette model, ramp, file I/O, extraction, export
  paint/              the bevel painters from photon.py, themed icons
  x11/                xcb connection, atoms, event filter, WindowModel
  widgets/            clock, tasks, media, pager, tray, ForeignSlot
  settings/           the settings window and palette editor
tests/                Qt Test, golden images
```

### 3.2 X connection

Use Qt's own xcb connection (`QNativeInterface::QX11Application`) and receive
events through one `QAbstractNativeEventFilter`. No second connection and no
`QSocketNotifier`. Qt already selects events on the root window and event
masks are per client, so the filter must OR its bits into the existing mask
rather than replace it.

Verified in `fvwm-rig` (Qt 6.11.3): bits OR-ed into Qt's root mask (enter,
leave, `StructureNotify`, `PropertyChange`) bring their events to the
filter, but `SubstructureNotify` on the root cannot follow frames. Qt drops
a `ConfigureNotify` when a later one with the same `event` window is
queued, and for a substructure event that window is the root, so a page
switch moving ten frames delivered one or two of them: 8 of 43 over four
switches. Qt also re-reads the RandR screens for every `ConfigureNotify`
whose `event` is the root. Turning off `Qt::AA_CompressHighFrequencyEvents`
brought all 43 through, at the cost of uncompressed pointer motion for the
whole program. So the window model selects `StructureNotify` on each frame
it tracks instead, where compression only merges moves of one frame, and the
root needs only `PropertyChange`.

### 3.3 The widget contract

```cpp
class ShelfWidget : public QWidget {
public:
    virtual QString id() const = 0;                // stable, used in config
    int heightForWidth(int w) const override = 0;  // natural height
    virtual int minimumHeightForWidth(int w) const;
    virtual bool wantsSpare() const;               // takes leftover height
signals:
    void heightChanged();
};
```

`settingsPage()` joins the contract with the settings window (milestone 7);
until there is a window to put a page in, nothing would call it.

A widget owns nothing outside its own rectangle, and does not know whether it
is docked or popped out. The existing rule carries over: a widget does not
talk to fvwm. The one exception is the pager, which is an fvwm module by
definition and keeps all fvwm traffic inside its own file.

### 3.4 Configuration

`~/.config/<name>/`:

```
config.ini          panel side, width, widget order, per-widget settings
palette.ini         the live palette (section 8)
palettes/           named presets, same format
history/            timestamped copies of palette.ini, written on each save
exports/            export targets (section 9)
```

Every file is written by the GUI and editable by hand. A file changed on
disk is picked up through `QFileSystemWatcher`. `.panel.state` and
`panel.items` merge into `config.ini`.

**OPEN**: INI through `QSettings` (no dependency, but the GUI rewrites the
file and drops comments) or TOML through a vendored header (toml++). Draft
assumes INI.

## 4. Layout

The panel is an ordered list of slots, one widget per slot. For a given
width:

1. Every widget gets its natural height.
2. Spare height goes to widgets with `wantsSpare()`, split evenly. Today that
   is tasks alone.
3. If the naturals exceed the panel, widgets shrink toward their minimum,
   those that want spare first.
4. If the minimums still exceed the panel, the bottom slots are clipped and
   edit mode flags them. The panel does not scroll.

The `flex` directive and the pinned-to-bottom rule disappear: slots above a
spare-taking widget sit at the top, slots below it sit at the bottom, as a
consequence of step 2.

Width stays resizable by dragging the inner edge. The panel types its window
`_NET_WM_WINDOW_TYPE_DOCK` and sets the strut on it as
`_NET_WM_STRUT_PARTIAL` and `_NET_WM_STRUT`.

Verified in `fvwm-rig` (fvwm3 1.1.5, 1600x900, `EwmhBaseStruts 0 0 0 0`):
fvwm3 honors the strut and follows it live. A window maximized after each
change spanned x 0 to 1439 with the panel 160 wide, 0 to 1359 after dragging
it to 240, 0 to 1399 after editing `config.ini` to 200, and 200 to 1599
after moving it to the left. A window maximized before a change keeps its
size until it is maximized again, as with `EwmhBaseStruts`. fvwm3 leaves
`_NET_WORKAREA` at the full screen throughout. So `EwmhBaseStruts`,
`--print-struts` and `--adjust-width` leave `config`, and `config.ini` needs
no strut command.

fvwm makes a dock sticky but still gives it a title, and `config` sets
`Style * NoDecorHint`, which ignores the frameless hint Qt sets. `config`
therefore keeps a `Style shelf !Title, !Handles, BorderWidth 0` line in
place of the `ShelfPanel` one.

## 5. Widget separation and edit mode

Headers go. Each slot is drawn as its own unit, with a strip of panel face
between neighbors, so that a widget reads as an object sitting in the panel
and not as a region of it. The exact treatment (raised tile, sunken bay,
spacing) is a visual design task: build two or three variants behind a flag,
screenshot them in the rig and pick. **OPEN** until those exist.

Edit mode is entered from the panel's context menu or the settings window.
While it is on:

- each slot shows an overlay with its name, a drag grip, a pop-out button and
  a remove button;
- dragging the grip reorders slots, with a line marking the insertion point;
- a strip at the bottom lists widgets not currently placed, to drag in;
- widget input is suspended, so a click on the pager does not change page.

Leaving edit mode writes `config.ini`. Collapse does not survive as a
separate feature: removing a widget is the same operation, and it comes back
from the strip.

Pop-out reparents the widget to a normal top-level window with its own
`WM_CLASS` instance (`<name>-media` and so on), so a window manager rule can
target it. Its position is remembered. Closing the window docks it back.

## 6. Window model

`x11/WindowModel` is the single source of truth about client windows, used by
tasks and available to anything later. It holds one record per client:
title, class, icon, state bits, desktop, frame geometry. Records are filled
once when a client appears and then changed only by the event that reports
the change. A page switch then costs no round trips: the viewport property
changes, visibility is recomputed from cached geometry, one signal fires.
Property fetches that are needed go out as a batch of xcb cookies and are
collected together.

This targets the page-switch lag. The current `refresh()` makes about six
synchronous round trips per client across every page on each run. There is
also a specific path worth naming as a hypothesis, not a diagnosis: the
first time a frame is seen moving, `_on_configure` records it and returns
without scheduling a refresh, so if the viewport-triggered refresh ran before
that frame landed, the list stays stale until some unrelated event arrives.
That would explain lag that appears on first visits to a page and is hard to
reproduce afterward.

Requirement: the list is correct within one frame (16 ms) of the last
`ConfigureNotify` of a page switch. A `--trace-x` flag logs event arrival and
repaint times so the intermittent case can be caught when it happens.

## 7. Widgets

### 7.1 Clock

As now: the time at the left end, the date at the right. The clock draws
two strings, so `config.ini` holds two QDateTime formats, `[clock] time`
and `date`.

### 7.2 World View

The `ShelfPager` FvwmPager instance in a `ForeignSlot`. Present only when an
fvwm socket is reachable; otherwise the widget is not offered.

The widget owns everything fvwm needs for it and sends it over `FvwmCommand`:
the `*ShelfPager:` module configuration, the four pager colorsets, and the
launch and kill at each width step. `config` then keeps only the `Style
ShelfPager` line. **VERIFY** that module configuration lines sent at runtime
behave the same as ones read from `config`.

Pager colors come from the palette like any other, pushed to fvwm when the
palette changes. This is the only place palette data crosses to fvwm without
the user asking for an export.

### 7.3 Tasks

Pinned (sticky) windows become a two-column grid of tiles big enough to hit,
each showing a thumbnail when one is available and a large icon when not.

The panel captures thumbnails itself through the Composite extension
(`xcb_composite_name_window_pixmap`), scaled down and held in memory. This
replaces `lib/Thumbnail` and the `images/thumbs/` directory for the panel's
purposes.

- A mapped window is recaptured when it loses focus and, while it is shown
  as a pinned tile, on Damage events throttled to about one capture a
  second.
- An unmapped window has no pixmap, so a minimized window shows its last
  capture from before it was minimized.
- `xcompmgr` already redirects windows on this machine. Without a
  compositor the panel asks for automatic redirection itself. **VERIFY**
  that this works with no compositor running and costs nothing visible.
- No Composite extension: icons only.

Overflow, in order, as the list outgrows its height:

1. minimized-window thumbnails shrink together to a floor height;
2. below the floor they collapse to plain rows, oldest first;
3. the pinned grid drops from thumbnails to icon tiles;
4. the row list scrolls.

Rows never go below `ROW_H`. The floor and the pinned tile size are settings.

### 7.4 Media

Ported: MPRIS transport, marquee title, cover art, sink volume, output
selection, the picture-in-picture monitor. `pactl subscribe` stays as a
subprocess.

Added:

- **Trackbar.** Position, length, click and drag to seek. MPRIS does not
  signal position, so the bar advances from a local timer between `Seeked`
  signals and playback-state changes, and only while playing and visible.
  Hidden for players that report `CanSeek` false.
- **Source selector.** "Automatic" follows the most recently playing player,
  as now; otherwise pin to one player by its bus name.

Output selection currently goes through `FvwmCommand`. Replace with a direct
`pactl` call so the widget has no fvwm dependency.

### 7.5 Tray

stalonetray in a `ForeignSlot`, launched and owned by the panel. Its colors
and geometry are passed as command-line arguments built from the palette, so
`stalonetrayrc` and its template go away. **VERIFY** every option the rc file
sets has a command-line form. A native XEmbed host stays rejected for the
reason recorded in `photon_tray.py`.

## 8. Palette

### 8.1 Model

`roles.j2` names about forty colors, most of them the face color lightened
or darkened by a number such as 0.134 or 0.207. Those numbers were measured
off a reference image one pixel at a time, which is why they look arbitrary:
they are. Ranked by perceptual lightness, the twenty-six face-derived roles
fall into about twelve clusters, and several pairs differ by an amount
nobody can see (`groove` and `trough_sh`, `face` and `face_alt`, `gutter`
and `well`).

So the palette is not a list of roles. It is:

- **A ramp**: twelve tones of one material, from near-black to near-white,
  with the face at tone 0. Generated from the face color in OKLCH, holding
  hue, stepping lightness, tapering chroma toward the ends. Steps are fine
  near the face, where bevels need subtle differences, and coarse far from
  it.
- **A handful of named colors** that are not shades of the face: ink, fill
  (meter and trackbar), selection, pager desk, and the window-manager chrome
  pair that exports use.

Approximate ramp, from today's palette (OKLab lightness):

| tone | L | replaces |
|---|---|---|
| -6 | 0.30 | dark |
| -5 | 0.40 | header_glyph, tick_lo |
| -4 | 0.50 | groove, trough_sh, sh |
| -3 | 0.57 | frame_mid, groove_mid, btn_lo |
| -2 | 0.61 | trough, well_sh |
| -1 | 0.645 | header_cell, gutter, well |
| 0 | 0.676 | face, face_alt, header |
| +1 | 0.71 | well_lip |
| +2 | 0.77 | face_hi |
| +3 | 0.82 | hi, btn_hi |
| +4 | 0.876 | thumb_hi |
| +5 | 0.91 | field |

Painters ask for tones by number: a sunken well is "outline -4, shadow -2,
floor -1, lip +1". Which tone a bevel edge uses is a fact about the bevel
and lives in the painter, not in the palette. The numbers that remain are
the twelve lightness offsets, in one ordered table, visible in the editor as
one strip.

The user's controls, in order of reach:

1. the face color, which regenerates the whole ramp;
2. a contrast amount, which scales every offset;
3. any single tone, overridden with a literal color and resettable;
4. the named colors.

The file stores exactly that:

```ini
[palette]
face=#a2948a
contrast=1.0
ink=#020200
fill=#584a0c
selection=#...
pager_desk=#5c300e

[override]
tone-2=#8c8078
```

Cost: the result will not match the measured reference to the pixel. Edges
that were 0.05 and 0.06 darker become the same tone. Golden images from the
Python shelf are therefore a visual reference for the port, not an exact
target.

**OPEN**: the exact tone count and offsets, settled in milestone 1 by
rendering the panel both ways and comparing.

### 8.2 Editor

In the settings window, in the manner of qt6ct:

- the ramp as one strip of twelve swatches with the face marked, a contrast
  slider under it, and the named colors beside it;
- clicking a swatch opens a color dialog; an overridden tone carries a
  marker and right-click resets it;
- hovering a swatch outlines where it is used on the live panel;
- changes apply to the panel immediately; Save writes `palette.ini` and a
  history copy; Revert restores the last saved state.

### 8.3 Generation

In-process, no wallust and no parameters to tune. "From image":

1. scale the image down to about 128 pixels on its long side;
2. cluster its pixels in OKLab (k-means, k around 8);
3. for each sufficiently distinct cluster, propose a palette with that
   cluster's hue as the face, its lightness and chroma clamped to a range in
   which the ramp has room on both sides, and the named colors taken from
   the remaining clusters by contrast with the face.

That gives four to six candidates, each shown as its ramp strip plus named
colors. Picking one loads it into the editor as unsaved changes. Nothing is
written until Save. This replaces `palette sweep`.

Existing `palette/schemes/*.json` are imported once at switch-over: face
from color7, pager desk from color14, as `roles.j2` maps them today.

### 8.4 Presets and history

Save As writes `palettes/<name>.ini`. The preset list shows each as a strip.
History lists timestamped saves, newest first, each restorable. History is
pruned to a fixed count.

## 9. Exports

The panel themes only itself by default. An export target writes the palette
into some other program's format, and is data, not code:

```
exports/fvwm/
  target.ini        destination path, command to run afterward
  template          the output file, with {name} placeholders
```

Substitution is plain replacement with a few format suffixes (`{face}`,
`{face:rgb}`, `{face:noHash}`). No conditionals, no arithmetic: all
derivation already happened in the palette model. Anything that needs a
color the model lacks means adding a named color, where it is visible in the editor.

Placeholders are tones and named colors: `{tone-2}`, `{face}`, `{ink}`.

Shipped targets, converted from the current templates: fvwm colorsets,
qt6ct, qt5ct, KDE, GTK 3, GTK 4. The fvwm chrome shades (`chrome_bg_deep`
and the rest of the second half of `roles.j2`) become a second, shorter ramp
generated from the chrome color, so the export has them.

Xresources is deferred: with wallust gone the palette holds no sixteen
terminal colors. **OPEN** whether to derive a terminal set later.

The settings window lists targets with a checkbox each and two buttons:
Export now, and Export on save. A target overwrites its destination after
copying the previous file into `history/`.

## 10. Settings window

One window, opened from the panel's context menu or `<name> --settings`.
Pages: Layout (side, width, widget order, edit mode toggle), Palette (8.2 to
8.4), Exports (9), then one page per widget that returns a `settingsPage()`.
It is part of the same process and edits the same objects the panel draws
from.

## 11. Build and test

- CMake, C++20, warnings as errors. Dependencies: Qt 6 (Widgets, DBus,
  Network), xcb, xcb-composite if 7.3 goes that way. All present on this
  machine.
- A development preset with AddressSanitizer and UBSan.
- `<name> --only <widget>` runs one widget as a plain window.
- `<name> --render out.png --fixture <dir>` draws the panel offscreen with
  canned data. Until there are fixtures, `--render out.png [--palette
  file.ini]` draws the default layout with a fixed time and font, and
  `--render-sheet out.png [--palette file.ini]` draws a sheet of the painters
  and the ramp for the tone comparison.
- Qt Test for the pure parts: layout solver, palette derivation and file
  round-trip, export substitution, task ordering and overflow, `_NET_WM_ICON`
  decoding.
- Golden images from `--render`, compared exactly.
- `fvwm-rig` for anything that needs a window manager: struts, swallowing,
  page-switch timing.
- A Gentoo ebuild once it installs cleanly.

## 12. Later widgets, and what they need from this design

| idea | requirement on the architecture now |
|---|---|
| dockapp tray | `ForeignSlot` handles windows it did not launch; a slot can hold a grid of 64x64 foreign windows |
| system monitor in the cputnik manner | none beyond the widget contract |
| Winamp-style player, analyzer | media's data sources (MPRIS, sink) stay separate classes from its painting, so a second face can reuse them |
| notes, pomodoro | per-widget persistent state in `config.ini` |
| calendar and reminders | none yet |

## 13. Milestones

Each ends with something runnable. The Python shelf stays in service until
the last one.

1. **Skeleton.** CMake project, palette model and file, painters, `--render`
   of a painter sheet (the panel's frame holding one sample of each painter,
   and the ramp), its golden image.
2. **Panel.** Window, slots, layout solver, resize, side switch, struts,
   config file, clock.
3. **Window model and tasks.** Including the timing trace and the new pinned
   grid and overflow rules.
4. **Foreign slots.** Pager and tray.
5. **Media.** Port, then trackbar and source selector.
6. **Edit mode and pop-out.** Including the visual separation decision.
7. **Settings window.** Layout page, palette editor, generation, presets,
   history.
8. **Exports and switch-over.** Convert templates, wire `config` to the new
   binary, retire `bin/shelf-panel`, `bin/photon*.py`, `bin/palette` and
   `palette/`.

## 14. Open questions

1. Name of the program.
2. Config format: INI or TOML (3.4).
3. Widget separation treatment (5).
4. Tone count and offsets (8.1), settled by rendering in milestone 1.
5. Terminal colors and the Xresources export (9).

Decided: own repository in `~/projects/`; launcher rows dropped; thumbnails
captured by the panel through Composite; extraction in-process, no wallust;
palette is a tonal ramp plus named colors, not a role list.
