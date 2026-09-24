## ADDED Requirements

### Requirement: Satellite vectors
The application SHALL offer motion vectors (`v`, off by default). While on, each drawn station and Starlink SHALL
have a line from its position to its apparent position 10 seconds of simulation time later, computed from the SGP4
velocity (no second propagation).

#### Scenario: ISS vector
- **WHEN** vectors are on and the ISS is at 40° altitude
- **THEN** a line runs from the ISS toward where it will be 10 simulated seconds later

#### Scenario: Cost unchanged
- **WHEN** vectors are on with the Starlink overlay
- **THEN** the number of SGP4 propagations is the same as with vectors off

### Requirement: Body vectors show motion against the stars
While vectors are on, the Sun, Moon and each planet above the horizon SHALL have a line from its position to where
it will be among the stars 24 hours later: its RA/Dec at `jd + 24 h` projected with the current sidereal time.
The daily rotation shared by every star SHALL NOT be drawn.

#### Scenario: Moon drift
- **WHEN** vectors are on and the Moon is up
- **THEN** its line points east along the ecliptic and spans about 13°

#### Scenario: Retrograde
- **WHEN** a planet is in retrograde motion
- **THEN** its line points west

### Requirement: Subtle rendering
Vectors SHALL be drawn in the dim role, beneath stars and object glyphs, as braille lines in Unicode mode and ASCII
lines otherwise, clipped at the horizon, capped at 30° of arc, and not drawn when shorter than one braille dot.
`v` SHALL toast "Vectors: satellites 10 s, Sun/Moon/planets 24 h vs stars".

#### Scenario: Stars stay visible
- **WHEN** a vector crosses a star's cell
- **THEN** the cell shows the star
