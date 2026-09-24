## ADDED Requirements

### Requirement: Starlink layer
The application SHALL provide a Starlink overlay, toggled with `x` and enabled at startup by `--starlink`, off by
default. It SHALL draw every sunlit Starlink above the horizon as a small blue mark; `X` SHALL also draw eclipsed
ones, dimmer. Toggling SHALL toast "Starlink: on, <sunlit> sunlit of <above> above horizon".

#### Scenario: Toggle on
- **WHEN** the user presses `x` with Starlink data cached
- **THEN** blue dots appear for sunlit Starlinks above the horizon and the toast gives both counts

#### Scenario: Winter midnight
- **WHEN** it is local midnight in December at 59°N
- **THEN** no Starlinks are drawn (the 550 km shell is entirely in Earth's shadow), and `X` reveals them dimmed

### Requirement: Never overshadow the stars
The Starlink layer SHALL be the lowest layer: any cell shared with a star, planet, Moon, station, line or label
shows that object. It SHALL carry no labels.

#### Scenario: Shared cell
- **WHEN** a Starlink and Vega project to the same cell
- **THEN** the cell shows Vega

### Requirement: Tiny glyphs
In Unicode mode each satellite SHALL be a single braille dot at its sub-cell position (2×4 per cell), so several
satellites in a cell show as several dots. In ASCII mode each satellite SHALL be a `,` (faint stars use `.`).

#### Scenario: A train in one cell
- **WHEN** three satellites fall in one cell at different sub-cell positions
- **THEN** the cell shows one braille glyph with three dots raised

### Requirement: Bounded cost
Starlink positions SHALL be recomputed at most 4 times per wall-clock second at any speed.

#### Scenario: Fast-forward
- **WHEN** the overlay is on at 3600x for 10 seconds with N satellites
- **THEN** at most 40 × N SGP4 propagations are made
