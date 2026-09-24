## ADDED Requirements

### Requirement: Look-ahead motion vectors
The application SHALL offer motion vectors (`v` key, `--vectors` flag; off by default). While they are on, the Sun,
the Moon, each planet, space station and visible Starlink satellite above the horizon SHALL be drawn with a line from its current screen position to its
projected position after a look-ahead interval: **1 hour** of simulation time for the Sun, Moon and planets, and
**30 seconds** for satellites. The line length therefore encodes apparent angular speed within each group.

#### Scenario: Moon vector
- **WHEN** vectors are on and the Moon is above the horizon
- **THEN** a line runs from the Moon toward where it will be one simulated hour later

#### Scenario: ISS vector
- **WHEN** vectors are on and the ISS is at 40° altitude
- **THEN** a line runs from the ISS toward its position 30 simulated seconds later

#### Scenario: Toggle live
- **WHEN** the user presses `v`
- **THEN** vectors appear or disappear on the next frame and the toast reads "Vectors: on (bodies 1 h, satellites 30 s)"

### Requirement: Direction follows the flow of time
The look-ahead SHALL take the sign of the current speed, so vectors always point where the object is heading on
screen. While paused, the sign of the last non-zero speed SHALL be used.

#### Scenario: Reverse
- **WHEN** time runs backwards
- **THEN** every vector points the opposite way compared to forward time

### Requirement: Subtle rendering
Vectors SHALL be drawn in the dim tier (dim red under night vision), beneath stars and all object glyphs, with no
arrowhead (the object glyph marks the tail). In Unicode mode they SHALL be braille lines, in ASCII mode ASCII
line characters. A vector SHALL be clipped at the horizon and SHALL be capped at one third of the dome radius.
A vector shorter than one cell SHALL NOT be drawn.

#### Scenario: Stars stay visible
- **WHEN** a vector crosses a cell containing a star
- **THEN** the cell shows the star

#### Scenario: Zenith pass cap
- **WHEN** the ISS passes the zenith and its 30 s look-ahead spans 35°
- **THEN** its vector is drawn at one third of the dome radius, not across the sky

### Requirement: Satellite vectors are cheap
Satellite look-ahead positions SHALL be computed from the SGP4 velocity (linear extrapolation in TEME, with the
Earth's rotation applied through sidereal time at the look-ahead instant), not by a second SGP4 propagation, and
SHALL be refreshed together with the satellite positions.

#### Scenario: Starlink cost unchanged
- **WHEN** vectors are on with the Starlink overlay
- **THEN** the number of SGP4 propagations per second is the same as with vectors off
