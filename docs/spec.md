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

A slot is handed a window id, or told to take the next client the window
model sees appear with a given `WM_CLASS` instance. The window model
announces each window that joins the client list, shown or not, once its
properties are read. Taking the next one rather than one already listed is
how the panel picks out the window of a program it has just launched, and
keeps it from taking another program's tray or a pager left from an earlier
run. Qt reparents the window into the panel's top-level window at the
slot's place. The slot selects `StructureNotify` on it and lets go on a
`DestroyNotify` or on a reparent to a window that is not the panel's.

stalonetray places itself in root coordinates each time it resizes, as if it
were still top-level; in `fvwm-rig` that took it out of the panel whenever an
icon arrived. So the slot moves the window back to its place after every
`ConfigureNotify`. It leaves the size alone.

### 3.1 Source layout

Its own repository under `~/projects/`, name undecided (`<name>` below).

```
src/
  main.cpp            argument parsing, single-instance check, startup
  panel/              Panel window, slot layout, edit mode, pop-out, struts
  palette/            Palette model, ramp, file I/O, extraction, export
  paint/              the bevel painters from photon.py, themed icons
  x11/                xcb connection, atoms, event filter, WindowModel
  media/              MPRIS, sink and cover art sources, free of widgets (section 12)
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
whole program. So the window model follows each client through
`StructureNotify` on the client itself (section 6), and the root needs only
`PropertyChange`.

### 3.3 The widget contract

```cpp
class ShelfWidget : public QWidget {
public:
    virtual QString id() const = 0;                // stable, used in config
    int heightForWidth(int w) const override = 0;  // natural height
    virtual int minimumHeightForWidth(int w) const;
    virtual bool wantsSpare() const;               // takes leftover height
    virtual void paletteChanged();                 // repaints, passes colors on
signals:
    void heightChanged();
};
```

`paletteChanged()` is how the pager and the tray hand new colors to the
programs that draw them (7.2, 7.5).

`settingsPage()` joins the contract with the settings window (milestone 7);
until there is a window to put a page in, nothing would call it.

A widget owns nothing outside its own rectangle, and does not know whether it
is docked or popped out. The existing rule carries over: a widget does not
talk to fvwm. The one exception is the pager, which is an fvwm module by
definition and keeps all fvwm traffic inside its own file.

### 3.4 Configuration

`~/.config/<name>/`:

```
config.ini          panel side, width, widget order, pop-outs, per-widget settings
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

The panel is an ordered list of slots, one widget per slot. Unless
`config.ini` says otherwise the order is clock, World View, tasks, media,
tray; a widget that is not offered (7.2, 7.5) is left out. For a given width:

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

Headers go. Each slot is drawn as its own unit, so that a widget reads as an
object sitting in the panel and not as a region of it. Most widgets meet
their edge with a well (World View, tasks, tray) or a full-width field
(media), so a closed edge drawn just around such a widget would frame it
twice. Three treatments are built, chosen by `[panel] separation=` in
`config.ini`:

- `tiles`: each widget on a raised tile of face, with two pixels of face
  around it, the tiles 5 pixels apart on a ground of tone -2. A well sits on
  the tile rather than inside a second outline.
- `bays`: each widget sunk into the face behind a one-pixel soft bevel,
  `raised` turned half round, flush with the widget, the bays 5 pixels apart.
  A well at the edge merges with it into one deeper well.
- `grooves`: no edge; slots 13 pixels apart with the five-row `groove`
  between them.

`tests/golden/panel-<name>.png` shows each. The default is `tiles`, the only
one in which the space between widgets is plainly not part of any of them;
it costs 12 pixels of the width. **OPEN**: pick one. The key is scaffolding
for that decision and goes with the two that lose: one enum, one name table
in the config file, one switch in the panel.

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

The class is `Shelf`. fvwm matches `Style` names with case, so `config`'s
`Style shelf` line for the panel, whose instance and class are both `shelf`,
leaves the windows alone; with the class `shelf` they lost their titles in
`fvwm-rig`. A window is as tall as its widget's natural height at its width,
which is the width the widget had in the panel.

`config.ini` keeps the state, and it survives a restart of the panel:

