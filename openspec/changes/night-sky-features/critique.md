## Critique: night-sky-features (13-lens panel, 2026-09-25)

Panel: De-slopify, Inversion, Red Team, Musk, User, Alexander, DDD, Deutsch-Popper, Willison, Rubin, Bergman,
Insight, Goodhart. Each read the change plus the astroterm source. "Verified" means Odrade reproduced the claim
against the live system or the source, not just the lens's say-so.

### Critical (must fix before proceeding)

- **The TLE format cannot carry any satellite catalogued since July 2026.** *Red Team* (also raised as a possibility
  by DDD, Willison, Insight). **Verified:** CelesTrak's `last-30-days` CSV has 241 objects, all with catalog
  numbers ≥ 100465, including 137 Starlinks; the `FORMAT=tle` Starlink feed tops out at 69998. A TLE-only design
  silently drops every fresh Starlink train. **Fix:** production format is OMM CSV (`FORMAT=csv`), 32-bit catalog
  numbers; the TLE parser is deleted.
- **Number parsing is locale-dependent.** *Red Team.* **Verified:** `main.c:119` calls `setlocale(LC_ALL, "")`
  before any data would be parsed, so `strtod` stops at "." under sv_SE/de_DE and positions go silently wrong with
  CI green. **Fix:** a locale-independent decimal parser; a unit test under a comma locale.
- **The Moon is a colour emoji, which night vision cannot tint.** *Alexander, De-slopify* (independent angles:
  structure and correctness). **Verified:** `normalize_emoji()` strips the VS15 text selector from 🌑…🌘.
  **Fix:** text-glyph Moon in night mode.
