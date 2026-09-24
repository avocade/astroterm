## 1. Foundation: live controls (PR slice 1)

- [ ] 1.1 Add `struct UiState` (help open, toast text + expiry, sunlit-only, names, vectors, stations, starlink, night) and `controls_handle_key()` in `src/controls.c` as a pure function returning an action enum
- [ ] 1.2 Unit tests `test/controls_test.c`: every key in the table, threshold bounds, braille-without-unicode toast, quit/ESC-closes-help, quit-on-any
- [ ] 1.3 Enable `keypad(stdscr, TRUE)` + `set_escdelay(25)`; initialise colors whenever `has_colors()`; clear the braille layer once per frame; bounds-check braille writes
- [ ] 1.4 Wire the main loop to `controls_handle_key()` and perform side effects (metadata window resize, palette re-apply, quit)
- [ ] 1.5 `src/overlay.c`: toast (2 s wall clock, bottom line) and the `?` help modal showing live state, clipped on small terminals

## 2. Sim clock and time controls (PR slice 2)

- [ ] 2.1 `src/sim_clock.c`: anchored clock, pause, speed ladder with snapping, jump-to-now, last-nonzero sign; wall time via `timespec_get`
- [ ] 2.2 Unit tests `test/sim_clock_test.c` (pure functions with injected `now`)
- [ ] 2.3 Replace `julian_date += dt × speed` in `main.c`; show speed/paused and signed elapsed time in the metadata panel

## 3. Night vision (PR slice 3)

- [ ] 3.1 Named color pairs and `palette_apply(night, colors)` (256-color red tiers, 8-color `COLOR_RED` + `A_DIM` fallback), `wbkgd` on all windows
- [ ] 3.2 Route render call sites through named pairs (stars/labels normal tier, bodies bright, grid/cardinals/lines dim); `--night`/`-n` flag and `r` key; monochrome toast

## 4. SGP4 and space stations (PR slice 4)

- [ ] 4.1 Generate golden vectors with python-sgp4 + Skyfield (script in `scripts/gen_sgp4_vectors.py`, output committed as a test header)
- [ ] 4.2 `src/sgp4.c`: near-Earth SGP4 (WGS-72), deep-space rejection, error codes
- [ ] 4.3 `src/satellite.c`: TLE parse (checksum, columns, name sanitising), epoch → JD, TEME → az/alt, sunlit test, look-ahead from velocity
- [ ] 4.4 Unit tests `test/sgp4_test.c` and `test/satellite_test.c` (10 m position, 0.1° az/alt, hostile names, bad checksums, shadow cases)
- [ ] 4.5 Render ISS/Tiangong markers (glyph, label, color, dimmed in shadow) above planets; `i` key
- [ ] 4.6 Next-pass prediction with the cached validity window and rate limit; ISS line in the metadata panel

## 5. TLE data and Starlink (PR slice 5)

- [ ] 5.1 `src/tle_cache.c`: cache dir resolution, freshness, `posix_spawnp` curl with stdio → `/dev/null`, `waitpid` polling, validate-then-rename, 15 min back-off, kill on exit, Windows stub; `--offline`/`-O`
- [ ] 5.2 Unit tests for path resolution, freshness and validate-then-rename (no network: a fake feed file)
- [ ] 5.3 Starlink layer: 4 Hz propagation cadence, sub-cell braille dots / ASCII fallback, sunlit/eclipsed tiers, `X` sunlit-only filter, `x` key, `--starlink`/`-x` flag, drawn first

## 6. Motion vectors (PR slice 6)

- [ ] 6.1 Look-ahead positions for Sun/planets/Moon (1 h) and satellites (30 s, velocity extrapolation), sign from sim clock
- [ ] 6.2 Draw dim braille/ASCII vectors beneath stars with horizon clipping, R/3 cap, sub-cell minimum; `v` key, `--vectors` flag

## 7. Constellation names (PR slice 7)

- [ ] 7.1 Abbreviation → IAU name table for all 88 constellations; centroid of figure stars as a unit vector
- [ ] 7.2 Render names centered on the centroid in the dim tier; `N` key, `--constellation-names` flag; test that every abbreviation in the data resolves

## 8. Verification and hand-off

- [ ] 8.1 Full `meson test` green, no new compiler warnings in new files
- [ ] 8.2 tmux smoke test: launch the real binary, press every key, capture panes, check the help modal, night vision, stations and Starlink render and that CPU stays below 25% with Starlink on
- [ ] 8.3 README: new keys, flags, data/cache/offline notes; bash completions include the new flags
- [ ] 8.4 One commit per PR slice on the fork branch; build the binary and notify Oskar
