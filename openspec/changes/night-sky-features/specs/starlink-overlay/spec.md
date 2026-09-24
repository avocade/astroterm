## ADDED Requirements

### Requirement: Starlink layer
The application SHALL provide a Starlink overlay, toggled with the `x` key and enabled at startup by `--starlink` /
`-x`. When it is on, every Starlink satellite above the horizon SHALL be drawn as a small blue mark. The layer SHALL be off by default.

#### Scenario: Toggle on
- **WHEN** the user presses `x` with Starlink data cached
- **THEN** blue dots appear for all Starlink satellites above the horizon and the toast reads "Starlink: on, <N> above horizon"

### Requirement: Never overshadow the stars
The Starlink layer SHALL be drawn before (beneath) stars, planets, the Moon and space stations, so any cell shared
with another object shows that object. It SHALL carry no labels.

#### Scenario: Shared cell
- **WHEN** a Starlink satellite and Vega project to the same terminal cell
- **THEN** the cell shows Vega

### Requirement: Tiny glyphs
In Unicode mode each satellite SHALL be a single braille dot at its sub-cell position (2×4 dots per cell), so
several satellites in one cell appear as several dots. In ASCII mode each satellite SHALL be a `.`, and with
colors off a `,` so it is distinguishable from a faint star.

#### Scenario: A train in one cell
- **WHEN** three satellites fall in the same cell at different sub-cell positions (Unicode)
- **THEN** the cell shows one braille glyph with three dots raised

### Requirement: Sunlit emphasis
Sunlit satellites SHALL be drawn in bright blue (bold) and eclipsed satellites in dim blue. `X` SHALL toggle
"sunlit only", hiding eclipsed satellites.

#### Scenario: Deep night
- **WHEN** it is local midnight in winter at 59°N and "sunlit only" is on
- **THEN** only the few satellites still in sunlight are drawn

### Requirement: Bounded cost
Starlink positions SHALL be recomputed at most 4 times per second of wall-clock time (and on every frame in which
simulation time jumps by more than 10 s), so the overlay keeps astroterm under 25% of one CPU core at 24 fps on a
2020-era laptop with ~10,000 satellites.

#### Scenario: Idle CPU
- **WHEN** the overlay is on at speed 1 for 60 seconds
- **THEN** average CPU use of the process stays below 25% of one core
