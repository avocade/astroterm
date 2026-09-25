## Context

astroterm (v1.2.0, MIT, C + ncurses + argtable, Meson, Unity tests, CI on Linux/macOS/Windows-MSVC with PDCurses)
renders a stereographic all-sky dome. `main.c` owns one render loop: update positions → render layers →
`getch()` (only `q`/`ESC`) → `doupdate()` → sleep. Options are parsed once into `struct Conf`. Colors are set up
only with `-c`. Simulation time advances by the *nominal* frame period each frame, so slow frames and laptop sleep
make the sky fall behind (upstream #77). Stars are J2000 positions rotated by GMST of date. Nothing touches the
network. v1 of this design was reviewed by a 13-lens panel (`critique.md`); this is v2.

## Glossary

- **Above horizon**: altitude > 0°. Everything drawn on the dome is above horizon.
- **Sunlit**: outside Earth's cylindrical shadow. Necessary, not sufficient, for seeing a satellite.
- **Observer dark**: the Sun's altitude < −6° at the observer.
- **Visible pass**: an ISS pass above 10° during which it is sunlit while the observer is dark.
- **Feed age**: wall-clock time since a feed file was downloaded; drives refreshing.
- **Element age**: |sim time − element epoch|; drives accuracy and hiding.
- **Stations**: the two tracked objects, ISS (25544) and Tiangong (48274), not CelesTrak's "stations" group.
- **Layer**: one z-ordered drawing pass with its own braille mask.

## Goals / Non-Goals

**Goals:** everything live-toggleable and explained by `?`; red night vision; ISS/Tiangong/Starlink from SGP4,
offline-first; motion vectors; time controls; dome rotation; tests whose gauges the implementer cannot bend.

**Non-Goals:** deep-space SGP4 (SDP4); satellite magnitudes; persisted settings/location (upstream #24);
constellation names (#35, separate PR); Windows downloads; precessing the star catalog.

## Decisions

### D1. Near-Earth SGP4 in C, gauged by Vallado's own suite
`src/sgp4.c` transcribes Vallado's SGP4 (near-Earth branch, WGS-72, opsmode `i`). Golden vectors come from
python-sgp4 (which wraps Vallado's C++) for our live sets **and** the near-Earth cases of Vallado's
`SGP4-VER.TLE`, including his decay and eccentricity error cases. Tolerances are 1 m and 1 mm/s, so a coefficient
slip cannot hide. Periods ≥ 225 min are rejected at init. *Rejected:* GPL trackers (predict, gpredict) cannot
enter an MIT codebase; vendoring C++ adds a language.

### D2. OMM CSV is the wire format; `omm.c` is the only translator
CelesTrak exhausted 5-digit catalog numbers in July 2026 and the TLE format cannot carry the rest (verified:
every object launched in the last 30 days is ≥ 100465). `omm.c` reads CelesTrak CSV by header name, handles
quoted fields, and converts values with its own **locale-independent** decimal parser (`setlocale(LC_ALL, "")`
runs at startup). Rows failing range checks (0 ≤ e < 1, 0 < n ≤ 20 rev/day, finite angles, parseable epoch) are
skipped. Object names are not used, so no name ever reaches the terminal.

### D3. Topocentric position through the existing equatorial pipeline
TEME is referred to the mean equinox of date, which is why GMST (IAU-82, `sgp4_gmst`) rotates it to Earth-fixed.
Observer on WGS-84 → TEME; topocentric vector → RA/Dec → `equatorial_to_horizontal`. Polar motion and ΔUT1 are
ignored. Gauged against Skyfield to 0.05°. The stars' missing precession (~0.37°) is documented, not fixed here:
a cell spans 2–5°.

### D4. Sunlit test and element age
Cylindrical shadow with the Sun direction from the existing Earth ephemeris. A set with element age > 14 days is
hidden (a toast explains), so `--datetime` in the past never draws satellites that were not launched yet.

### D5. Data: fetched before the UI, politely
`src/feed.c` resolves the cache dir (`$XDG_CACHE_HOME/astroterm` › `~/.cache/astroterm`, created 0700; relative
`XDG_CACHE_HOME` ignored). At launch, **before `initscr`**, if any satellite layer is on and not `--offline`, each
the stations feed is refreshed when older than 12 h (3.5 KB). Starlink is never fetched without consent (Oskar,
2026-09-25: "we don't want to force a download ever, and the user might be on slow or no internet"): enabling it
shows the cache at once and asks only when the cache is over 14 days old (his cadence, matching the element-age
limit) or missing; `U` asks at any time. Accepted downloads run in the background (spawned `curl`, polled with
`waitpid(WNOHANG)` each frame, cancelled on quit) so a slow link never freezes the sky. The catalog loads only
when the layer is first turned on. Refreshes happen unless a marker file shows an attempt in the last 2 h
(CelesTrak answers repeats with 403 until its next update). The download runs `curl -q -sS --proto =https
--proto-redir =https --max-filesize 20M --connect-timeout 5 --max-time 60 -w %{http_code}` via `posix_spawnp`
(argv, no shell) into a `mkstemp` file, then waits. HTTP 200 with ≥ 1 valid row, and at least half the previous
row count, is renamed over the feed; anything else leaves the cache untouched and is reported on the first frame.
*Why blocking:* it deletes the async state machine (child polling, hot swap, orphaned children on Ctrl-C) at the
cost of a few seconds at most twice a day. *Why both feeds:* so the cache is warm before you leave signal.

### D6. Sim clock anchored to the realtime clock
`sim_clock.c`: `{anchor_jd, anchor_wall, speed, paused}`; `jd = anchor_jd + (wall − anchor_wall) × speed / 86400`.
Every change re-anchors first. Wall time comes from `clock_gettime(CLOCK_REALTIME)` (Windows:
`GetSystemTimeAsFileTime`), so sleep and slow frames cannot cause drift. Ladder `1, 10, 60, 600, 3600`, no
reverse. Toast expiry and satellite cadence use the monotonic `stopwatch` clock instead, so a stepped wall clock
cannot freeze them. Pure functions take `now`, so tests inject time.

### D7. Loop order and state ownership
Per frame: update → draw (stdscr background, sky layers, panel, corner lines, toast, help) → `wnoutrefresh` all →
`doupdate()` → **then** read input, so `getch()`'s implicit `wrefresh(stdscr)` has nothing to paint. Display
toggles live in `Conf`; `UiState` holds only transient UI (help open, toast); `SimClock` alone owns time and
speed. One static table in `ui.c` maps each key to its handler, help row and toast text.

### D8. Night vision: roles, not hues
Objects carry a render **role** (`BODY`, `STAR`, `LABEL`, `STATION`, `STATION_DARK`, `SAT_LIT`, `SAT_DARK`,
`LINE`, `UI`). `palette_attr(role)` returns pair + attributes. Day mode keeps upstream colors (pairs 1–8) plus
blue satellites. Night mode maps roles to three reds on true black: bright (bodies, stations), medium (stars,
labels, sunlit satellites, UI text), dim (eclipsed satellites, lines, vectors, grid). 256 colors: xterm 196/160/88
on index 16; fewer colors: `COLOR_RED` with `A_BOLD`/normal/`A_DIM` on `COLOR_BLACK`. `wbkgd` paints every window
and stdscr. The Moon uses a text glyph (`●◐○◑` by phase) in night mode because emoji ignore color.
Colors are initialised whenever the terminal supports them; `-c` only chooses day colors.

### D9. Layers and braille
Layer order, bottom to top: Starlink → vectors → stars → constellation figures → planets → Moon → stations →
grid/cardinals → corner lines → toast → help. Each braille layer has its own mask buffer (bounds-checked, sized
to the window); within a cell the higher layer wins outright. Starlink dots use a sub-cell projection.

### D10. Bounded satellite cost
Stations propagate every frame. Starlink propagates at most 4 times per wall-clock second, whatever the speed.
An SGP4 call counter makes this testable (≤ 4 × N per second at 3600x).

### D11. Next pass
Scan forward in 20 s steps up to 72 h for altitude > 10° with the ISS sunlit and the observer dark, then refine
edges by bisection to 1 s. Cached while `computed_at ≤ jd < pass_end`; recomputed outside that window, when data
changes, at most twice per wall second. Gauged against Skyfield `find_events` (geometric rise/culmination/set).

### D12. Motion vectors
Satellites: `r + v·10 s` in TEME, converted at `jd + 10 s`, no second SGP4 call; only for drawn satellites.
Bodies: RA/Dec at `jd + 24 h` projected with the *current* sidereal time, i.e. motion against the stars.
Lines start at the object, are clipped at the horizon circle, capped at 30° of arc, skipped below one braille dot.

### D13. Rotation
`Conf.rotation` (radians) is added to the projected polar angle; cardinal letters and grid spokes are placed
through the same projection. `←`/`→` step 15°, `↓` resets.

### D14. Windows
`set_escdelay` behind `NCURSES_VERSION`; `feed.c` compiles to "downloads unsupported"; wall clock via Win32.

## Risks / Trade-offs

- [Transcription bug] → Vallado suite at 1 m / 1 mm/s, velocity included.
- [CelesTrak policy or URL changes] → status-code handling, 2 h marker, cache kept, message on first frame.
- [Launch delay] → at most two short downloads per day; `--offline`; 5 s connect timeout.
- [Hostile/garbled feed] → header-driven parsing, range checks, no names displayed, count guard, `mkstemp`.
- [Night vision leaks] → roles cover every draw call; a smoke check parses SGR codes; the backlight, OS chrome
  and terminal padding are outside our control and the README says so.
- [Stale data in the field] → toast shows data age at launch; element age > 14 days hides sets.
- [Upstream appetite] → slices are independent; the maintainer has not been asked (Oskar's call).

## Migration Plan

No migration: flags keep meaning and defaults. New: stations are on by default, which implies one launch-time
download per feed per 12 h; `--offline` restores zero network. Rollback: toggle any layer off, or `--offline`.
