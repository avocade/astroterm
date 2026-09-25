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
| `U` | ask to update the Starlink data now |
| `z` / `Z` | zoom in / out: 1x (whole sky), 2x (quadrants), 4x (4×4 tiles) |
| `h j k l`, arrows | move one tile left/down/up/right when zoomed |
| `1`–`4` | look at a quadrant (2x): top-left, top-right, bottom-left, bottom-right |
| `[` / `]`, `R` | rotate the dome 15°, reset the view (1x, no rotation) |
| `+`/`=`, `-` | magnitude threshold ±0.5 (bounded −1.5 … 8.0) |
| `space`, `>`/`.`, `<`/`,`, `n` | pause, faster, slower, now (see sim-clock) |
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
`[`/`]` SHALL rotate the whole dome (objects, lines, grid, cardinal letters) by 15° per press; `R` SHALL reset
rotation and zoom.

#### Scenario: Facing south
- **WHEN** the user presses `]` twelve times
- **THEN** "S" is at the top of the dome and "N" at the bottom

### Requirement: Zoom into tiles of the sky
The application SHALL offer three zoom levels: 1x shows the whole sky exactly as upstream does; 2x shows one of
the window's four quadrants; 4x one tile of a 4×4 grid. `z`/`Z` SHALL zoom in/out (zooming out goes to the
quadrant containing the tile; zooming in goes to the sub-tile nearest the zenith, bottom-left from 1x).
`h j k l` and the arrow keys SHALL move one tile, stopping at the edges. `1`–`4` SHALL show that quadrant at 2x
from any level. Every layer SHALL follow the view, and nothing below the horizon or outside the window SHALL be
drawn. While zoomed, the corner SHALL show the level, a minimap of the current tile, and the compass direction and
altitude at the tile's centre.

#### Scenario: Default is the whole sky
- **WHEN** astroterm starts
- **THEN** zoom is 1x and the sky looks as it does upstream

#### Scenario: Jump to a quadrant
- **WHEN** the view is 1x with no rotation and the user presses `3`
- **THEN** zoom is 2x on the bottom-left quadrant, looking south-east

#### Scenario: Walk the tiles
- **WHEN** zoom is 4x on the top-left tile and the user presses `l` three times and `h` once
- **THEN** the view is on the third tile of the top row, and a further `k` stays put

#### Scenario: Arrows at 1x
- **WHEN** zoom is 1x and the user presses an arrow key
- **THEN** nothing moves and the toast says to zoom in with `z` first
