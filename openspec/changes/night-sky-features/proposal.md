## Why

It is a clear night at a dark site with no signal. You want to know what is overhead, when the ISS comes over and
from where, and whether that moving light is a Starlink. You also want to keep your night vision while you look.
astroterm draws a lovely sky, but it is a viewer you configure at launch: every toggle means quit and restart, its
white and colored glyphs destroy dark adaptation, and the objects people actually point at (the ISS, Starlink) are
missing. This change makes it a **field instrument**: live controls, a red night-vision mode, and the satellites
overhead, offline-first.

## What Changes

- **Live keybindings** for every display option, and a **`?` help modal** listing keys with their live state.
  CLI flags keep working and only set the initial state. A short toast confirms each action.
- **Time controls** on a drift-free **sim clock**: `space` pause, `>`/`<` faster/slower (1x to 3600x), `n` back
  to now. Sim time is anchored to the realtime clock instead of accumulating nominal frame periods, which also
  fixes the slow desync reported upstream (#77).
- **Red night-vision filter** (`r`, `--night`): every glyph red on true black, including the Moon (a text glyph
  instead of a colour emoji), the panel, the help modal and the satellites.
- **Space stations**: the ISS and Tiangong drawn like planets from a built-in SGP4 propagator (on by default,
  `i` toggles), plus a corner line with the next visible ISS pass: time, rise direction, peak altitude.
- **Starlink overlay** (`x`, `--starlink`): sunlit Starlinks as tiny blue dots (single braille dots in Unicode
  mode), beneath the stars so they never hide them. `X` also reveals the ones in Earth's shadow, dimmed.
- **Motion vectors** (`v`): a thin dim line showing where each moving thing is heading. Satellites: their path
  over the next 10 seconds. Sun, Moon and planets: their drift against the stars over the next 24 hours (the Moon
  moves ~13° a day; planets reveal retrograde loops).
- **Face the sky**: `←`/`→` rotate the dome so the horizon you face sits at the bottom of the screen.
- **Satellite data**: CelesTrak OMM CSV (the TLE format cannot carry catalog numbers issued since July 2026),
  cached per user, refreshed before the UI starts when older than 12 h and never more often than CelesTrak
  allows. `--offline` guarantees no network.
- **Fixes on the way**: arrow keys no longer quit (they arrived as `ESC [ A`), and a warning when the location is
  still the default 0°, 0°.

## Capabilities

### New Capabilities
- `interactive-controls`: live keybindings, the help modal, toasts, arrow-key safety, dome rotation.
- `sim-clock`: drift-free simulation time with pause, a speed ladder and jump-to-now.
- `night-vision`: red-on-black rendering covering every visual element.
- `satellite-tracking`: OMM parsing, SGP4, topocentric position, sunlit state, element-age limits, station markers,
  next-pass prediction.
- `satellite-data`: cached, polite, pre-UI acquisition of CelesTrak feeds with an offline mode.
- `starlink-overlay`: the Starlink layer and its rendering rules.
- `motion-vectors`: look-ahead motion lines for satellites and Solar System bodies.

### Modified Capabilities
- (none: the repository has no existing OpenSpec specs; existing CLI flags keep their meaning)

## Impact

- **Code**: `src/main.c` (loop order, input, flags), `src/term.c` (color init, palette), `src/core_render.c`
  (layers, rotation, vectors), `src/core_position.c` (pure position queries), `src/drawing.c` (per-layer braille
  masks), `include/arg_definitions.h`. New: `sgp4`, `omm`, `satellite`, `feed`, `sim_clock`, `ui`.
- **Dependencies**: none new at link time. Runtime: `curl` on `PATH` to refresh satellite data (optional).
- **Network/privacy**: at launch, when a satellite layer is on and a feed is older than 12 h (and CelesTrak has
  had at least 2 h to update), one GET per feed to `celestrak.org`. `--offline` disables it.
- **Platforms**: macOS/Linux fully; Windows compiles (PDCurses guards) but does not download.
- **Upstream**: sliced into independent commits for PRs: sim clock (#77) · live keys + help · night vision ·
  stations + data (#46) · Starlink · vectors · rotation. The maintainer has not been consulted yet.