```ini
[panel]
widgets=clock, worldview, tasks, media, tray
popped=media

[popout]
media=188, 420
```

A popped-out widget keeps its place in `widgets`, where it docks again.
`[popout]` holds where each window's frame was last, written once a move has
settled for half a second and when the window closes, and kept after the
widget docks for the next time it pops out. A widget with no place yet opens
beside the panel, level with its slot as far as the screen allows. Qt takes a
position as the frame's only when it is set before the window exists; a
window placed by its inside instead came up 11 pixels lower in `fvwm-rig`
than asked, on every restart. Widgets that `widgets` names but that cannot
run here (7.2, 7.5) keep their place in it too, so that a panel started
without fvwm does not drop World View from the file.

## 6. Window model

`x11/WindowModel` is the single source of truth about client windows, used by
tasks and available to anything later. It holds one record per client:
title, class, icon, state bits, desktop, and the client's geometry in root
coordinates. Records are filled once when a client appears and then changed
only by the event that reports the change. Property fetches that are needed
go out as a batch of xcb cookies and are collected together, so a pass over
a burst of events costs one round trip, and one more when windows appeared.

A page switch costs no round trips. fvwm moves every frame on the desk,
minimized ones included, before it sets `_NET_DESKTOP_VIEWPORT` (whose value
fvwm3 leaves empty), so the viewport is not read: a client is shown when its
geometry meets the screen. The geometry comes from the synthetic
`ConfigureNotify` that ICCCM 4.1.5 has a window manager send a client it
moves, which carries root coordinates. A real one, from a resize, has
coordinates within the frame and costs one batched `TranslateCoordinates`.
A focus change that comes with a switch costs one fetch of
`_NET_ACTIVE_WINDOW`. An update runs 5 ms after the first event of a burst;
a switch and its focus change span about 2 ms in `fvwm-rig`, so a switch
fires one signal.

Frames are not followed. Watching them (3.2) lost moves to Qt's compression
a second way: fvwm follows each frame move with a synthetic `ConfigureNotify`
on the frame, a workaround it keeps for Tk, and Qt kept that one and dropped
the real one, so 27 of 38 moves arrived. A client's own synthetic event is
the newest for its window, so compression only drops stale ones.

This targets the page-switch lag. The current `refresh()` makes about six
synchronous round trips per client across every page on each run. There is
also a specific path worth naming as a hypothesis, not a diagnosis: the
first time a frame is seen moving, `_on_configure` records it and returns
without scheduling a refresh, so if the viewport-triggered refresh ran before
that frame landed, the list stays stale until some unrelated event arrives.
That would explain lag that appears on first visits to a page and is hard to
reproduce afterward. `_on_configure` also tests the view against the frame's
position negated, so many moves into or out of view are not seen as changes.
Neither path exists here: there is no frame to discover, and visibility does
not wait for a refresh.

Requirement: the list is correct within one frame (16 ms) of the last
`ConfigureNotify` of a page switch. A `--trace-x` flag logs event arrival and
repaint times so the intermittent case can be caught when it happens.

Measured with `--trace-x` in `fvwm-rig`: twelve windows over five pages, ten
switches, the first five each a first visit to its page. Each switch moved
eleven windows within 0.5 ms, and the corrected list was painted 5.5 to 6.5
ms after the last `ConfigureNotify`, after one round trip, for
`_NET_ACTIVE_WINDOW`. First and later visits did not differ.

## 7. Widgets

### 7.1 Clock

As now: the time at the left end, the date at the right. The clock draws
two strings, so `config.ini` holds two QDateTime formats, `[clock] time`
and `date`.

### 7.2 World View

FvwmPager in a `ForeignSlot`, under the module alias `ShelfWorldView`,
showing desks 0 and 1 stacked. The alias is not the Python shelf's
`ShelfPager`, so that both can run. The widget is offered only when fvwm can
be reached: `FVWMMFL_SOCKET` names a file that exists and `FvwmCommand` is
on the path.

