## ADDED Requirements

### Requirement: Live display toggles
The application SHALL toggle each display option at runtime from a single keypress, visible on the next frame and
without restarting. CLI flags SHALL set only the initial state. One table SHALL define each key, its help row and
its toast text.

| Key | Effect |
|-----|--------|
| `c` | day colors on/off |
| `C` | constellation figures on/off |
| `g` | azimuthal grid on/off (cardinal letters when off) |
| `u` | Unicode glyphs on/off |
| `b` | braille constellation lines on/off (needs Unicode) |
| `m` | metadata panel on/off |
| `r` | red night vision on/off |
| `i` | space stations (ISS, Tiangong) on/off |
| `x` | Starlink overlay on/off |
| `X` | also show Starlinks in Earth's shadow (dimmed) |
| `v` | motion vectors on/off |
| `+`/`=`, `-` | magnitude threshold ±0.5 (bounded −1.5 … 8.0) |
| `space`, `>`/`.`, `<`/`,`, `n` | pause, faster, slower, now (see sim-clock) |
| `←`, `→`, `↓` | rotate the dome 15° left/right, reset |
| `?` | help modal |
| `q`, `ESC` | quit (`ESC` closes the help modal first) |

#### Scenario: Toggle constellations live
- **WHEN** astroterm runs without `-C` and the user presses `C`
- **THEN** constellation figures are drawn from the next frame on, and pressing `C` again removes them

#### Scenario: Flags set the initial state
- **WHEN** astroterm starts with `-g` and the user presses `g`
- **THEN** the grid disappears and cardinal letters are shown

#### Scenario: Braille without Unicode
- **WHEN** Unicode is off and the user presses `b`
- **THEN** the setting is stored and the toast says braille needs Unicode (`u`)

#### Scenario: Threshold bounds
- **WHEN** the threshold is 8.0 and the user presses `+`
- **THEN** the threshold stays 8.0

#### Scenario: Quit-on-any preserved
- **WHEN** astroterm runs with `--quit-on-any`
- **THEN** every keypress quits, as before

### Requirement: Help modal
Pressing `?` SHALL open a centered, bordered overlay listing every key and its current state. The sky SHALL keep
animating behind it. `?` or `ESC` SHALL close it; other keys SHALL act and keep it open so the state updates in
place. On a terminal too small for it, the modal SHALL be clipped without crashing.

#### Scenario: Open and close
- **WHEN** the user presses `?` and then `ESC`
- **THEN** the modal appears and then disappears without quitting

#### Scenario: State shown live
- **WHEN** the modal is open and the user presses `x`
- **THEN** the Starlink row changes from "off" to "on"

### Requirement: Toasts
Each key action SHALL show a one-line toast in the bottom-left corner for 2 seconds of monotonic time. On launch
the toast SHALL report satellite data problems or data age if any, else warn "Location 0°, 0°: use -i <city> or
-a/-o" when no location was given, else read "? for keys".

#### Scenario: Toast expires
- **WHEN** the user presses `g`
- **THEN** "Grid: on" appears and is gone 2 seconds later

### Requirement: Arrow and function keys never quit
Multi-byte key sequences SHALL be decoded as single keys and SHALL NOT be taken as `ESC`. A lone `ESC` SHALL
still register within 100 ms on ncurses.

#### Scenario: Arrow key
- **WHEN** the user presses the up arrow
- **THEN** the application keeps running

### Requirement: Face the sky
`←`/`→` SHALL rotate the whole dome (objects, lines, grid, cardinal letters) by 15° per press; `↓` SHALL reset.

#### Scenario: Facing south
- **WHEN** the user presses `→` twelve times
- **THEN** "S" is at the top of the dome and "N" at the bottom
