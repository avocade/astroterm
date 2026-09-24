## ADDED Requirements

### Requirement: Red-on-black rendering
The application SHALL draw every glyph in red on black while night vision is on: stars, planets, Moon, labels,
constellation lines and names, grid, cardinal letters, space stations, Starlink dots, metadata panel, help modal
and toasts, including the area outside the square sky window. No other hue SHALL appear.

#### Scenario: Starlink is red, not blue
- **WHEN** night vision and the Starlink overlay are both on
- **THEN** Starlink dots are drawn in dim red

#### Scenario: Light terminal theme
- **WHEN** the terminal's default background is white and night vision is turned on
- **THEN** the whole screen background becomes black

### Requirement: Brightness hierarchy in red
On terminals with 256 or more colors, night vision SHALL use three red intensities: bright for the Sun, Moon,
planets and space stations, medium for stars and labels, dim for grid, constellation lines, Starlink and UI chrome.
On 8/16-color terminals it SHALL use `COLOR_RED` with the `A_DIM` attribute for the dim tier.

#### Scenario: 8-color fallback
- **WHEN** the terminal reports 8 colors
- **THEN** night vision still renders all content in red, with secondary elements dimmed

### Requirement: Independent of the color setting
Night vision SHALL work whether or not colors (`c`, `--color`) are on, and turning it off SHALL restore the
previous color setting exactly.

#### Scenario: Round trip
- **WHEN** colors are off, the user presses `r` twice
- **THEN** the display returns to uncolored rendering

### Requirement: Flag and key
Night vision SHALL be enabled at startup by `--night` (`-n`) and toggled with `r`.

#### Scenario: Start in the dark
- **WHEN** astroterm starts with `--night`
- **THEN** the very first frame is red-on-black

### Requirement: No color support
On terminals without color support, pressing `r` SHALL show the toast "Night vision needs a color terminal" and
leave rendering unchanged.

#### Scenario: Monochrome terminal
- **WHEN** `has_colors()` is false and the user presses `r`
- **THEN** the toast explains why and nothing else changes