The widget owns everything fvwm needs for it and sends it over `FvwmCommand
-c`, a batch to a process and one process at a time, so that fvwm runs the
commands in order; in `fvwm-rig` FvwmMFL ran every line of a single write,
forty lines included. A launch is `KillModule`, `DestroyModuleConfig
ShelfWorldView:*`, the module configuration, the four colorsets, then
`Module FvwmPager ShelfWorldView 0 1`. The configuration is the old
`*ShelfPager:` block less the lines that set defaults: the colorsets,
`Font`, `Rows 2`, `MiniIcons`, `SolidSeparators`, `HideSmallWindows`,
`UseSkipList` and `Geometry`.

FvwmPager lays out its grid when it starts, so each new size of the slot,
once it has held for 50 ms, starts a new pager. One launch is in flight at
a time, and a size that changed meanwhile is taken up when the window
arrives. A pager lost any other way means fvwm restarted or the module
died, and the widget starts it again after half a second. While fvwm
restarts, `FvwmCommand` fails or its commands go nowhere, so a launch whose
`FvwmCommand` failed, or whose window has not come within five seconds, is
tried again every half second, twenty times. Three `Restart`s in `fvwm-rig`
each brought the pager back at the first try. The widget kills the pager
when it goes; when the panel is killed instead, the pager's window dies with
the panel's and FvwmPager exits.

The pager colorsets are fixed at 300 to 303, far above the numbers configs
give their own. They carry the colors colorsets 21, 20, 36 and 37 had,
through the table in 8.1:

| colorset | pager option | fg | bg |
|---|---|---|---|
| 300 | `Colorset` | tone -6 | tone -2 |
| 301 | `HilightColorset` | ink | tone 0 |
| 302 | `WindowColorset` | tone -6 | tone -1 |
| 303 | `FocusColorset` | tone -6 | tone +2 |

`pager_desk` is not among them, as it was not among the old ones. A palette
change re-sends the four `Colorset` lines, and a running FvwmPager redraws
from them. This is the only place palette data crosses to fvwm without the
user asking for an export.

The labels are in the widget's font, sent as an Xft name at its whole pixel
size (`xft:Family:pixelsize=N:weight=W`), so that the widget knows their
height: FvwmPager's label bar is the font's height plus 2. In `fvwm-rig` a
169x228 pager drew 19-pixel labels over 95-pixel desks for a 17-pixel font,
as computed. The natural height is two labels and two pages in the
proportions of the screen's virtual geometry, inside the well.

Verified in `fvwm-rig` (fvwm3 1.1.5): module configuration sent at runtime
behaves as configuration read from `config` does. A pager started after
`*Alias:` lines sent with `FvwmCommand` used them. `$[...]` in such a line
is expanded when the line runs, alike for a line sent at runtime and one in
a file read with `Read`: a `Font $[infostore.bigfont]` line stored while
`bigfont` named a 24-point font, followed by a change to 8 points, gave
24-point labels both ways. So the note in `~/.fvwm/AGENTS.md` that such
lines do not expand `$[...]` does not hold for `$[infostore.*]`. The widget
sends no `$[...]`.

`config` keeps one line, so that the pager's window does not show as an
ordinary one for the moment before the slot takes it:

    Style ShelfWorldView !Title, !Handles, BorderWidth 0, HandleWidth 0, \
        Sticky, WindowListSkip, CirculateSkip, NeverFocus, !Iconifiable

Without it the window gets the styles that match its class, `FvwmPager`.

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
  capture from before it was minimized. A window is unmapped before the
  focus leaves it, so the panel names the focused window's storage while it
  has the focus and reads it when the focus moves; named storage outlives
  the unmap.
- The panel redirects each listed client itself, automatically, whether or
  not a compositor runs, rather than the root's subwindows. A capture then
  shows the client without its frame, and the panel's own windows and every
  popup stay unredirected. Under `xcompmgr` the client's storage nests in
  its frame's at the cost of one more copy per redraw.
- No Composite extension: icons only.

Verified in `fvwm-rig` (Xvfb, no compositor): automatic redirection of each
client gives storage that `NameWindowPixmap` and `GetImage` read correctly,
and costs nothing visible. A screenshot of four windows before and after
they were redirected differed in no pixel, and page switches with two pinned
windows redrawing all the time still painted the list 5.8 to 7.1 ms after
the last `ConfigureNotify`.

