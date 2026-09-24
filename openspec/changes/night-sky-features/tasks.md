## 1. Sim clock (slice 1, fixes upstream #77)

- [x] 1.1 Reproduce #77 on unmodified upstream (SIGSTOP 60 s, SIGCONT, compare with `date`) and record the result
- [x] 1.2 `src/sim_clock.c`: anchored clock, pause, forward ladder, jump-to-now; platform realtime helper
- [x] 1.3 Unit tests with injected time: slow frames, suspended process, pause, ladder, snap, now
- [x] 1.4 Wire into `main.c`; speed/paused line in the metadata panel; re-run the #77 reproduction on the fork

## 2. Live controls (slice 2)

- [x] 2.1 Loop order (draw → doupdate → input); `keypad` + `set_escdelay` behind `NCURSES_VERSION`; colors initialised whenever supported
- [x] 2.2 `src/ui.c`: key table (handler, help row, toast), pure `ui_handle_key` with a context struct; toast; help modal
- [x] 2.3 Unit tests: every key in the table, bounds, braille-needs-unicode, ESC-closes-help, quit-on-any
- [x] 2.4 Dome rotation (`←`/`→`/`↓`): projection offset, cardinal letters and grid through the projection; launch toast (`? for keys`, 0°,0° warning)

## 3. Night vision (slice 3)

- [x] 3.1 Render roles + `palette_attr()`; day/night palettes (256-color reds on index 16, 8-color fallback); `wbkgd` everywhere
- [x] 3.2 Route every draw call through roles; Moon text glyph in night mode; `--night` flag and `r` key; monochrome toast

## 4. Stations and satellite data (slice 4, closes upstream #46)

- [x] 4.1 Golden vectors: python-sgp4 + Skyfield, live sets, a 6-digit catalog row, Vallado near-Earth suite, ISS passes (`scripts/gen_sgp4_vectors.py`)
- [x] 4.2 `src/sgp4.c` passes the suite at 1 m / 1 mm/s
- [x] 4.3 `src/omm.c`: header-driven CSV, quoted fields, locale-independent numbers, range checks; tests incl. comma locale
- [x] 4.4 `src/satellite.c`: topocentric, sunlit, element age, pass prediction; tests against Skyfield
- [x] 4.5 `src/feed.c`: cache dir, freshness, 2 h attempt marker, curl spawn into `mkstemp`, status codes, count guard; tests with a fake curl
- [x] 4.6 Station markers + ISS corner line; `i` key; `--offline` flag

## 5. Starlink (slice 5)

- [x] 5.1 Per-layer braille masks (bounds-checked); sub-cell projection
- [x] 5.2 Starlink layer: 4 Hz wall-clock cadence with a call counter, sunlit-only default, `X`, `x` key, `--starlink` flag; cadence test at 3600x

## 6. Motion vectors (slice 6)

- [x] 6.1 Pure position queries for Sun/planets/Moon; satellite look-ahead from velocity
- [x] 6.2 Draw vectors (dim, beneath, clipped, capped 30°, min one dot); `v` key

## 7. Verification and hand-off

- [x] 7.1 `meson test` green; every slice commit builds and passes (`git rebase --exec`)
- [x] 7.2 tmux smoke test: every key, help modal, stations, Starlink; parse `capture-pane -e` SGR codes to prove night vision emits only reds on black; SGP4 call counter at 3600x
- [x] 7.3 README: keys, flags, data/cache/offline, a field checklist (brightness, OS red filter, keyboard backlight, tmux status)
- [x] 7.4 Build the binary and notify Oskar

## 8. Left for the humans

- [ ] 8.1 Field trial under a dark sky: what failed, what was missing (Oskar)
- [ ] 8.2 Ask the upstream maintainer before opening PRs (Oskar)

## Implementation notes (2026-09-25)

- #77 reproduced on v1.2.0 before fixing: 51 s of sky time per 60.2 s of wall time. After: 60 s, including a
  25 s SIGSTOP.
- Found on the way and fixed as its own commit: `--datetime` was an hour late whenever local DST applies
  (`mktime()` in `string_to_time()`).
- Mutation checks: a 0.03% change to one SGP4 coefficient and dropping the ellipsoid flattening both fail the
  suite; a 2 m radius change correctly does not.
- Visible-pass logic cross-checked against Skyfield + DE421: no dark-sky ISS pass from Stockholm between
  2026-09-25 and 09-29, so the corner line falls back to the next pass marked daylight or twilight.
- Cost: ~10.8k Starlinks at 6.9% of one core at 1x, 7.8% at 3600x (4 Hz wall-clock cap).
