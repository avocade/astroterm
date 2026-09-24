## Why

astroterm is a lovely planetarium for the terminal, but it is a *viewer you configure at launch*: every toggle
(`-C`, `-g`, `-c`, `-m`…) means quit and restart, and it cannot be used outside at night because its white/colored
glyphs destroy dark adaptation. Meanwhile the objects people actually point at in a real night sky (the ISS,
Starlink trains) are missing entirely (upstream issue #46 has asked for the ISS since 2024). This change turns
astroterm into a **field instrument**: live controls, a red night-vision mode, and the satellites overhead.

A second goal is **upstreamability**: every feature is sliced so it can land as its own small PR against
`da-luce/astroterm` (no new link-time dependencies, Windows still compiles, Unity tests for the new math).

## What Changes

- **Live keybindings** for every existing display toggle (color, constellations, grid, unicode, braille, metadata,
  magnitude threshold). CLI flags keep working and now only set the *initial* state.
- **`?` help modal**: centered overlay listing all keys; closes with `?` or `ESC`. A short **toast** confirms each toggle
  ("Starlink: on, 10,687 sats").
- **Time controls** on a drift-free **sim clock**: `space` pause, `>`/`<` faster/slower (through reverse), `n` jump
  back to *now*. Sim time is derived from the wall clock instead of accumulated frame deltas, which also fixes
  upstream #77 (slow desync from local time).
- **Red night-vision filter** (`r` key, `--night` flag): everything, including the metadata panel, help modal and
  satellite overlays, is drawn in red on black.
- **Space stations**: the ISS (and China's Tiangong) are drawn like the planets, propagated with a built-in SGP4
  implementation from current TLEs. On by default, `i` toggles. Closes upstream #46.
- **Starlink overlay** (`x` key, `--starlink` flag): every Starlink satellite above the horizon as a tiny blue dot
  (a single braille dot in Unicode mode), rendered *beneath* the stars so it never hides them. Satellites lit by
  the Sun are brighter than those in Earth's shadow, which is exactly the "can I see it right now?" question.
- **Next ISS pass** in the metadata panel: "ISS up now, 34° NW" or "next visible pass 21:43, max 67°".
- **Motion vectors** (`v` key, `--vectors`): a thin, dim line from each moving object (Sun, Moon, planets, space
  stations, Starlink) to where it will be after a fixed look-ahead (1 h for Solar System bodies, 30 s for
  satellites), so direction and speed of travel read at a glance. A Starlink "flow field" falls out for free.
- **Constellation names** (`N` key): full constellation names at each figure's centroid. Closes upstream #35.
- **Arrow keys no longer quit** (they arrive as `ESC [ A` today): enable `keypad()` and a short `ESCDELAY`.
- **TLE data pipeline**: satellites load from a local cache (`$XDG_CACHE_HOME/astroterm/`), refreshed from
  CelesTrak in the background by spawning `curl` (no libcurl link dependency) when older than 12 h. `--offline`
  disables all network access. Offline-first because the dark site has no signal.

### Beyond this change (idea backlog, not built here)

1. **Auto night mode at dusk**: switch to red when the Sun drops below -6° (civil twilight ends).
2. **Meteor shower radiants**: mark active radiants with their ZHR (Orionids peak Oct 21).
3. **Face-the-sky rotation**: arrow keys rotate the dome so the horizon you face is at the bottom of the screen.
4. **`/` search**: find "Saturn" or "Vega", highlighting it or pointing to the horizon it is below.
5. **Aurora watch**: NOAA Kp index and auroral oval hint for high latitudes (Stockholm!).
6. **Starlink train detector**: label fresh-launch trains as a group instead of 60 anonymous dots.
7. **Messier / deep-sky overlay**, **conjunction alerts**, **twinkle mode** (scintillation grows toward the horizon),
   **config file** for persistent toggles (upstream #24).

## Capabilities

### New Capabilities
- `interactive-controls`: live keybindings, the `?` help modal, toast feedback, arrow-key safety.
- `sim-clock`: drift-free simulation time with pause, speed ladder (including reverse), and jump-to-now.
- `night-vision`: red-on-black rendering mode covering every visual element.
- `satellite-tracking`: TLE parsing, SGP4 propagation, topocentric az/alt, sunlit/eclipsed state, space-station
  markers and next-pass prediction.
- `satellite-data`: cached, background-refreshed TLE acquisition with an offline mode.
- `starlink-overlay`: the Starlink constellation layer and its rendering rules.
- `motion-vectors`: look-ahead motion lines for fast-moving objects.
- `constellation-names`: constellation name labels.

### Modified Capabilities
- (none: the repository has no existing OpenSpec specs; existing CLI flags keep their meaning)

## Impact

- **Code**: `src/main.c` (render loop, input, flags), `src/term.c` (color init, palettes), `src/core_render.c`
  (layer order, satellite/label rendering), `include/arg_definitions.h` (new flags). New modules: `sgp4`,
  `satellite`, `tle_cache`, `controls`, `sim_clock`, `overlay` (help/toast), `constell_names`.
- **Dependencies**: none new at link time. Runtime: `curl` on `PATH` for refreshing satellite data (optional;
  without it, satellites use the cache or stay hidden).
- **Network/privacy**: a GET to `celestrak.org` at most every 12 h per feed when satellites are enabled; nothing
  else. `--offline` guarantees none.
- **Platforms**: macOS/Linux fully; Windows compiles and reads the cache, but background refresh is POSIX-only.
- **Tests**: new Unity suites for SGP4 (vectors from python-sgp4, Vallado's reference implementation), TLE
  parsing, topocentric conversion, shadow test, sim clock and key handling.