- **Fast-forward bypasses the Starlink throttle.** *Musk, Red Team, Deutsch, Goodhart, De-slopify.* "Or sim time
  moved > 10 s" fires every frame at ≥ 240x, about 257k SGP4 calls/s. **Fix:** a hard 4 Hz wall-clock cap
  regardless of speed; a call-counter test at 3600x (Goodhart's instrument, replacing CPU %).
- **`set_escdelay` breaks the Windows (PDCurses) build.** *De-slopify.* **Fix:** guard with `NCURSES_VERSION`.
- **Nothing hides satellites far from their element epoch.** *De-slopify* (critical), *DDD* ("age" means two
  things), *Red Team*. `--datetime` a month back draws Starlinks that were not launched yet. **Fix:** hide sets with
  |sim − epoch| > 14 days, and name *element age* (sim time) separately from *feed age* (wall time).

### High (should fix before proceeding)

- **CelesTrak serves each group once per update per IP, then HTTP 403.** *Red Team.* **Verified** first-hand
  ("GP data has not updated since your last successful download… once every 2 hours"). **Fix:** read the status
  code, treat 403 as "not updated yet", and never ask more often than every 2 h (tracked by a marker file across
  processes).
- **Body motion vectors show Earth's rotation, not the body's travel.** *Bergman, Musk, Alexander, Deutsch,
  Willison, User, De-slopify.* Seven lenses, but one cheap insight: in alt/az a 1 h look-ahead is ~15°/h for every
  object, stars included. **Resolution (Oskar's call, defaulted):** satellites show apparent motion (10 s ahead);
  Sun, Moon and planets show their motion *against the stars* (24 h ahead at fixed sidereal time), which is the
  information unique to them.
- **One braille buffer merges and recolours layers.** *Alexander (critical), DDD, Red Team, De-slopify.*
  **Fix:** a separate mask per layer; the higher layer owns a contested cell.
- **Night vision collapses sunlit vs eclipsed.** *Alexander, DDD, De-slopify.* **Fix:** sunlit = medium red,
  eclipsed = dim red; attributes live in a per-role table (pairs cannot carry `A_DIM`/`A_BOLD`); background uses
  xterm index 16 (true black) on 256-color terminals, because themes remap `COLOR_BLACK`.
- **Slice 4 (stations) has no data source.** *Bergman, DDD, Goodhart, Deutsch, De-slopify.* **Fix:** feed read
  and fetch move into slice 4; Starlink only adds a group.
- **Refresh model undesigned; `getch()` refreshes stdscr mid-frame.** *De-slopify.* **Fix:** read input after
  `doupdate()`; explicit draw order stdscr → sky → panel → overlays.
- **Fetch architecture.** *Musk* (replace async spawn/poll with a blocking fetch before curses starts), *Red Team /
  De-slopify* (fixed `.tmp` races, Ctrl-C orphans, `~/.curlrc`, redirects, size). **Fix:** fetch before `initscr`
  (no async state machine, no orphaned children), `mkstemp` temp file, `curl -q --proto =https --proto-redir =https
  --max-filesize 20M --connect-timeout 5 --max-time 60`, reject a feed with < 50% of the previous count.
- **Offline-first fails on the first outing.** *User, Bergman.* **Fix:** any enabled satellite layer refreshes
  *both* feeds at launch, so the cache is warm before you drive out; README "before you leave" step.
- **Starlink default contradicts "can I see it?".** *Rubin, User, Alexander.* **Fix:** draw sunlit satellites
  only; `X` reveals the eclipsed ones.
- **The next-pass line is untested and hidden.** *Deutsch, User.* **Fix:** golden passes from Skyfield
  `find_events`; the ISS line shows whenever stations are on, with rise direction and "(in shadow)".
- **Keys do not mirror flags.** *User, Inversion, Alexander, Willison, De-slopify.* **Fix:** new flags are
  long-only (`--night`, `--starlink`, `--offline`); drop the mirroring claim for new keys.
- **Sim clock:** cut reverse time (*Musk, Rubin*); `timespec_get` is C11 (*Musk, Willison, De-slopify*); timers
  must be monotonic (*Red Team*). **Fix:** forward ladder with pause; platform realtime helper for the anchor;
  monotonic `stopwatch` for toasts and cadence.
- **Gauges written and graded by the implementer.** *Goodhart, Deutsch, Willison.* **Fix:** Vallado verification
  suite + velocity at ≤ 1 m / 1 mm/s (done); night-vision check parses SGR codes from `tmux capture-pane -e`;
  reproduce #77 on unmodified upstream before claiming the fix; build and test every slice commit.
- **The maintainer has not been asked.** *Bergman (critical), Insight, Musk, Inversion.* Outward-facing, so it is
  Oskar's action, not the implementer's; the fork is useful on its own either way.
- **The night itself is never rehearsed; backlight dominates hue.** *User, Rubin, Bergman, Insight, Red Team.*
  **Fix:** README field checklist (minimum brightness, macOS Color Filters red tint, keyboard backlight, `tmux set
  status off`); the claim is scoped to what a terminal program controls. The field trial is Oskar's.

### Medium (fix during implementation)

- Glossary: *above horizon*, *sunlit*, *observer dark*, *naked-eye pass*, *feed age* vs *element age*, *stations*
  (the two tracked objects, not the CelesTrak group). *DDD, Willison.*
- One owner per state: display toggles in `Conf`, transient UI in `UiState`, time and speed in `SimClock`.
  *DDD, Alexander.* One table drives keys, help rows and toasts. *Alexander.*
- Pure position queries for bodies (vectors must not overwrite the frame's positions). *DDD, Alexander.*
- NaN guards: `!(r <= 1)`, `isfinite` on SGP4 output, element range checks. *Red Team.*
- UI text off the dome: toast and ISS line in the bottom-left corner. *Alexander.*
- Arrow keys rotate the dome ("face the sky"). *User, Inversion, Rubin.* Cheap once arrows are decoded.
- "? for keys" hint on launch. *User.*
- Metadata panel is 6×45 and overflows. *De-slopify.*
- Satellite vectors: 10 s, sunlit only, capped in degrees. *Red Team, Alexander, De-slopify.*
- ASCII Starlink glyph `,` (the faint-star glyph is `.`). *Alexander.*
- Merge `controls` + `overlay` into `ui.c`. *Musk.*
- **Cut constellation names** (*Musk, Rubin, Bergman*; *Alexander* and *De-slopify* found label-rule and Serpens
  problems). Separate upstream PR for #35 later.
- Scenario fixes: winter-midnight Starlink count is 0, not "a few"; the zenith cap is in degrees. *De-slopify.*

### Low / documented limitations

- Stars are J2000 rotated by GMST of date (no precession), so they sit ~0.37° off; satellites are of-date. Below
  display resolution (a cell spans 2–5°); an upstream fix, not this change. *DDD, Deutsch, Willison, Goodhart, De-slopify.*
- "Visible pass" uses Sun < −6°; above ~60.6°N the Sun never gets that low around midsummer. *Red Team.*
- Windows: compiles, no background download (no way to test MSVC here). *Willison, De-slopify.*
- TLE two-digit-year pivot, `mvwaddstr_truncate` 2048-byte buffer: pre-existing; noted. *Red Team.*

### Opportunities (Inversion, User)

Backlog, not this change: event timeline with jump-to-next-pass keys and a bell at T-60 s; headless
`astroterm passes` for status bars and cron; `GROUP=visual` bright-satellite feed; Starlink train detector
(launch designator); demo GIF via the deterministic `--datetime`; Termux build; aurora Kp; persisted toggles and
location (upstream #24); goodwill PR for upstream #115.

### Rejected findings (with reasons)

- *Musk:* delete `--offline`. Stations are on by default, so launch implies a network request; a hard off switch
  is the privacy floor.
- *Musk:* delete `X`. Kept but inverted (sunlit-only default) per Rubin/User, so it now reveals rather than filters.
- *Musk:* two red tiers only. The third tier is what keeps sunlit and eclipsed apart at night (Alexander/DDD).
- *User:* persisted state file and location. Real friction, but it is upstream #24's scope; mitigated by a toast
  at 0°,0° and a shell alias.
- *Rubin:* auto night mode at dusk. It would change the default for every user at night; `--night` in an alias
  gives the field outcome with no surprise. Backlog.
- *Alexander:* draw grid and figures beneath objects. Changes upstream's existing look; separate PR.
- *Deutsch:* gate the bright satellite tier on Sun < −6°. With sunlit-only default it only affects daytime use.
- *Insight (tanhā):* "features costed as free". Reads identically in Rubin's voice (satellite vectors belong, body
  vectors only half do); Rubin's copy kept, Insight's dropped.
- *Insight:* ISS deorbit ~2030 as impermanence. True and irrelevant to a 2026 change.
- *Goodhart:* "#77 test compares the clock to itself". Strip optimizer and cheapest action and the residue is
  Lens 8's finding; Lens 8's copy kept, Goodhart's instrument (reproduce on upstream with SIGSTOP/SIGCONT) folded in.
- *Goodhart:* CPU gate read at 1x. Duplicate of Red Team/Deutsch; kept theirs, folded in the call-counter instrument.
- *De-slopify:* "D8 rationale false (masks do not leak)". Accepted: the rationale is struck, the per-frame clear
  stays because layers now share nothing.

### Summary

The spec was right about the seed (a field instrument: red, the ISS, keys that do not kill it) and wrong about the
world in two verified ways: the TLE format has stopped carrying new satellites, and CelesTrak rate-limits per
update per IP. Biggest remaining risk is not maths but the night itself (backlight, first outing offline), which
only Oskar can test. Next action: revise the spec (v2) with the fixes above, then implement in slice order.