Tried under `xcompmgr` in `fvwm-rig`, both as the live session runs it,
`xcompmgr -o -Cc`, and as `xcompmgr -c -C`. The live command is not what it
reads as: `-o` takes a value, so `-Cc` becomes the shadow opacity and
neither `-C` nor `-c` applies. Pinned tiles went on recapturing about once a
second, a window minimized while the compositor ran showed its capture, and
ten screenshots over two seconds under `-c -C` differed only in the second
hand of a pinned `xclock`. Nothing flickered or went black, and the pager
and the tray drew as without a compositor.

A capture is a `GetGeometry` and a `GetImage` round trip and a scale to at
most 400x400, taken one per turn of the event loop. A 1250x868 window took
1.4 to 4.6 ms in a release build (4.7 to 7.2 ms under ASan), a 300x200 one
about 0.3 ms. Pinned windows that redraw constantly were captured once a
second each. Not measured: a 4K window, with seven times those pixels. If
that blocks visibly, the next step is to scale on the server with RENDER
and read only the small image.

Overflow, in order, as the list outgrows its height:

1. minimized-window thumbnails shrink together to a floor height;
2. below the floor they collapse to plain rows, oldest first;
3. the pinned grid drops from thumbnails to icon tiles;
4. the row list scrolls.

Rows never go below `ROW_H`. The floor and the pinned tile size are settings:
`config.ini` `[tasks]` `floor` (default 64) and `tile` (default 48), both in
pixels; the tile is the height, as there are always two columns. "Oldest" is
the window minimized longest ago. Icon tiles are 32 pixels tall.

### 7.4 Media

Ported: MPRIS transport, marquee title, cover art, sink volume, output
selection, the picture-in-picture monitor. `pactl subscribe` stays as a
subprocess.

Picture-in-picture: a window the window model announces joining the client
list is the candidate if it is shaped like a video (160x90 to 1000x700, 1.2
to 2.6 times as wide as tall), is not `_NET_WM_WINDOW_TYPE_NORMAL`, and
belongs to a process named firefox, chromium or chrome. The Python widget
polled the client list every 1.5 seconds instead, which also caught a window
that took that shape later; this one does not. The candidate is dropped
when a synthetic `ConfigureNotify` puts its centre over the widget with no
button down, and a `ForeignSlot` holds it in the well, shaped to it. A
second one is not taken while one is held. Two timers remain, each running
only while there is something to watch. While the candidate's centre is
over the widget and a button is down, the pointer is read every 120 ms,
since the button's release reaches only the window manager carrying the
window. While a window is held, the pointer is read every 50 ms, since
fvwm's Win+drag grabs the pointer on the panel and a drag out of the well
reaches neither the window nor the panel. A drag of four pixels puts the
window back on the root under the pointer and, once the window manager has
it again, asks for the move with `_NET_WM_MOVERESIZE`, where the Python
widget sent fvwm a `Move`. A held window goes back on the root with the
widget. The drag out relies on the panel's style having `FixedPosition`, as
`ShelfPanel`'s does: in `fvwm-rig` without it, the same Win+drag let the
window go and moved the whole panel as well.

Added:

- **Trackbar.** Position, length, click and drag to seek. MPRIS does not
  signal position, so the bar advances from a local timer between `Seeked`
  signals and playback-state changes, and only while playing and visible.
  Hidden for players that report `CanSeek` false or no length. The position
  is read again when the state, the rate or the track changes, any of which
  can move it without a `Seeked`, and the timer fires as often as the fill
  can gain a pixel, at most every 40 ms. A drag shows where it will land and
  seeks on release, by `SetPosition` with the track's id, or by `Seek` over
  the distance for a player that gives no id.
- **Source selector.** "Automatic" follows the most recently playing player:
  of those playing, the one that started last; with none playing, the one
  that started last; else the first the bus lists. The Python widget kept a
  playing player until it stopped instead. Pinned, the controls keep to one
  player by its well-known name less any `.instance…` suffix, `vlc` for
  `org.mpris.MediaPlayer2.vlc.instance4242`, kept in `config.ini` `[media]
  source`. A unique bus name lasts one run of the player, and the suffix of
  a player that can run twice carries its pid.

Top to bottom: the source, the cover in a well shaped to it, the title, the
trackbar, transport, volume, the output. The Python widget had the output at
the top and no source. Both are drop-downs drawn with the painters that open
a menu. The speaker is drawn rather than taken from the Haiku icon theme, so
that a render depends on nothing installed.

