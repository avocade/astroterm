## ADDED Requirements

### Requirement: Live display toggles
The application SHALL toggle each display option at runtime from a single keypress, with the change visible on
the next frame and without restarting. CLI flags SHALL set only the initial state.

| Key | Effect |
|-----|--------|
| `c` | terminal colors on/off |
| `C` | constellation figures on/off |
| `N` | constellation names on/off |
| `g` | azimuthal grid on/off (cardinal letters when off) |
| `u` | Unicode glyphs on/off |
| `b` | braille constellation lines on/off (only takes effect while Unicode is on) |
| `m` | metadata panel on/off |
| `r` | red night-vision filter on/off |
| `i` | space stations (ISS, Tiangong) on/off |
| `x` | Starlink overlay on/off |
| `X` | Starlink "sunlit only" filter on/off |
| `v` | motion vectors on/off |
| `+` / `=` | show fainter stars (magnitude threshold +0.5, max 8.0) |
| `-` | show fewer stars (magnitude threshold -0.5, min -1.5) |
| `space`, `>` / `.`, `<` / `,`, `n` | time controls (see sim-clock) |
| `?` | help modal |
| `q`, `ESC` | quit (ESC closes the help modal first when it is open) |

#### Scenario: Toggle constellations live
- **WHEN** astroterm runs without `-C` and the user presses `C`
- **THEN** constellation figures are drawn from the next frame on, and pressing `C` again removes them

#### Scenario: Flags set the initial state
- **WHEN** astroterm starts with `-g` and the user presses `g`
- **THEN** the grid disappears and cardinal direction letters are shown

#### Scenario: Braille without Unicode
- **WHEN** Unicode is off and the user presses `b`
- **THEN** the braille setting is stored and a toast reports that braille needs Unicode (`u`)

#### Scenario: Threshold bounds
- **WHEN** the magnitude threshold is 8.0 and the user presses `+`
- **THEN** the threshold stays at 8.0

#### Scenario: Quit-on-any preserved
- **WHEN** astroterm runs with `--quit-on-any`
- **THEN** every keypress quits, exactly as before

### Requirement: Help modal
Pressing `?` SHALL open a centered, bordered overlay listing every key and its current state (on/off/value). The
sky SHALL keep animating behind it. `?` or `ESC` SHALL close it. Pressing any other key while it is open SHALL
still perform that key's action and keep the modal open, so the listed state updates in place.

#### Scenario: Open and close
- **WHEN** the user presses `?` and then `ESC`
- **THEN** the modal appears, and then disappears without quitting the application

#### Scenario: State shown live
- **WHEN** the modal is open and the user presses `x`
- **THEN** the Starlink row changes from "off" to "on" in the modal

#### Scenario: Small terminal
- **WHEN** the terminal is too small to fit the modal
- **THEN** the modal is clipped to the screen without crashing, and a one-line "? help: enlarge terminal" hint is shown if even the title will not fit

### Requirement: Toast feedback
Every key action SHALL show a one-line toast at the bottom of the screen describing the new state (for example
"Starlink: on, 10,687 sats" or "Speed: 60x"). The toast SHALL disappear after 2 seconds of wall-clock time.

#### Scenario: Toast expires
- **WHEN** the user presses `g`
- **THEN** a toast reading "Grid: on" appears and is gone 2 seconds later

### Requirement: Arrow and function keys never quit
Multi-byte key sequences (arrows, function keys) SHALL be decoded as single keys and SHALL NOT be interpreted as
`ESC`. A lone `ESC` SHALL still be recognised within 100 ms.

#### Scenario: Arrow key
- **WHEN** the user presses the up arrow
- **THEN** the application keeps running
