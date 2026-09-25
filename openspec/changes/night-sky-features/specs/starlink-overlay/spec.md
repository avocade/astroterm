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

### Requirement: No download without consent
The application SHALL NEVER download Starlink data without the user saying yes. Turning the layer on (`x` or
`--starlink`) SHALL show cached data immediately. It SHALL ask "Refresh it now? y/N" only when the cache is older
than 14 days, and "Download it now? y/N" when there is none; any answer other than `y` keeps the cached data (or
turns the layer off when there is none). `U` SHALL ask to refresh at any time. With `--offline` it SHALL never
ask. An accepted download SHALL run in the background, keeping the cached dots on screen and the sky animating,
and SHALL swap in the new data when it validates.

#### Scenario: Cached data shown at once
- **WHEN** the Starlink cache is 3 days old and the user presses `x`
- **THEN** the dots appear immediately and nothing is downloaded or asked

#### Scenario: Old cache
- **WHEN** the Starlink cache is 16 days old and the user presses `x`
- **THEN** the cached dots appear and the prompt asks to refresh; pressing `n` keeps them and downloads nothing

#### Scenario: Slow network
- **WHEN** the user accepts a refresh on a slow connection
- **THEN** the sky keeps animating with the cached dots, and the new data appears when the download completes
