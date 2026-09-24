## 1. Sim clock (slice 1, fixes upstream #77)

- [ ] 1.1 Reproduce #77 on unmodified upstream (SIGSTOP 60 s, SIGCONT, compare with `date`) and record the result
- [ ] 1.2 `src/sim_clock.c`: anchored clock, pause, forward ladder, jump-to-now; platform realtime helper
- [ ] 1.3 Unit tests with injected time: slow frames, suspended process, pause, ladder, snap, now
- [ ] 1.4 Wire into `main.c`; speed/paused line in the metadata panel; re-run the #77 reproduction on the fork

## 2. Live controls (slice 2)

- [ ] 2.1 Loop order (draw → doupdate → input); `keypad` + `set_escdelay` behind `NCURSES_VERSION`; colors initialised whenever supported
- [ ] 2.2 `src/ui.c`: key table (handler, help row, toast), pure `ui_handle_key` with a context struct; toast; help modal
- [ ] 2.3 Unit tests: every key in the table, bounds, braille-needs-unicode, ESC-closes-help, quit-on-any
- [ ] 2.4 Dome rotation (`←`/`→`/`↓`): projection offset, cardinal letters and grid through the projection; launch toast (`? for keys`, 0°,0° warning)

## 3. Night vision (slice 3)

- [ ] 3.1 Render roles + `palette_attr()`; day/night palettes (256-color reds on index 16, 8-color fallback); `wbkgd` everywhere
- [ ] 3.2 Route every draw call through roles; Moon text glyph in night mode; `--night` flag and `r` key; monochrome toast

## 4. Stations and satellite data (slice 4, closes upstream #46)

- [x] 4.1 Golden vectors: python-sgp4 + Skyfield, live sets, a 6-digit catalog row, Vallado near-Earth suite, ISS passes (`scripts/gen_sgp4_vectors.py`)
- [ ] 4.2 `src/sgp4.c` passes the suite at 1 m / 1 mm/s
- [ ] 4.3 `src/omm.c`: header-driven CSV, quoted fields, locale-independent numbers, range checks; tests incl. comma locale
- [ ] 4.4 `src/satellite.c`: topocentric, sunlit, element age, pass prediction; tests against Skyfield
- [ ] 4.5 `src/feed.c`: cache dir, freshness, 2 h attempt marker, curl spawn into `mkstemp`, status codes, count guard; tests with a fake curl
- [ ] 4.6 Station markers + ISS corner line; `i` key; `--offline` flag

## 5. Starlink (slice 5)

- [ ] 5.1 Per-layer braille masks (bounds-checked); sub-cell projection
- [ ] 5.2 Starlink layer: 4 Hz wall-clock cadence with a call counter, sunlit-only default, `X`, `x` key, `--starlink` flag; cadence test at 3600x

## 6. Motion vectors (slice 6)

- [ ] 6.1 Pure position queries for Sun/planets/Moon; satellite look-ahead from velocity
- [ ] 6.2 Draw vectors (dim, beneath, clipped, capped 30°, min one dot); `v` key

## 7. Verification and hand-off

- [ ] 7.1 `meson test` green; every slice commit builds and passes (`git rebase --exec`)
- [ ] 7.2 tmux smoke test: every key, help modal, stations, Starlink; parse `capture-pane -e` SGR codes to prove night vision emits only reds on black; SGP4 call counter at 3600x
- [ ] 7.3 README: keys, flags, data/cache/offline, a field checklist (brightness, OS red filter, keyboard backlight, tmux status)
- [ ] 7.4 Build the binary and notify Oskar