Output selection is a direct `pactl` call, where the Python widget went
through `FvwmCommand`, so the widget has no fvwm dependency.

The sink side is `pactl` alone, where the Python widget read with `wpctl`.
After each burst of `pactl subscribe` events about a sink or the server,
`pactl -f json info` and `pactl -f json list sinks` give the default sink and
every sink's volume; `pactl set-sink-volume`, `set-sink-mute` and
`set-default-sink` make the changes. Choosing an output sets the default
sink, which `@DEFAULT_SINK@` follows, so nothing is published to fvwm: the
`InfoStoreAdd media_output` goes. A subscription that ends, as it does when
pipewire-pulse restarts, is started again after two seconds, the wait
doubling to about a minute while the server stays away; nothing signals that
a server is back.

`mpris:artUrl` is whatever the page a browser plays put there, so cover art
is bounded: a fetch is abandoned past 8 MB or ten seconds, or when the track
changes, follows at most three redirects and only to http or https, and a
local cover must be a regular file within the same 8 MB, read directly
rather than through Qt's network layer. Loopback and private addresses are
not refused, since the response is only ever decoded as an image inside the
panel and the browser that named the URL could fetch it anyway.

### 7.5 Tray

stalonetray in a `ForeignSlot`, launched and owned by the panel through
`QProcess`, offered only when `stalonetray` is on the path. A native XEmbed
host stays rejected for the reason recorded in `photon_tray.py`.

Its colors and geometry are passed as command-line arguments built from the
palette and the width, so `stalonetrayrc` and its template go away:

    stalonetray --config /dev/null --background <tone -1> \
        --geometry Nx1 --max-geometry Nx0 --icon-size 22 --slot-size 26 \
        --kludges force_icons_size --skip-taskbar

N is how many 26-pixel slots fit across the well. Verified against
stalonetray 1.5.0's `--help` and man page: every option `stalonetrayrc` sets
has a command-line form. Those it sets to stalonetray's defaults
(`decorations`, `dockapp_mode`, `fuzzy_edges`, `grow_gravity`,
`icon_gravity`, `log_level`, `no_shrink`, `parent_bg`, `scrollbars`,
`tint_color`, `tint_level`, `transparent`, `vertical`, `window_layer`,
`xsync`) are left out. Without `--config`, stalonetray reads
`~/.stalonetrayrc` or `$XDG_CONFIG_HOME/stalonetrayrc` under its
arguments, so `--config /dev/null` keeps such a file out. No rc file is
generated.

The panel restarts stalonetray when N changes and when the palette changes,
starting the new one once the old has exited and given up the selection.
stalonetray asks for SIGTERM when the panel dies (`PR_SET_PDEATHSIG`): a
signal or a broken X connection ends the panel without its destructors, and
in `fvwm-rig` a stalonetray whose panel was killed otherwise lived on and
then ignored SIGTERM. One that exits on its own is reported and leaves the
well empty. A stalonetray started while another runs takes the selection
from it, and the other deactivates.

The height follows the icons, as in the Python shelf. stalonetray's window
holds one child per icon and two 1x1 windows of its own. The widget counts
the icons with a `QueryTree` and one batch of `GetGeometry` when the slot
takes the window and after each `ConfigureNotify` on it, which stalonetray
causes whenever it gains or loses a row, and is as many 26-pixel rows as
they need at N across.

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
- `<name> --only <widget>` runs one widget as a plain window, as tall as
  the widget asks to be.
- `<name> --render out.png --fixture <dir>` draws the panel offscreen with
  canned data. Until there are fixtures, `--render out.png [--palette
  file.ini]` draws the default layout with a fixed time, font and screen
  size, foreign slots as empty wells, and
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
3. Widget separation treatment: `tiles`, `bays` or `grooves` (5).
4. Tone count and offsets (8.1), settled by rendering in milestone 1.
5. Terminal colors and the Xresources export (9).

Decided: own repository in `~/projects/`; launcher rows dropped; thumbnails
captured by the panel through Composite; extraction in-process, no wallust;
palette is a tonal ramp plus named colors, not a role list.
